#include "algoritmo_cambio.h"
#include <iostream>
#include <string>
#include <vector>

struct Numbers {
    BigIntArray items{nullptr, 0};
    explicit Numbers(const std::vector<std::string> &texts) {
        if (!bigint_array_create(&items, texts.size())) throw std::bad_alloc();
        for (std::size_t i = 0; i < texts.size(); ++i) {
            BigInt v{nullptr};
            bool ok = bigint_init(&v, texts[i].c_str()) &&
                      bigint_array_set(&items, i, &v);
            bigint_free(&v);
            if (!ok) throw std::bad_alloc();
        }
    }
    ~Numbers() { bigint_array_free(&items); }
    Numbers(const Numbers&) = delete;
    Numbers& operator=(const Numbers&) = delete;
};

static void test(const char *name, const std::string &amount, int mode,
                 const std::vector<std::string> &denom,
                 const std::vector<std::string> &stock,
                 bool exists, const std::string &covered,
                 const std::vector<std::string> &counts, std::size_t max=100) {
    Numbers denominations(denom);
    Numbers stockItems(stock.empty() ? std::vector<std::string>(denom.size(), "0") : stock);
    BigInt value{nullptr}, coverage{nullptr};
    BigIntArray solution{nullptr,0};
    if (!bigint_init(&value, amount.c_str())) throw std::runtime_error("setup");
    int ok=0;
    if (mode==0) ok=calcular_cambio_optimo(&value,&denominations.items,&solution);
    if (mode==1) ok=calcular_cambio_optimo_stock(&value,&denominations.items,&stockItems.items,&solution);
    if (mode==2) ok=calcular_cambio_cercano_con_rango(&value,&denominations.items,0,max,&coverage,&solution);
    if (mode==3) ok=calcular_cambio_cercano_stock_con_rango(&value,&denominations.items,&stockItems.items,0,max,&coverage,&solution);
    bool pass=(ok!=0)==exists;
    if (ok) {
        pass=pass && solution.len==counts.size();
        for (std::size_t i=0; i<counts.size() && i<solution.len; ++i)
            pass=pass && solution.items[i].digits &&
                       counts[i]==solution.items[i].digits;
        if(mode>=2)pass=pass && coverage.digits && covered==coverage.digits;
    }
    bigint_array_free(&solution);
    bigint_free(&value);
    bigint_free(&coverage);
    if(!pass) {std::cerr<<"FAIL "<<name<<"\n";throw std::runtime_error(name);}
    std::cout<<"PASS "<<name<<"\n";
}

int main() {
    test("unlimited optimum scaled", "1400000", 0,
         {"800000","700000","100000"}, {}, true,"",
         {"0","2","0"});
    test("bounded optimum scaled", "1400000", 1,
         {"800000","700000","100000"}, {"1","2","0"}, true,"",
         {"0","2","0"});
    test("impossible exact remainder", "1400001", 0,
         {"800000","700000","100000"}, {}, false,"",{});
    test("closest scaled", "1400001", 2,
         {"800000","700000","100000"}, {}, true,"1400000",
         {"0","2","0"},2);
    test("closest one coin beyond threshold", "500001", 2,
         {"500000"}, {}, true,"500000",{"1"},1);
    test("stock closest scaled", "1400001", 3,
         {"800000","700000","100000"}, {"1","2","0"}, true,"1400000",
         {"0","2","0"},2);
    return 0;
}
