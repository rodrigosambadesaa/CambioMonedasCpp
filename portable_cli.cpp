
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif
#include <limits>
#include <regex>

#include "portable_cli.h"
#include "csv_io.h"
#include "json_io.h"
#include "gui_launcher.h"
#include "exchange_api.h"
#include "bigint.h"
#include "algoritmo_cambio.h"
#include "moneda_gestion.h"
#include <algorithm>
#include <cctype>
#include <deque>
#include <filesystem>
#ifdef _WIN32
#include <windows.h>
#else
#include <spawn.h>
#include <system_error>
extern char **environ;
#endif
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;
namespace {

using Blocks = std::map<std::string, std::vector<std::string>>;

bool numeric(const std::string &s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c){return std::isdigit(c) != 0;});
}

std::string lowercase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return s;
}

std::string currency(std::string name) {
    name = lowercase(name);
    std::replace(name.begin(), name.end(), '-', '_');
    std::replace(name.begin(), name.end(), ' ', '_');
    if (name == "eur" || name == "euros") return "euro";
    if (name == "usd" || name == "dollar" || name == "dolares") return "dolar";
    if (name == "jpy" || name == "yenes") return "yen";
    if (name == "gbp" || name == "libras") return "libra";
    if (name == "chf") return "franco_suizo";
    if (name == "cad") return "dolar_canadiense";
    if (name == "aud") return "dolar_australiano";
    if (name == "cny" || name == "rmb") return "yuan_chino";
    if (name == "mxn") return "peso_mexicano";
    return name;
}

Blocks read_blocks(const fs::path &path, bool stock = false) {
    std::ifstream file;
    // Win32 may briefly reject a read while another process atomically
    // replaces stock.txt. A bounded read-open retry prevents a false
    // currency/inventory failure. Do not retry the financial transaction.
    for (int attempt=0; attempt<1000; ++attempt) {
        file.open(path);
        if (file.is_open()) break;
        file.clear();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!file.is_open())
        throw std::runtime_error("No se pudo abrir " + path.string());
    Blocks result;
    std::string current, token;
    while (file >> token) {
        if (numeric(token) || stock && token == "-1") {
            if (current.empty()) throw std::runtime_error("Formato de archivo invalido");
            result[current].push_back(token == "-1" ? "0" : token);
        } else {
            current = currency(token);
            if (result.contains(current)) throw std::runtime_error("Bloque de moneda duplicado");
            result[current] = {};
        }
    }
    return result;
}

bool less_decimal(std::string a, std::string b) {
    a.erase(0, std::min(a.find_first_not_of('0'), a.size() - 1));
    b.erase(0, std::min(b.find_first_not_of('0'), b.size() - 1));
    return a.size() != b.size() ? a.size() < b.size() : a < b;
}

bool valid(const std::string &key, const Blocks &denoms, const Blocks &stocks) {
    auto d = denoms.find(key), s = stocks.find(key);
    if (d == denoms.end() || s == stocks.end() || d->second.empty() || d->second.size() != s->second.size())
        return false;
    const auto &values = d->second;
    for (size_t i = 0; i < values.size(); ++i) {
        if (!numeric(values[i]) || values[i] == "0" || !numeric(s->second[i])) return false;
        if (i && !less_decimal(values[i], values[i - 1])) return false;
    }
    return true;
}

struct OwnedInt {
    BigInt value{nullptr};
    explicit OwnedInt(const std::string &text) {
        if (!bigint_init(&value, text.c_str())) throw std::runtime_error("BigInt invalido: " + text);
    }
    ~OwnedInt() { bigint_free(&value); }
    OwnedInt(const OwnedInt &) = delete;
    OwnedInt &operator=(const OwnedInt &) = delete;
};

struct OwnedArray {
    BigIntArray value{};
    explicit OwnedArray(const std::vector<std::string> &numbers) {
        if (!bigint_array_create(&value, numbers.size())) throw std::bad_alloc();
        try {
            for (size_t i = 0; i < numbers.size(); ++i) {
                OwnedInt number(numbers[i]);
                if (!bigint_array_set(&value, i, &number.value)) throw std::bad_alloc();
            }
        } catch (...) { bigint_array_free(&value); throw; }
    }
    OwnedArray() = default;
    ~OwnedArray() { bigint_array_free(&value); }
    OwnedArray(const OwnedArray &) = delete;
    OwnedArray &operator=(const OwnedArray &) = delete;
};

