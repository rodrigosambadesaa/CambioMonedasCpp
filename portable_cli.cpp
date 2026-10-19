#include "portable_cli.h"
#include "bigint.h"
#include "algoritmo_cambio.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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
    std::ifstream file(path);
    if (!file) throw std::runtime_error("No se pudo abrir " + path.string());
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

std::string decimal_add(const std::string &a, const std::string &b) {
    OwnedInt left(a), right(b);
    BigInt sum{nullptr};
    if (!bigint_add(&left.value, &right.value, &sum)) throw std::bad_alloc();
    std::string value(sum.digits);
    bigint_free(&sum);
    return value;
}

std::string decimal_multiply(const std::string &a, const std::string &b) {
    OwnedInt left(a), right(b);
    BigInt product{nullptr};
    if (!bigint_multiply(&left.value, &right.value, &product)) throw std::bad_alloc();
    std::string value(product.digits);
    bigint_free(&product);
    return value;
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

std::string csv_escape(const std::string &v) {
    if (v.find_first_of(",\"\r\n") == std::string::npos) return v;
    std::string escaped = "\"";
    for (char c : v) { if (c == '"') escaped += '"'; escaped += c; }
    return escaped + "\"";
}

void csv_row(std::ostream &out, const std::vector<std::string> &row) {
    for (size_t i = 0; i < row.size(); ++i)
        out << (i ? "," : "") << csv_escape(row[i]);
    out << '\n';
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

void export_json(std::ostream &out, const Blocks &denoms, const Blocks &stocks) {
    out << "{\n  \"monedas\": [\n";
    bool first = true;
    for (const auto &[name, d] : denoms) {
        if (!valid(name, denoms, stocks)) continue;
        out << (first ? "" : ",\n") << "    {\"nombre\":\"" << name << "\",\"denominaciones\":[";
        for (size_t i = 0; i < d.size(); ++i) out << (i ? "," : "") << "\"" << d[i] << "\"";
        out << "],\"stock\":[";
        for (size_t i = 0; i < stocks.at(name).size(); ++i)
            out << (i ? "," : "") << stocks.at(name)[i];
        out << "]}";
        first = false;
    }
    out << "\n  ]\n}\n";
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

std::optional<std::string> flag_value(const std::vector<std::string> &args, const std::string &flag) {
    auto it = std::find(args.begin(), args.end(), flag);
    if (it == args.end() || ++it == args.end()) return std::nullopt;
    return *it;
}

bool has(const std::vector<std::string> &args, const std::string &flag) {
    return std::find(args.begin(), args.end(), flag) != args.end();
}

} // namespace

int run_portable_cli(int argc, char **argv) {
    if (argc <= 1) return -1; // interfaz de consola existente
    std::vector<std::string> args(argv + 1, argv + argc);
    if (has(args, "--help") || has(args, "-h")) {
        std::cout << "ProgVoraz C++ --input CSV [--output CSV] [--log LOG] | --stream | --docker\n"
                  << "--lang es|en --export-stock-json FILE --export-report FILE\n"
                  << "--snapshot-stock FILE --restore-stock FILE --convert FROM TO CENTS [--convert-stock]\n"
                  << "--help --version\n";
        return 0;
    }
    if (has(args, "--version") || has(args, "-v")) {
        std::cout << "ProgVoraz C++20 (migration)\n";
        return 0;
    }
    try {
        if (has(args, "--restore-stock")) {
            std::string path = flag_value(args, "--restore-stock").value_or("");
            if (path.empty()) throw std::runtime_error("Falta ruta de snapshot");
            Blocks denom = read_blocks("monedas.txt"), candidate = read_blocks(path, true);
            for (const auto &[key, values] : denom)
                if (!valid(key, denom, candidate)) throw std::runtime_error("Snapshot inconsistente: " + key);
            fs::path temp = "stock.restore.tmp";
            fs::copy_file(path, temp, fs::copy_options::overwrite_existing);
            try {
                fs::rename(temp, "stock.txt");
            } catch (...) { fs::remove(temp); throw; }
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
        if (has(args, "--export-stock-json") || has(args, "--export-report")) {
            auto json = flag_value(args, "--export-stock-json");
            auto report = flag_value(args, "--export-report");
            std::string path = json.value_or(report.value_or(""));
            if (path.empty()) throw std::runtime_error("Falta destino de exportacion");
            std::ofstream out(path);
            if (!out) throw std::runtime_error("No se pudo abrir " + path);
            if (json) export_json(out, denoms, stocks);
            else export_report(out, denoms, stocks);
            if (!out) throw std::runtime_error("Error escribiendo " + path);
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
            if (from != to) throw std::runtime_error("Sin proveedor de tasas verificado; conversion cruzada deshabilitada");
            auto outcome = calculate(args[index + 3], to, has(args, "--convert-stock"),
                                     {}, {}, denoms, stocks);
            if (!outcome) throw std::runtime_error("Cambio exacto no disponible");
            std::cout << "Origen=" << args[index + 3] << " Destino=" << args[index + 3] << " Tasa=1\n";
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
