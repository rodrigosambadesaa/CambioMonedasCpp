#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif
#include "moneda_gestion.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <functional>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#endif


#define MAX_TOKEN 256
#define MAX_DENOMINACIONES 64
#define MAX_LINEA_STOCK 256
#define MAX_LINEAS_STOCK 512

static int es_texto_bigint_valido(const char *texto)
{
    size_t i;

    if (texto == NULL || *texto == '\0')
        return 0;

    for (i = 0; texto[i] != '\0'; i++)
    {
        if (texto[i] < '0' || texto[i] > '9')
            return 0;
    }

    return 1;
}

static char *duplicar_cadena(const char *texto)
{
    size_t len;
    char *copia;

    if (texto == NULL)
        return NULL;

    len = strlen(texto) + 1;
    copia = (char *)malloc(len);
    if (copia == NULL)
        return NULL;

    memcpy(copia, texto, len);
    return copia;
}

static void recortar_fin_linea(char *texto)
{
    if (texto == NULL)
        return;

    texto[strcspn(texto, "\r\n")] = '\0';
}

static int cargar_datos_moneda(const char *archivo, const char *nombreMoneda, int convertirMenosUnoACero, BigIntArray *resultado)
{
    FILE *fp = fopen(archivo, "r");
    char token[MAX_TOKEN];
    char valores[MAX_DENOMINACIONES][MAX_TOKEN];
    size_t cantidad = 0;
    int enSeccion = 0;

    if (fp == NULL)
        return 0;

    while (fscanf(fp, "%255s", token) == 1)
    {
        if (!enSeccion)
        {
            if (strcmp(token, nombreMoneda) == 0)
                enSeccion = 1;
            continue;
        }

        if (convertirMenosUnoACero && strcmp(token, "-1") == 0)
            strcpy(token, "0");

        if (!es_texto_bigint_valido(token))
            break;

        if (cantidad >= MAX_DENOMINACIONES)
            break;

        strcpy(valores[cantidad], token);
        cantidad++;
    }

    fclose(fp);

    if (!enSeccion || cantidad == 0)
        return 0;

    if (!bigint_array_create(resultado, cantidad))
        return 0;

    for (size_t i = 0; i < cantidad; i++)
    {
        BigInt tmp = {0};
        if (!bigint_init(&tmp, valores[i]))
        {
            bigint_array_free(resultado);
            return 0;
        }
        if (!bigint_array_set(resultado, i, &tmp))
        {
            bigint_free(&tmp);
            bigint_array_free(resultado);
            return 0;
        }
        bigint_free(&tmp);
    }

    return 1;
}

int cargar_denominaciones_moneda(const char *nombreMoneda, BigIntArray *resultado)
{
    return cargar_datos_moneda("monedas.txt", nombreMoneda, 0, resultado);
}

int cargar_stock_moneda(const char *nombreMoneda, BigIntArray *resultado)
{
    return cargar_datos_moneda("stock.txt", nombreMoneda, 1, resultado);
}