/* Scope-bound BigInt ownership also covers exceptions from std::string.
   Manual bigint_free after constructing the std::string is not exception-safe. */
struct OwnedResult {
    BigInt value{};
    ~OwnedResult() { bigint_free(&value); }
    OwnedResult(const OwnedResult &) = delete;
    OwnedResult &operator=(const OwnedResult &) = delete;
    OwnedResult() = default;
};

std::string decimal_add(const std::string &a, const std::string &b) {
    OwnedInt left(a), right(b);
    OwnedResult sum;
    if (!bigint_add(&left.value, &right.value, &sum.value)) throw std::bad_alloc();
    return std::string(sum.value.digits);
}

std::string decimal_multiply(const std::string &a, const std::string &b) {
    OwnedInt left(a), right(b);
    OwnedResult product;
    if (!bigint_multiply(&left.value, &right.value, &product.value)) throw std::bad_alloc();
    return std::string(product.value.digits);
}

struct Outcome {
    std::vector<std::string> pieces;
    std::string total;
};

std::optional<Outcome> calculate(const std::string &amount, const std::string &key,
                                 bool use_stock, std::optional<size_t> min,
                                 std::optional<size_t> max, const Blocks &denoms, const Blocks &stocks) {
    if (!numeric(amount) || !valid(key, denoms, stocks)) return std::nullopt;
    OwnedInt requested(amount);
    OwnedArray ds(denoms.at(key));
    std::unique_ptr<OwnedArray> st;
    if (use_stock) st = std::make_unique<OwnedArray>(stocks.at(key));
    OwnedArray result;
    int found = 0;
    if (use_stock) {
        found = max ? calcular_cambio_optimo_stock_con_rango(&requested.value, &ds.value, &st->value,
                    min.value_or(0), *max, &result.value)
                    : calcular_cambio_optimo_stock(&requested.value, &ds.value, &st->value, &result.value);
    } else {
        found = max ? calcular_cambio_optimo_con_rango(&requested.value, &ds.value, min.value_or(0),
                    *max, &result.value)
                    : calcular_cambio_optimo(&requested.value, &ds.value, &result.value);
    }
    if (!found) return std::nullopt;
    Outcome output;
    output.total = "0";
    for (size_t i = 0; i < result.value.len; ++i) {
        std::string number(result.value.items[i].digits);
        output.pieces.push_back(number);
        output.total = decimal_add(output.total, number);
    }
    return output;
}

std::string trim(std::string input) {
    const auto start = input.find_first_not_of(" \r\n\t");
    if (start == std::string::npos) return "";
    const auto end = input.find_last_not_of(" \r\n\t");
    return input.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string &line, char delim) {
    std::vector<std::string> result;
    std::istringstream stream(line);
    std::string item;
    while (std::getline(stream, item, delim)) result.push_back(trim(item));
    return result;
}

bool nonnegative_size(const std::string &s, size_t &value) {
    if (!numeric(s)) return false;
    try {
        size_t used = 0;
        unsigned long long n = std::stoull(s, &used);
        if (used != s.size() || n > std::numeric_limits<size_t>::max()) return false;
        value = static_cast<size_t>(n);
        return true;
    } catch (...) { return false; }
}

