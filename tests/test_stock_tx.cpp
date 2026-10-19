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
        Array incoming({"0","0","5"}),outgoing({"1","0","0"});
        BigIntArray exchanged{};
        assert(intercambiar_stock_moneda_transaccional("euro",&incoming.value,
                                                        &outgoing.value,&exchanged));
        assert(std::string(exchanged.items[0].digits)=="1");
        assert(std::string(exchanged.items[1].digits)=="1");
        assert(std::string(exchanged.items[2].digits)=="5");
        bigint_array_free(&exchanged);
        Array bad_input({"0","2","0"}),bad_output({"0","0","6"});
        assert(!intercambiar_stock_moneda_transaccional("euro",&bad_input.value,
                                                         &bad_output.value,&exchanged));
        Array wrong_input({"0","0","1"});
        assert(!intercambiar_stock_moneda_transaccional("euro",&wrong_input.value,
                                                         &outgoing.value,&exchanged));
        BigIntArray unchanged{};
        assert(cargar_stock_moneda("euro",&unchanged));
        assert(std::string(unchanged.items[0].digits)=="1");
        assert(std::string(unchanged.items[2].digits)=="5");
        bigint_array_free(&unchanged);
        std::ifstream audit("historial.txt");
        assert(audit);
        int lines=0;
        std::string entry;
        while(std::getline(audit,entry)) {
            assert(entry.find("Moneda=euro")!=std::string::npos);
            ++lines;
        }
        assert(lines==4); // consumo, dos ajustes y cambio especifico
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
