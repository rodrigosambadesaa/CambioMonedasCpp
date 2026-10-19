#include "moneda_gestion.h"
#include "bigint.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
struct Array {
    BigIntArray value{};
    explicit Array(std::initializer_list<const char*> digits) {
        assert(bigint_array_create(&value,digits.size()));
        std::size_t i=0;
        for (const char *digit:digits) {
            BigInt number{nullptr};
            assert(bigint_init(&number,digit));
            assert(bigint_array_set(&value,i++,&number));
            bigint_free(&number);
        }
    }
    ~Array(){bigint_array_free(&value);}
};
int main() {
    auto original=fs::current_path();
    auto folder=fs::temp_directory_path()/("progvoraz-tx-"+std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(fs::create_directories(folder));
    fs::current_path(folder);
    try {
        {
            std::ofstream d("monedas.txt");
            d << "euro\n5\n3\n1\ndolar\n10\n1\n";
            std::ofstream s("stock.txt");
            s << "euro\n4\n2\n-1\ndolar\n1\n2\n";
        }
        Array overlong({"0","0","0","0"});
        assert(!actualizar_stock_moneda("euro",&overlong.value));
        BigIntArray other{};
        assert(cargar_stock_moneda("dolar",&other));
        assert(std::string(other.items[0].digits)=="1");
        bigint_array_free(&other);
        Array consume({"2","1","0"});
        BigIntArray left{};
        assert(consumir_stock_moneda_transaccional("euro",&consume.value,&left));
        assert(std::string(left.items[0].digits)=="2");
        assert(std::string(left.items[1].digits)=="1");
        bigint_array_free(&left);
        Array unavailable({"3","0","0"});
        assert(!consumir_stock_moneda_transaccional("euro",&unavailable.value,&left));
        BigInt increase{nullptr};
        assert(bigint_init(&increase,"7"));
        assert(ajustar_stock_moneda_transaccional("euro",0,&increase,1,&left));
        assert(std::string(left.items[0].digits)=="9");
        bigint_array_free(&left);
        assert(ajustar_stock_moneda_transaccional("euro",0,&increase,0,&left));
        assert(std::string(left.items[0].digits)=="2");
        bigint_array_free(&left);
        bigint_free(&increase);
        assert(cargar_stock_moneda("dolar",&other));
        assert(std::string(other.items[0].digits)=="1");
        bigint_array_free(&other);
        std::cout << "OK: stock atomico, limites de bloque, consumo y administrador\n";
    } catch (...) {
        fs::current_path(original);
        fs::remove_all(folder);
        throw;
    }
    fs::current_path(original);
    fs::remove_all(folder);
    return 0;
}