void process_batch(std::istream &in, std::ostream &out, std::ostream *log,
                   bool english, const Blocks &denoms, const Blocks &stocks) {
    csv_row(out, english ? std::vector<std::string>{"mode","currency","amount","result","note"}
                        : std::vector<std::string>{"modo","moneda","monto","resultado","nota"});
    std::string line;
    size_t line_number = 0;
    while (std::getline(in, line)) {
        ++line_number;
        auto row = split(line, ',');
        if (row.empty() || trim(line).empty()) continue;
        if (line_number == 1 && (lowercase(row[0]) == "mode" || lowercase(row[0]) == "modo")) continue;
        auto field = [&](size_t index, const std::string &fallback) {
            return index < row.size() ? row[index] : fallback;
        };
        std::string mode = lowercase(field(0, "a")), key = currency(field(1, ""));
        std::string amount = field(2, "0");
        if (!numeric(amount)) {
            csv_row(out, {"", "", amount, "ERROR", english ? "invalid amount" : "monto invalido"});
            if (log) *log << "invalid amount at line " << line_number << '\n';
            continue;
        }
        if (!valid(key, denoms, stocks)) {
            csv_row(out, {"", "", amount, "ERROR", english ? "currency not found" : "moneda no encontrada"});
            if (log) *log << "currency not found: " << key << '\n';
            continue;
        }
        std::optional<size_t> min, max;
        std::string range = field(3, "");
        if (!range.empty()) {
            size_t dash = range.find('-'), value = 0;
            std::string first = range.substr(0, dash);
            if (!first.empty() && first[0] == '=') first.erase(first.begin());
            if (!nonnegative_size(first, value)) {
                csv_row(out, {"", key, amount, "ERROR", "rango invalido"});
                continue;
            }
            if (dash != std::string::npos) {
                size_t upper = 0;
                if (!nonnegative_size(range.substr(dash + 1), upper) || value > upper) {
                    csv_row(out, {"", key, amount, "ERROR", "rango invalido"});
                    continue;
                }
                min = value; max = upper;
            } else {
                max = value;
                if (range[0] == '=') min = value;
            }
        }
        auto result = calculate(amount, key, mode == "b" || mode == "c", min, max, denoms, stocks);
        if (!result) {
            csv_row(out, {"", key, amount, "NO", english ? "no solution" : "sin solucion"});
            continue;
        }
        std::string details;
        for (size_t i = 0; i < result->pieces.size() && i < 8; ++i)
            details += denoms.at(key)[i] + ":" + result->pieces[i] + ";";
        csv_row(out, {"", key, amount, "OK total=" + result->total, details});
    }
}

void export_report(std::ostream &out, const Blocks &denoms, const Blocks &stocks) {
    out << "REPORTE GLOBAL DE INVENTARIO\n";
    for (const auto &[name, d] : denoms) {
        if (!valid(name, denoms, stocks)) continue;
        std::string sum = "0", total_value = "0";
        for (size_t i = 0; i < d.size(); ++i) {
            sum = decimal_add(sum, stocks.at(name)[i]);
            total_value = decimal_add(total_value, decimal_multiply(d[i], stocks.at(name)[i]));
        }
        out << name << " | denominaciones=" << d.size() << " | piezas=" << sum
            << " | valor_centimos=" << total_value << '\n';
    }
}


std::string iso_code(const std::string &name) {
    const std::string key = currency(name);
    static const std::map<std::string,std::string> codes {
        {"euro","EUR"},{"dolar","USD"},{"yen","JPY"},
        {"libra","GBP"},{"franco_suizo","CHF"},{"dolar_canadiense","CAD"},
        {"dolar_australiano","AUD"},{"yuan_chino","CNY"},{"peso_mexicano","MXN"}
    };
    if (auto it=codes.find(key); it!=codes.end()) return it->second;
    if (key.size()==3 && std::all_of(key.begin(),key.end(),
         [](unsigned char c){return c >= 'a' && c <= 'z';})) {
        std::string result=key;
        std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
        return result;
    }
    throw std::invalid_argument("Moneda ISO desconocida: " + name);
}

struct DecimalRate { std::string numerator, denominator, printable; };