namespace {
namespace fs = std::filesystem;

bool digits_only(const std::string &text) {
    return !text.empty() && std::all_of(text.begin(),text.end(),
        [](unsigned char c){return c>='0' && c<='9';});
}
class StockFileLock {
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif
public:
    StockFileLock() {
        for (int i=0; i<200; ++i) {
#ifdef _WIN32
            handle_=CreateFileA("stock.txt.lock",GENERIC_READ|GENERIC_WRITE,0,nullptr,
                               OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
            if (handle_!=INVALID_HANDLE_VALUE) return;
#else
            if (fd_<0) fd_=open("stock.txt.lock",O_CREAT|O_RDWR,0600);
            if (fd_>=0 && flock(fd_,LOCK_EX|LOCK_NB)==0) return;
#endif
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("No se pudo bloquear stock.txt");
    }
    StockFileLock(const StockFileLock&)=delete;
    StockFileLock& operator=(const StockFileLock&)=delete;
    ~StockFileLock() {
#ifdef _WIN32
        if (handle_!=INVALID_HANDLE_VALUE) CloseHandle(handle_);
#else
        if (fd_>=0) {flock(fd_,LOCK_UN);close(fd_);}
#endif
    }
};

bool replace_atomic(const std::vector<std::string> &lines) {
    fs::path staging = ".stock-tx-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";
    try {
        {
            std::ofstream out(staging,std::ios::binary|std::ios::trunc);
            if (!out) return false;
            for (const auto &line:lines) out << line << '\n';
            out.flush();
            if (!out) throw std::runtime_error("No se pudo escribir staging");
        }
#ifdef _WIN32
        // A reader outside the writers' lock can momentarily open stock.txt
        // without FILE_SHARE_DELETE. Keep the global inventory lock held and
        // retry ONLY the atomic replacement, never the entire transaction.
        bool replaced=false;
        for (int attempt=0; attempt<200; ++attempt) {
            if (MoveFileExA(staging.string().c_str(), "stock.txt",
                MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
                replaced=true;
                break;
            }
            DWORD error=GetLastError();
            if (error!=ERROR_SHARING_VIOLATION &&
                error!=ERROR_ACCESS_DENIED &&
                error!=ERROR_LOCK_VIOLATION) break;
            Sleep(10);
        }
        if (!replaced)
            throw std::runtime_error("No se pudo reemplazar stock.txt en Windows");
#else
        fs::rename(staging,"stock.txt");
#endif
        return true;
    } catch (...) {
        std::error_code ignored;
        fs::remove(staging,ignored);
        return false;
    }
}

using Update = std::function<bool(const std::vector<std::string>&,
                                  std::vector<std::string>*)>;

int transaction(const char *name, const Update &transform, BigIntArray *result) {
    if (!name || !*name) return 0;
    BigIntArray denominations{};
    if (!cargar_denominaciones_moneda(name,&denominations)) return 0;
    const size_t expected=denominations.len;
    bigint_array_free(&denominations);
    try {
        StockFileLock lock;
        std::ifstream file("stock.txt",std::ios::binary);
        if (!file) return 0;
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file,line)) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (line.size()>4096 || lines.size()>10000) return 0;
            lines.push_back(line);
        }
        if (file.bad()) return 0;
        auto header=std::find(lines.begin(),lines.end(),std::string(name));
        if (header==lines.end()) return 0;
        size_t first=static_cast<size_t>(header-lines.begin())+1;
        size_t last=first;
        std::vector<std::string> before;
        while (last<lines.size() && (lines[last]=="-1" || digits_only(lines[last]))) {
            before.push_back(lines[last]=="-1" ? "0" : lines[last]);
            ++last;
        }
        if (before.size()!=expected) return 0;
        std::vector<std::string> after;
        if (!transform(before,&after) || after.size()!=expected ||
            !std::all_of(after.begin(),after.end(),digits_only)) return 0;
        BigIntArray staged{};
        if (result) {
            if (!bigint_array_create(&staged,expected)) return 0;
            for (size_t i=0;i<expected;++i) {
                BigInt temp{nullptr};
                bool ok=bigint_init(&temp,after[i].c_str()) &&
                        bigint_array_set(&staged,i,&temp);
                bigint_free(&temp);
                if (!ok) {bigint_array_free(&staged);return 0;}
            }
        }
        for (size_t i=0;i<expected;++i)
            lines[first+i]=(after[i]=="0" ? "-1" : after[i]);
        if (!replace_atomic(lines)) {bigint_array_free(&staged);return 0;}
        if (result) {bigint_array_free(result);*result=staged;}
        // Journal best effort AFTER durable stock replacement. Like the C
        // original, an audit write failure does not roll back the stock.
        {
            std::time_t now=std::time(nullptr);
            std::tm stamp{};
#ifdef _WIN32
            if (localtime_s(&stamp,&now)==0)
#else
            if (localtime_r(&now,&stamp)!=nullptr)
#endif
            {
                char timestamp[40]={};
                if (std::strftime(timestamp,sizeof(timestamp),"%Y-%m-%d %H:%M:%S",&stamp)) {
                    std::ofstream audit("historial.txt",std::ios::app);
                    if (audit)
                        audit << '[' << timestamp << "] Stock actualizado | Moneda="
                              << name << '\n';
                }
            }
        }
        return 1;
    } catch (...) {return 0;}
}

bool parse_values(const BigIntArray *items,std::vector<std::string> *out) {
    if (!items || !items->items) return false;
    out->clear();
    for (size_t i=0;i<items->len;++i) {
        const char *s=items->items[i].digits;
        if (!s || !digits_only(s)) return false;
        out->emplace_back(s);
    }
    return true;
}

bool calculate_difference(const std::string &left, const std::string &right,
                          bool add, std::string *out) {
    BigInt lhs{nullptr},rhs{nullptr},computed{nullptr};
    bool ok=bigint_init(&lhs,left.c_str()) && bigint_init(&rhs,right.c_str());
    if (ok && (add || bigint_compare(&lhs,&rhs)>=0))
        ok=add ? bigint_add(&lhs,&rhs,&computed)
               : bigint_subtract(&lhs,&rhs,&computed);
    else ok=false;
    if (ok) *out=computed.digits;
    bigint_free(&lhs);bigint_free(&rhs);bigint_free(&computed);
    return ok;
}
} // namespace

