#include "algoritmo_cambio.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <iostream>
#include <cstring>

struct Values {
    BigIntArray array{nullptr, 0};
    explicit Values(std::vector<std::string> words) {
        if (!bigint_array_create(&array,words.size()))throw std::bad_alloc();
        for(std::size_t i=0;i<words.size();++i) {
            BigInt n{nullptr};
            bool ok=bigint_init(&n,words[i].c_str())&&bigint_array_set(&array,i,&n);
            bigint_free(&n);
            if(!ok)throw std::bad_alloc();
        }
    }
    ~Values(){bigint_array_free(&array);}
    Values(const Values&)=delete;
    Values& operator=(const Values&)=delete;
};
void check(const char *name,const char *amount,std::vector<std::string> d,
           std::vector<std::string> st,const char* expected,
           std::vector<std::string> expectedCoins) {
    BigInt m{nullptr}, covered{nullptr};
    BigIntArray result{nullptr,0};
    Values denoms(d), stock(st.empty()?std::vector<std::string>(d.size(),"0"):st);
    if(!bigint_init(&m,amount))throw std::runtime_error("init");
    int ok=st.empty()?
        calcular_cambio_cercano_con_rango(&m,&denoms.array,0,1,&covered,&result):
        calcular_cambio_cercano_stock_con_rango(&m,&denoms.array,&stock.array,0,1,&covered,&result);
    bool pass=(ok!=0)==(expected!=nullptr);
    if(expected&&ok) {
        pass=pass&&covered.digits&&std::strcmp(covered.digits,expected)==0&&
             result.len==expectedCoins.size();
        for(std::size_t i=0;i<result.len&&i<expectedCoins.size();++i)
            pass=pass&&result.items[i].digits&&
                expectedCoins[i]==result.items[i].digits;
    }
    bigint_free(&m);bigint_free(&covered);bigint_array_free(&result);
    if(!pass){std::cerr<<"FAIL "<<name<<"\n";throw std::runtime_error(name);}
    std::cout<<"PASS "<<name<<"\n";
}
int main(){
    check("optimal BigInt nearest", "700001",
        {"500000","300000","1"}, {}, "500000", {"1","0","0"});
    check("bounded BigInt nearest", "700001",
        {"500000","300000","1"}, {"0","1","10"},"300000",{"0","1","0"});
    check("no coin fits", "700001",
        {"800000","900000"}, {}, nullptr, {});
    check("all stock empty", "999999999999999999999999999999",
        {"500000","300000"}, {"0","0"}, nullptr, {});
    check("one exact huge", "500000000000000000000000001",
        {"500000000000000000000000001","1"}, {}, 
        "500000000000000000000000001",{"1","0"});
}