DecimalRate parse_decimal_rate(std::string digits) {
    std::string printable=digits;
    std::size_t dot=digits.find('.');
    std::size_t scale=(dot==std::string::npos) ? 0 : digits.size()-dot-1;
    if (scale > 30 || digits.size() > 100) throw std::runtime_error("Tasa demasiado precisa");
    if (dot!=std::string::npos) digits.erase(dot,1);
    if (!numeric(digits) || std::all_of(digits.begin(),digits.end(),[](char c){return c=='0';}))
        throw std::runtime_error("Tasa cero o invalida");
    return {digits, "1"+std::string(scale,'0'), printable};
}
DecimalRate read_stub_rate(const std::string &from, const std::string &to) {
    if (from==to) return {"1","1","1"};
    std::ifstream input("rates_stub.json", std::ios::binary);
    if (!input) throw std::runtime_error("No existe rates_stub.json");
    /* Bound memory BEFORE constructing the document, unlike an unbounded
       istreambuf_iterator read followed by checking the result's size. */
    char buffer[16385];
    input.read(buffer, sizeof(buffer));
    std::streamsize count = input.gcount();
    if (count > 16384) throw std::runtime_error("Archivo de tasas demasiado grande");
    std::string document(buffer, static_cast<std::size_t>(count));
    const std::regex keyPattern("\"" + from + "_" + to + "\"\\s*:");
    auto found = std::sregex_iterator(document.begin(), document.end(), keyPattern);
    if (found == std::sregex_iterator())
        throw std::runtime_error("No existe tasa para " + from + "_" + to);
    if (++found != std::sregex_iterator())
        throw std::runtime_error("Tasa duplicada para " + from + "_" + to);
    const std::regex pattern("\"" + from + "_" + to + "\"\\s*:\\s*([0-9]+(?:\\.[0-9]+)?)");
    std::smatch matches;
    if (!std::regex_search(document, matches, pattern))
        throw std::runtime_error("Tasa invalida para " + from + "_" + to);
    return parse_decimal_rate(matches[1].str());
}

std::string convert_cents(const std::string &amount, const DecimalRate &rate) {
    if (!numeric(amount)) throw std::invalid_argument("Importe invalido");
    OwnedInt value(amount), numerator(rate.numerator), denominator(rate.denominator);
    OwnedInt two("2"), one("1"), product("0"), quotient("0"), remainder("0"), doubled("0");
    if (!bigint_multiply(&value.value,&numerator.value,&product.value) ||
        !bigint_divmod(&product.value,&denominator.value,&quotient.value,&remainder.value) ||
        !bigint_multiply(&remainder.value,&two.value,&doubled.value))
        throw std::runtime_error("Error de aritmetica BigInt");
    if (bigint_compare(&doubled.value,&denominator.value)>=0) {
        BigInt rounded{nullptr};
        if (!bigint_add(&quotient.value,&one.value,&rounded))
            throw std::runtime_error("Error redondeando BigInt");
        bigint_free(&quotient.value);
        quotient.value=rounded;
    }
    return quotient.value.digits;
}

std::optional<std::string> flag_value(const std::vector<std::string> &args, const std::string &flag) {
    auto it = std::find(args.begin(), args.end(), flag);
    if (it == args.end() || ++it == args.end()) return std::nullopt;
    return *it;
}

bool has(const std::vector<std::string> &args, const std::string &flag) {
    return std::find(args.begin(), args.end(), flag) != args.end();
}


#ifdef _WIN32
using HttpSocket = SOCKET;
const HttpSocket InvalidHttpSocket = INVALID_SOCKET;
void close_http(HttpSocket s) { closesocket(s); }
#else
using HttpSocket = int;
const HttpSocket InvalidHttpSocket = -1;
void close_http(HttpSocket s) { close(s); }
#endif

/* Resource ownership stays correct if an error response throws. */
struct HttpSocketGuard {
    HttpSocket handle;
    explicit HttpSocketGuard(HttpSocket h) : handle(h) {}
    ~HttpSocketGuard() { if (handle != InvalidHttpSocket) close_http(handle); }
    HttpSocketGuard(const HttpSocketGuard &) = delete;
    HttpSocketGuard &operator=(const HttpSocketGuard &) = delete;
};

void send_http(HttpSocket client, int code, const std::string &reason,
               const std::string &mime, const std::string &content) {
    std::string header = "HTTP/1.1 " + std::to_string(code) + " " + reason + "\r\n"
        "Content-Type: " + mime + "\r\n"
        "Content-Length: " + std::to_string(content.size()) + "\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "Cache-Control: no-store\r\n"
        "Connection: close\r\n\r\n";
    std::string response = header + content;
    std::size_t sent = 0;
    while (sent < response.size()) {
        auto size = static_cast<int>(std::min<std::size_t>(response.size() - sent, 8192));
        /* A peer can abort between recv and send. MSG_NOSIGNAL prevents
           a SIGPIPE from terminating the entire Unix server. */
#ifdef MSG_NOSIGNAL
        int n = send(client, response.data() + sent, size, MSG_NOSIGNAL);
#else
        int n = send(client, response.data() + sent, size, 0);
#endif
        if (n <= 0) break;
        sent += static_cast<std::size_t>(n);
    }
}