int actualizar_stock_moneda(const char *nombreMoneda, const BigIntArray *stock) {
    std::vector<std::string> target;
    if (!parse_values(stock,&target)) return 0;
    return transaction(nombreMoneda,[&](const std::vector<std::string> &before,
                                        std::vector<std::string> *after) {
        if (target.size()!=before.size()) return false;
        *after=target;
        return true;
    },nullptr);
}

int consumir_stock_moneda_transaccional(const char *nombreMoneda,
                                       const BigIntArray *consumo,
                                       BigIntArray *stock_actualizado) {
    std::vector<std::string> used;
    if (!stock_actualizado || !parse_values(consumo,&used)) return 0;
    return transaction(nombreMoneda,[&](const std::vector<std::string> &before,
                                        std::vector<std::string> *after) {
        if (before.size()!=used.size()) return false;
        after->clear();
        for (size_t i=0;i<before.size();++i) {
            std::string left;
            if (!calculate_difference(before[i],used[i],false,&left)) return false;
            after->push_back(left);
        }
        return true;
    },stock_actualizado);
}

int ajustar_stock_moneda_transaccional(const char *nombreMoneda, size_t indice,
                                      const BigInt *delta, int esSuma,
                                      BigIntArray *stock_actualizado) {
    if (!delta || !delta->digits || !digits_only(delta->digits) || !stock_actualizado)
        return 0;
    return transaction(nombreMoneda,[&](const std::vector<std::string> &before,
                                        std::vector<std::string> *after) {
        if (indice>=before.size()) return false;
        *after=before;
        return calculate_difference(before[indice],delta->digits,esSuma!=0,
                                    &(*after)[indice]);
    },stock_actualizado);
}


namespace {
bool sum_specific_value(const BigIntArray *denominations,
                        const BigIntArray *pieces, BigInt *total) {
    if (!denominations || !pieces || denominations->len != pieces->len ||
        !denominations->items || !pieces->items || !total) return false;
    BigInt aggregate{nullptr};
    if (!bigint_init(&aggregate,"0")) return false;
    bool ok=true;
    for (size_t i=0;i<pieces->len;++i) {
        BigInt value{nullptr}, next{nullptr};
        ok=bigint_multiply(&denominations->items[i],&pieces->items[i],&value) &&
           bigint_add(&aggregate,&value,&next);
        bigint_free(&value);
        if (!ok) {bigint_free(&next);break;}
        bigint_free(&aggregate);
        aggregate=next;
    }
    if (ok) {bigint_free(total);*total=aggregate;}
    else bigint_free(&aggregate);
    return ok;
}
}