void handle_http(HttpSocket client) {
    constexpr std::size_t kMaxBody = 1024 * 1024;
    constexpr std::size_t kMaxHeaders = 32768;
    std::string request;
    char buffer[4096];
    std::size_t header_end = std::string::npos;
    while ((header_end = request.find("\r\n\r\n")) == std::string::npos) {
        if (request.size() > kMaxHeaders) {
            send_http(client, 431, "Request Header Fields Too Large", "text/plain", "Headers Too Large");
            return;
        }
        int n = recv(client, buffer, sizeof(buffer), 0);
        if (n <= 0) return;
        request.append(buffer, static_cast<std::size_t>(n));
    }
    std::istringstream input(request.substr(0, header_end + 2));
    std::string method, path, protocol, first_line;
    if (!std::getline(input, first_line)) return;
    {
        std::istringstream first(first_line);
        first >> method >> path >> protocol;
    }
    std::string line;
    std::optional<std::size_t> content_length;
    while (std::getline(input, line)) {
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string name = lowercase(trim(line.substr(0, colon)));
        if (name == "content-length") {
            std::size_t parsed = 0;
            if (!nonnegative_size(trim(line.substr(colon + 1)), parsed)) {
                send_http(client, 400, "Bad Request", "text/plain", "Invalid Content-Length");
                return;
            }
            content_length = parsed;
        }
    }
    if (method == "POST" && path == "/batch") {
        if (!content_length) {
            send_http(client, 411, "Length Required", "text/plain", "Length Required"); return;
        }
        if (*content_length > kMaxBody) {
            send_http(client, 413, "Payload Too Large", "text/plain", "Payload Too Large"); return;
        }
        std::size_t start = header_end + 4;
        while (request.size() < start + *content_length) {
            int n = recv(client, buffer, sizeof(buffer), 0);
            if (n <= 0) return;
            request.append(buffer, static_cast<std::size_t>(n));
        }
        std::istringstream csv(request.substr(start, *content_length));
        std::ostringstream out;
        Blocks denominations = read_blocks("monedas.txt");
        Blocks stocks = read_blocks("stock.txt", true);
        process_batch(csv, out, nullptr, false, denominations, stocks);
        send_http(client, 200, "OK", "text/csv; charset=utf-8", out.str());
        return;
    }
    if (method == "GET" && path == "/health") {
        send_http(client, 200, "OK", "text/plain; charset=utf-8", "OK"); return;
    }
    if (method == "GET" && path == "/export_stock_json") {
        Blocks denominations = read_blocks("monedas.txt");
        Blocks stocks = read_blocks("stock.txt", true);
        std::ostringstream out;
        export_stock_json_cpp(out, denominations, stocks, [&](const std::string &key) { return valid(key, denominations, stocks); });
        send_http(client, 200, "OK", "application/json; charset=utf-8", out.str());
        return;
    }
    send_http(client, 404, "Not Found", "text/plain", "Not Found");
}

int serve_local_http(int port) {
    if (port < 1 || port > 65535) throw std::invalid_argument("Puerto HTTP invalido");
    /* A finite lifetime enables accurate leak/FD checks under Valgrind.
       Production remains unlimited when the variable is not set. */
    std::size_t max_requests = 0;
    if (const char *limit = std::getenv("PROGVORAZ_HTTP_MAX_REQUESTS")) {
        if (!nonnegative_size(limit, max_requests) || max_requests == 0)
            throw std::invalid_argument("Limite HTTP de pruebas invalido");
    }
#ifdef _WIN32
    WSADATA winsock{};
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
        throw std::runtime_error("Winsock no disponible");
    struct WinsockGuard { ~WinsockGuard() { WSACleanup(); } } winsock_guard;
#endif
    HttpSocket listening = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listening == InvalidHttpSocket)
        throw std::runtime_error("No se puede abrir socket HTTP");
    HttpSocketGuard listening_guard(listening);
#ifndef _WIN32
    int reuse = 1;
    setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif
    sockaddr_in bind_address{};
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons(static_cast<unsigned short>(port));
    bind_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 ONLY
    if (bind(listening, reinterpret_cast<sockaddr*>(&bind_address), sizeof(bind_address)) != 0 ||
        listen(listening, 16) != 0)
        throw std::runtime_error("No se pudo escuchar en 127.0.0.1");
    std::cout << "Servidor HTTP local 127.0.0.1:" << port << std::endl;
    std::size_t served = 0;
    for (;;) {
        if (max_requests && served >= max_requests) break;
        HttpSocket client = accept(listening, nullptr, nullptr);
        if (client == InvalidHttpSocket) continue;
        HttpSocketGuard client_guard(client);
        // A client sending one header byte or an incomplete POST must not
        // monopolize this sequential listener indefinitely. Time out both
        // reads and writes; dropped requests still release the RAII socket.
        const std::size_t timeout_ms = [] {
            const char *raw = std::getenv("PROGVORAZ_HTTP_IO_TIMEOUT_MS");
            std::size_t parsed = 0;
            return raw && nonnegative_size(raw, parsed) && parsed >= 100 &&
                   parsed <= 60000 ? parsed : static_cast<std::size_t>(15000);
        }();
#ifdef _WIN32
        DWORD io_timeout = static_cast<DWORD>(timeout_ms);
        int recv_timeout = setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char *>(&io_timeout), sizeof(io_timeout));
        int send_timeout = setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
            reinterpret_cast<const char *>(&io_timeout), sizeof(io_timeout));
#else
        timeval io_timeout{};
        io_timeout.tv_sec = static_cast<decltype(io_timeout.tv_sec)>(timeout_ms / 1000);
        io_timeout.tv_usec = static_cast<decltype(io_timeout.tv_usec)>((timeout_ms % 1000) * 1000);
        int recv_timeout = setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
            &io_timeout, sizeof(io_timeout));
        int send_timeout = setsockopt(client, SOL_SOCKET, SO_SNDTIMEO,
            &io_timeout, sizeof(io_timeout));
#endif
        if (recv_timeout != 0 || send_timeout != 0) {
            std::cerr << "HTTP: no se pudo establecer timeout de cliente\n";
            continue;
        }
#ifdef SO_NOSIGPIPE
        int no_sigpipe = 1;
        (void)setsockopt(client, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
        try { handle_http(client); }
        catch (const std::exception &ex) {
            std::cerr << "HTTP: " << ex.what() << '\n';
            try { send_http(client, 500, "Internal Server Error", "text/plain", "Internal Error"); }
            catch (const std::exception &) { /* Client dropped while replying. */ }
        }
        ++served;
    }
    return 0;
}

} // namespace