int intercambiar_stock_moneda_transaccional(const char *moneda,
            const BigIntArray *recibidas, const BigIntArray *entregadas,
            BigIntArray *stock_actualizado) {
    if (!stock_actualizado) return 0;
    std::vector<std::string> input,output;
    if (!parse_values(recibidas,&input) || !parse_values(entregadas,&output) ||
        input.size()!=output.size()) return 0;
    BigIntArray denominations{};
    if (!cargar_denominaciones_moneda(moneda,&denominations)) return 0;
    BigInt incoming{nullptr},outgoing{nullptr};
    bool equal=sum_specific_value(&denominations,recibidas,&incoming) &&
               sum_specific_value(&denominations,entregadas,&outgoing) &&
               bigint_compare(&incoming,&outgoing)==0;
    bigint_array_free(&denominations);
    bigint_free(&incoming);
    bigint_free(&outgoing);
    if (!equal) return 0;
    return transaction(moneda,[&](const std::vector<std::string> &before,
                                  std::vector<std::string> *after) {
        if (before.size()!=input.size()) return false;
        after->clear();
        for (size_t i=0;i<before.size();++i) {
            std::string added,remainder;
            if (!calculate_difference(before[i],input[i],true,&added) ||
                !calculate_difference(added,output[i],false,&remainder))
                return false;
            after->push_back(remainder);
        }
        return true;
    },stock_actualizado);
}


int restaurar_snapshot_stock_transaccional(const char *snapshot) {
    if (!snapshot || !*snapshot) return 0;
    try {
        StockFileLock lock;  // serializa también consumo y cambio específico
        std::ifstream input(snapshot,std::ios::binary);
        if (!input) return 0;
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(input,line)) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (line.size()>4096 || lines.size()>=10000) return 0;
            lines.push_back(line);
        }
        if (input.bad() || lines.empty()) return 0;
        auto sections=[&](const std::vector<std::string> &all,bool is_stock) {
            std::map<std::string,std::vector<std::string>> output;
            std::string key,word;
            for (const auto &ln:all) {
                std::istringstream tokenizer(ln);
                while (tokenizer>>word) {
                    if (digits_only(word) || (is_stock && word=="-1")) {
                        if (key.empty()) throw std::runtime_error("Token sin moneda");
                        output[key].push_back(word);
                    } else {
                        key=word;
                        if (output.contains(key))
                            throw std::runtime_error("Divisa duplicada");
                        output[key]={};
                    }
                }
            }
            return output;
        };
        std::ifstream denominationsFile("monedas.txt",std::ios::binary);
        if (!denominationsFile) return 0;
        std::vector<std::string> dlines;
        while (std::getline(denominationsFile,line)) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (line.size()>4096 || dlines.size()>=10000) return 0;
            dlines.push_back(line);
        }
        if (denominationsFile.bad()) return 0;
        auto denominationSections=sections(dlines,false);
        auto snapshotSections=sections(lines,true);
        if (denominationSections.empty() ||
            denominationSections.size()!=snapshotSections.size()) return 0;
        for (const auto &[key,values]:denominationSections) {
            auto found=snapshotSections.find(key);
            if (values.empty() || found==snapshotSections.end() ||
                found->second.size()!=values.size()) return 0;
            for (const auto &v:found->second)
                if (!digits_only(v) && v!="-1") return 0;
        }
        if (!replace_atomic(lines)) return 0;
        {
            std::ofstream audit("historial.txt",std::ios::app);
            if (audit) audit<<"Snapshot de stock restaurado\n";
        }
        return 1;
    } catch (...) {return 0;}
}