int run_portable_cli(int argc, char **argv) {
    if (argc <= 1) return -1; // interfaz de consola existente
    std::vector<std::string> args(argv + 1, argv + argc);
    if (std::find(args.begin(), args.end(), "--json") != args.end()) {
#ifdef _WIN32
        if (_putenv_s("PROGVORAZ_JSON", "1") != 0)
            throw std::runtime_error("No se pudo activar el renderer JSON");
#else
        if (setenv("PROGVORAZ_JSON", "1", 1) != 0)
            throw std::runtime_error("No se pudo activar el renderer JSON");
#endif
        if (args.size() == 1) return -1; // continuar con el menu interactivo
    }

    if (has(args, "--help") || has(args, "-h")) {
        std::cout << "ProgVoraz C++ --input CSV [--output CSV] [--log LOG] | --stream | --docker\n"
                  << "--lang es|en --json (menu) --export-stock-json FILE --export-report FILE\n"
                  << "--snapshot-stock FILE --restore-stock FILE --convert FROM TO CENTS [--convert-stock]\n"
                  << "--specific CURRENCY RECEIVED_CSV RETURNED_CSV [--specific-stock]\n"
                  << "--history [--tail N] --summary CURRENCY\n"
                  << "--gui (lanzamiento nativo, sin shell) --server [--port N] --help --version\n";
        return 0;
    }
    if (has(args, "--version") || has(args, "-v")) {
        std::cout << "ProgVoraz C++20 (migration)\n";
        return 0;
    }
    try {
        if (has(args, "--gui") || has(args, "gui")) return launch_native_gui(argv[0]);
        if (has(args, "--server")) {
            std::string portText = flag_value(args, "--port").value_or("8080");
            std::size_t port = 0;
            if (!nonnegative_size(portText, port) || port == 0 || port > 65535)
                throw std::runtime_error("Puerto HTTP invalido");
            return serve_local_http(static_cast<int>(port));
        }
        if (has(args, "--restore-stock")) {
            auto path=flag_value(args,"--restore-stock");
            if (!path || path->empty()) throw std::invalid_argument("Falta ruta de snapshot");
            if (!restaurar_snapshot_stock_transaccional(path->c_str()))
                throw std::runtime_error("Snapshot no válido o error al restaurar inventario");
            return 0;
        }
        if (has(args, "--snapshot-stock")) {
            auto path = flag_value(args, "--snapshot-stock");
            if (!path || path->empty()) throw std::runtime_error("Falta destino snapshot");
            fs::copy_file("stock.txt", *path, fs::copy_options::overwrite_existing);
            return 0;
        }

        Blocks denoms = read_blocks("monedas.txt");
        Blocks stocks = read_blocks("stock.txt", true);
        if (has(args,"--history")) {
            std::size_t limit=100;
            if (auto value=flag_value(args,"--tail")) {
                if (!nonnegative_size(*value,limit) || limit<1 || limit>1000)
                    throw std::invalid_argument("Limite de historial permitido: 1-1000");
            }
            std::ifstream history("historial.txt");
            std::deque<std::string> recent;
            std::string line;
            while (std::getline(history,line)) {
                recent.push_back(line);
                if (recent.size()>limit) recent.pop_front();
            }
            for(const auto &entry:recent) std::cout<<entry<<'\n';
            return 0;
        }
        if (has(args,"--summary")) {
            auto value=flag_value(args,"--summary");
            if (!value) throw std::invalid_argument("Indique divisa para --summary");
            std::string key=currency(*value);
            if (!valid(key,denoms,stocks))
                throw std::invalid_argument("Moneda inexistente o inventario inconsistente");
            const auto &d=denoms.at(key),&s=stocks.at(key);
            std::string pieces="0",total="0";
            for(size_t i=0;i<d.size();++i) {
                pieces=decimal_add(pieces,s[i]);
                total=decimal_add(total,decimal_multiply(d[i],s[i]));
            }
            std::cout<<"Moneda="<<key<<" Denominaciones="<<d.size()
                     <<" Min="<<d.back()<<" Max="<<d.front()
                     <<" Piezas="<<pieces<<" ValorCentimos="<<total<<'\n';
            return 0;
        }

        if (has(args, "--export-stock-json") || has(args, "--export-report")) {
            auto json = flag_value(args, "--export-stock-json");
            auto report = flag_value(args, "--export-report");
            std::string path = json.value_or(report.value_or(""));
            if (path.empty()) throw std::runtime_error("Falta destino de exportacion");
            std::ofstream out(path);
            if (!out) throw std::runtime_error("No se pudo abrir " + path);
            if (json) export_stock_json_cpp(out, denoms, stocks, [&](const std::string &key) { return valid(key, denoms, stocks); });
            else export_report(out, denoms, stocks);
            if (!out) throw std::runtime_error("Error escribiendo " + path);
            return 0;
        }

        auto specific=std::find(args.begin(),args.end(),"--specific");
        if (specific!=args.end()) {
            size_t pos=static_cast<size_t>(specific-args.begin());
            if (pos+3>=args.size())
                throw std::invalid_argument("Se requiere divisa y dos listas de cantidades");
            std::string key=currency(args[pos+1]);
            if (!valid(key,denoms,stocks))
                throw std::invalid_argument("Moneda desconocida o inconsistente");
            auto incoming=split(args[pos+2],',');
            auto outgoing=split(args[pos+3],',');
            const auto &values=denoms.at(key);
            if (incoming.size()!=values.size() || outgoing.size()!=values.size())
                throw std::invalid_argument("Numero incorrecto de denominaciones");
            if (!std::all_of(incoming.begin(),incoming.end(),numeric) ||
                !std::all_of(outgoing.begin(),outgoing.end(),numeric))
                throw std::invalid_argument("Cantidades de monedas invalidas");
            std::string in_value="0",out_value="0";
            for (size_t i=0;i<values.size();++i) {
                in_value=decimal_add(in_value,decimal_multiply(values[i],incoming[i]));
                out_value=decimal_add(out_value,decimal_multiply(values[i],outgoing[i]));
            }
            if (in_value!=out_value)
                throw std::runtime_error("Importes entregados y devueltos distintos");
            std::vector<std::string> balance=stocks.at(key);
            if (has(args,"--specific-stock")) {
                OwnedArray incoming_bi(incoming),outgoing_bi(outgoing);
                BigIntArray updated{};
                if (!intercambiar_stock_moneda_transaccional(
                    key.c_str(),&incoming_bi.value,&outgoing_bi.value,&updated))
                    throw std::runtime_error("Inventario insuficiente o error de persistencia");
                balance.clear();
                for (size_t i=0;i<updated.len;++i)
                    balance.emplace_back(updated.items[i].digits);
                bigint_array_free(&updated);
            }
            std::cout<<"OK moneda="<<key<<" total_centimos="<<in_value<<'\n';
            for(size_t i=0;i<values.size();++i)
                std::cout<<values[i]<<" recibido="<<incoming[i]
                         <<" entregado="<<outgoing[i]<<" stock="<<balance[i]<<'\n';
            return 0;
        }

        auto input = flag_value(args, "--input");
        if (input || has(args, "--stream") || has(args, "--docker")) {
            std::unique_ptr<std::ifstream> in;
            std::unique_ptr<std::ofstream> out, log;
            if (input) {
                in = std::make_unique<std::ifstream>(*input);
                if (!*in) throw std::runtime_error("No se pudo leer " + *input);
            }
            if (auto path = flag_value(args, "--output")) {
                out = std::make_unique<std::ofstream>(*path);
                if (!*out) throw std::runtime_error("No se pudo escribir " + *path);
            }
            if (auto path = flag_value(args, "--log")) {
                log = std::make_unique<std::ofstream>(*path, std::ios::app);
                if (!*log) throw std::runtime_error("No se pudo escribir log");
            }
            const bool english = flag_value(args, "--lang").value_or("") == "en";
            process_batch(in ? static_cast<std::istream&>(*in) : std::cin,
                          out ? static_cast<std::ostream&>(*out) : std::cout, log.get(),
                          english, denoms, stocks);
            return 0;
        }
        auto it = std::find(args.begin(), args.end(), "--convert");
        if (it != args.end()) {
            size_t index = static_cast<size_t>(it - args.begin());
            if (index + 3 >= args.size()) throw std::runtime_error("Faltan argumentos --convert");
            std::string from = currency(args[index + 1]), to = currency(args[index + 2]);
            const std::string from_iso=iso_code(from), to_iso=iso_code(to);
            const char *configured=std::getenv("PROGVORAZ_EXCHANGE_SOURCE");
            const bool force_stub=configured && std::string(configured)=="stub";
            auto remote=force_stub ? std::optional<std::string>() :
                                  fetch_reference_rate(from_iso, to_iso);
            DecimalRate rate=remote ? parse_decimal_rate(*remote) :
                                     read_stub_rate(from_iso,to_iso);
            std::string destination=convert_cents(args[index + 3],rate);
            auto outcome = calculate(destination, to, has(args, "--convert-stock"),
                                     {}, {}, denoms, stocks);
            if (!outcome) throw std::runtime_error("Cambio exacto no disponible");
            std::cout << "Origen=" << args[index + 3] << " Destino=" << destination << " Tasa=" << rate.printable << "\n";
            for (size_t i = 0; i < outcome->pieces.size(); ++i)
                std::cout << denoms.at(to)[i] << ":" << outcome->pieces[i] << '\n';
            return 0;
        }
        throw std::runtime_error("Opcion no implementada. Consulte --help.");
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 2;
    }
}
