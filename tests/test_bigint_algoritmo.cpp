#include <stdio.h>
#include <string.h>
#include "bigint.h"
#include "algoritmo_cambio.h"
#include <algorithm>
#include <functional>
#include <limits>
#include <random>
#include <string>
#include <vector>

static int fallos = 0;

static void check_int(int condicion, const char *mensaje)
{
    if (!condicion)
    {
        printf("FAIL: %s\n", mensaje);
        fallos++;
    }
}

static void check_arreglo_texto(const BigIntArray *arr, const char *const *esperado, size_t len, const char *mensaje)
{
    if (arr == NULL || arr->items == NULL || arr->len != len)
    {
        printf("FAIL: %s (longitud esperada %zu)\n", mensaje, len);
        fallos++;
        return;
    }

    for (size_t i = 0; i < len; i++)
    {
        if (arr->items[i].digits == NULL || strcmp(arr->items[i].digits, esperado[i]) != 0)
        {
            printf("FAIL: %s (indice %zu esperado=%s obtenido=%s)\n", mensaje, i, esperado[i], arr->items[i].digits ? arr->items[i].digits : "(null)");
            fallos++;
            return;
        }
    }
}

static int crear_arreglo(BigIntArray *arr, const char *const *valores, size_t len)
{
    size_t i;

    if (!bigint_array_create(arr, len))
        return 0;

    for (i = 0; i < len; i++)
    {
        BigInt tmp = {0};
        if (!bigint_init(&tmp, valores[i]) || !bigint_array_set(arr, i, &tmp))
        {
            bigint_free(&tmp);
            bigint_array_free(arr);
            return 0;
        }
        bigint_free(&tmp);
    }

    return 1;
}

static void test_cambio_no_canonico_ilimitado(void)
{
    const char *denom_txt[] = {"4", "3", "1"};
    const char *esperado[] = {"0", "2", "0"};
    BigInt monto = {0};
    BigIntArray denom = {0};
    BigIntArray solucion = {0};

    check_int(bigint_init(&monto, "6"), "inicializa monto no canonico");
    check_int(crear_arreglo(&denom, denom_txt, 3), "crea denominaciones no canonicas");
    check_int(calcular_cambio_optimo(&monto, &denom, &solucion), "calcula cambio no canonico ilimitado");
    check_arreglo_texto(&solucion, esperado, 3, "elige minimo numero de monedas en modo ilimitado");

    bigint_free(&monto);
    bigint_array_free(&denom);
    bigint_array_free(&solucion);
}


static void test_rangos_stock_bigint(void) {
    const char *d[] = {"4","3","1"}, *stock[] = {"1","2","0"};
    BigInt amount{nullptr};
    BigIntArray denom{}, inventory{}, result{};
    check_int(bigint_init(&amount,"6"), "monto");
    check_int(crear_arreglo(&denom,d,3), "denominaciones");
    check_int(crear_arreglo(&inventory,stock,3), "inventario");
    check_int(calcular_cambio_optimo_stock(&amount,&denom,&inventory,&result),"acotado");
    const char *expected[]={"0","2","0"};
    check_arreglo_texto(&result,expected,3,"reconstruccion sin exceder stock");
    bigint_array_free(&result);
    check_int(calcular_cambio_optimo_con_rango(&amount,&denom,3,3,&result),"rango exacto");
    const char *ranged[]={"1","0","2"};
    check_arreglo_texto(&result,ranged,3,"tres piezas 4+1+1");
    bigint_array_free(&result);
    check_int(!calcular_cambio_optimo_stock_con_rango(&amount,&denom,&inventory,3,3,&result),"rango incompatible con stock");
    bigint_array_free(&result);
    bigint_array_free(&inventory); bigint_array_free(&denom); bigint_free(&amount);

    const char *large_d[]={"50000","1"};
    const char *large_s[]={"999999999999999999999999999","999999999999999999999999999"};
    BigInt big{nullptr}; BigIntArray d2{},s2{},r2{};
    check_int(bigint_init(&big,"600000"),"monto > techo DP");
    check_int(crear_arreglo(&d2,large_d,2),"denom grandes");
    check_int(crear_arreglo(&s2,large_s,2),"stock gigantesco");
    check_int(calcular_cambio_optimo_stock(&big,&d2,&s2,&r2),"fallback BigInt grande");
    const char *big_expected[]={"12","0"};
    check_arreglo_texto(&r2,big_expected,2,"doce piezas de 50000");
    bigint_free(&big);bigint_array_free(&d2);bigint_array_free(&s2);bigint_array_free(&r2);
}


struct Expected {
    bool found=false;
    int covered=-1;
    int pieces=std::numeric_limits<int>::max();
};

static Expected brute(int amount, const std::vector<int> &coins,
                      const std::vector<int> *stock, int min, int max, bool near) {
    Expected best;
    std::function<void(std::size_t,int,int)> walk=[&](std::size_t i,int sum,int pieces) {
        if (sum>amount || pieces>max) return;
        if (i==coins.size()) {
            if (pieces<min || (near && sum==0) || (!near && sum!=amount)) return;
            if (!best.found || sum>best.covered || (sum==best.covered && pieces<best.pieces)) {
                best={true,sum,pieces};
            }
            return;
        }
        int limit=(amount-sum)/coins[i];
        if (stock) limit=std::min(limit,(*stock)[i]);
        for (int n=0;n<=limit;++n) walk(i+1,sum+n*coins[i],pieces+n);
    };
    walk(0,0,0);
    return best;
}

static void differential_property_tests(void) {
    std::mt19937 generator(0xC0FFEE);
    std::uniform_int_distribution<int> denomination_distribution(1,10);
    std::uniform_int_distribution<int> stock_distribution(0,3);
    std::uniform_int_distribution<int> amount_distribution(0,24);
    std::uniform_int_distribution<int> size_distribution(2,5);
    for(int iteration=0;iteration<80;++iteration) {
        const int n=size_distribution(generator);
        std::vector<int> coins;
        while (coins.size()<static_cast<std::size_t>(n)) {
            int v=denomination_distribution(generator);
            if (std::find(coins.begin(),coins.end(),v)==coins.end()) coins.push_back(v);
        }
        std::sort(coins.rbegin(),coins.rend());
        std::vector<int> stock;
        for(int i=0;i<n;++i) stock.push_back(stock_distribution(generator));
        int amount=amount_distribution(generator);
        std::vector<std::string> dstrings, sstrings;
        std::vector<const char *> ds,ss;
        for(int value:coins) dstrings.push_back(std::to_string(value));
        for(int value:stock) sstrings.push_back(std::to_string(value));
        for(const auto &v:dstrings) ds.push_back(v.c_str());
        for(const auto &v:sstrings) ss.push_back(v.c_str());
        BigInt target{nullptr}; BigIntArray denominations{}, inventory{};
        check_int(bigint_init(&target,std::to_string(amount).c_str()),"valor property");
        check_int(crear_arreglo(&denominations,ds.data(),ds.size()),"denom property");
        check_int(crear_arreglo(&inventory,ss.data(),ss.size()),"stock property");
        for(bool limited : {false,true}) {
            const auto *limits=limited ? &stock : nullptr;
            for(int variation=0;variation<3;++variation) {
                int min=variation==0 ? 0 : variation==1 ? 2 : 1;
                int max=variation==0 ? 1000 : variation==1 ? 4 : 6;
                bool nearest=variation==2;
                Expected reference=brute(amount,coins,limits,min,max,nearest);
                BigIntArray answer{};
                BigInt covered{nullptr};
                int success=0;
                if(nearest)
                    success=limited
                        ? calcular_cambio_cercano_stock_con_rango(&target,&denominations,&inventory,min,max,&covered,&answer)
                        : calcular_cambio_cercano_con_rango(&target,&denominations,min,max,&covered,&answer);
                else
                    success=limited
                        ? calcular_cambio_optimo_stock_con_rango(&target,&denominations,&inventory,min,max,&answer)
                        : calcular_cambio_optimo_con_rango(&target,&denominations,min,max,&answer);
                check_int(static_cast<bool>(success)==reference.found,"differential existence");
                if (success && reference.found) {
                    int actualSum=0,actualPieces=0;
                    for(std::size_t i=0;i<answer.len;++i) {
                        int count=std::stoi(answer.items[i].digits);
                        actualSum+=coins[i]*count;
                        actualPieces+=count;
                        if(limited) check_int(count<=stock[i],"differential stock");
                    }
                    check_int(actualSum==reference.covered,"differential covered");
                    check_int(actualPieces==reference.pieces,"differential min pieces");
                    if(nearest)
                        check_int(covered.digits && std::to_string(reference.covered)==covered.digits,"differential near");
                }
                bigint_array_free(&answer);bigint_free(&covered);
            }
        }
        bigint_array_free(&denominations);bigint_array_free(&inventory);bigint_free(&target);
    }
}


static void test_near_high_amount(void) {
    const char *coins[]={"50000"};
    BigInt requested{nullptr},covered{nullptr};
    BigIntArray denominations{},answer{};
    check_int(bigint_init(&requested,"150001"),"large nearest target");
    check_int(crear_arreglo(&denominations,coins,1),"large nearest denoms");
    check_int(calcular_cambio_cercano_con_rango(&requested,&denominations,0,
              std::numeric_limits<std::size_t>::max(),&covered,&answer),"single-pass nearest");
    check_int(covered.digits && std::string(covered.digits)=="150000","nearest reaches 150000");
    const char *expected[]={"3"};
    check_arreglo_texto(&answer,expected,1,"nearest uses three coins");
    bigint_array_free(&answer);
    bigint_free(&covered);
    bigint_array_free(&denominations);
    bigint_free(&requested);
    const char *tooLarge[]={"200001"};
    check_int(bigint_init(&requested,"150000"),"unreachable target");
    check_int(crear_arreglo(&denominations,tooLarge,1),"unreachable denom");
    check_int(!calcular_cambio_cercano_con_rango(&requested,&denominations,0,
              std::numeric_limits<std::size_t>::max(),&covered,&answer),
              "no positive closest change");
    bigint_array_free(&answer);bigint_array_free(&denominations);
    bigint_free(&covered);bigint_free(&requested);
}


static void test_nearest_single_bigint_boundary(void) {
    const char *coins[]={"500000"};
    const char *st[]={"2"};
    BigInt amount{nullptr}, covered{nullptr};
    BigIntArray denominations{}, stock{}, answer{};
    check_int(bigint_init(&amount, "500001"), "boundary nearest amount");
    check_int(crear_arreglo(&denominations,coins,1), "boundary nearest denomination");
    check_int(crear_arreglo(&stock,st,1), "boundary nearest stock");
    check_int(calcular_cambio_cercano_con_rango(&amount,&denominations,0,1,
              &covered,&answer), "nearest above C++ DP maximum with coin limit");
    check_int(covered.digits && std::string(covered.digits)=="500000",
              "nearest returns partial amount");
    const char *one[]={"1"};
    check_arreglo_texto(&answer,one,1,"nearest count is bounded");
    bigint_array_free(&answer);bigint_free(&covered);
    check_int(calcular_cambio_cercano_stock_con_rango(&amount,&denominations,
              &stock,1,1,&covered,&answer),"nearest BigInt stock, minimum");
    check_int(covered.digits && std::string(covered.digits)=="500000",
              "nearest stocked covered");
    check_arreglo_texto(&answer,one,1,"nearest stocked quantity");
    bigint_array_free(&answer);bigint_free(&covered);
    check_int(!calcular_cambio_cercano_stock_con_rango(&amount,&denominations,
              &stock,3,3,&covered,&answer),"nearest rejects impossible minimum");
    bigint_array_free(&answer);bigint_free(&covered);
    bigint_array_free(&stock);bigint_array_free(&denominations);bigint_free(&amount);
}

static void test_noncanonical_large_residue(void) {
    const char *denominations[]={"4","3","1"};
    BigIntArray values{},answer{};
    BigInt amount{nullptr};
    check_int(crear_arreglo(&values,denominations,3),"noncanonical denoms");
    check_int(bigint_init(&amount,"500006"),"large noncanonical amount");
    check_int(calcular_cambio_optimo(&amount,&values,&answer),
              "large exact noncanonical Dijkstra result");
    const char *expected[]={"125000","2","0"};
    check_arreglo_texto(&answer,expected,3,"Dijkstra beats greedy by one piece");
    bigint_array_free(&answer);
    check_int(!calcular_cambio_optimo_con_limite(&amount,&values,125001,&answer),
              "proven minimum exceeds maximum piece constraint");
    bigint_array_free(&answer);
    bigint_free(&amount);
    check_int(bigint_init(&amount,"1000000000000000000000000000000000000000006"),
              "astronomical noncanonical amount");
    check_int(calcular_cambio_optimo(&amount,&values,&answer),
              "astronomical optimal solution");
    BigInt total{nullptr};
    check_int(bigint_init(&total,"0"),"astronomical sum");
    for (std::size_t i=0;i<answer.len;++i) {
        BigInt product{nullptr},sum{nullptr};
        check_int(bigint_multiply(&values.items[i],&answer.items[i],&product),
                  "astronomical product");
        check_int(bigint_add(&total,&product,&sum),"astronomical aggregate");
        bigint_free(&total);total=sum;
        bigint_free(&product);
    }
    check_int(bigint_compare(&total,&amount)==0,"astronomical sum exact");
    bigint_free(&total);
    bigint_array_free(&values);bigint_array_free(&answer);bigint_free(&amount);
}


static void test_bigint_two_denominations(void) {
    const char *denoms[]={"7","4"};
    const char *bounded[]={"71422","100"};
    const char *insufficient[]={"71421","13"};
    const char *optimal[]={"71426","6"};
    const char *optimalStock[]={"71422","13"};
    BigInt amount{nullptr};
    BigIntArray values{},stock{},result{},tooLittle{};
    check_int(bigint_init(&amount,"500006"),"two-denomination amount");
    check_int(crear_arreglo(&values,denoms,2),"two-denomination data");
    check_int(crear_arreglo(&stock,bounded,2),"two-denomination stock");
    check_int(calcular_cambio_optimo(&amount,&values,&result),"Diophantine unbounded");
    check_arreglo_texto(&result,optimal,2,"maximise higher denomination exactly");
    bigint_array_free(&result);
    check_int(calcular_cambio_optimo_stock(&amount,&values,&stock,&result),
        "Diophantine bounded");
    check_arreglo_texto(&result,optimalStock,2,"optimal under bounded stock");
    bigint_array_free(&result);
    check_int(calcular_cambio_optimo_stock_con_rango(&amount,&values,&stock,
        71435,71435,&result),"exact coin count with bounded stock");
    check_arreglo_texto(&result,optimalStock,2,"exact count solution");
    bigint_array_free(&result);
    check_int(crear_arreglo(&tooLittle,insufficient,2),"insufficient stock");
    check_int(!calcular_cambio_optimo_stock(&amount,&values,&tooLittle,&result),
        "reject unavailable exact change");
    bigint_array_free(&result);
    bigint_array_free(&stock);bigint_array_free(&tooLittle);
    bigint_array_free(&values);bigint_free(&amount);
}

static void test_medium_exact_two_nonminimal_range(void) {
    const char *denoms[]={"20000","10000"};
    const char *inventory[]={"1","2"};
    const char *expected[]={"1","2"};
    BigInt amount{nullptr};
    BigIntArray denominations{}, stock{}, result{};
    check_int(bigint_init(&amount,"40000"),"medium positive minimum amount");
    check_int(crear_arreglo(&denominations,denoms,2),"medium denominations");
    check_int(crear_arreglo(&stock,inventory,2),"medium bounded stock");
    check_int(calcular_cambio_optimo_con_rango(&amount,&denominations,3,3,&result),
              "medium nonminimal exact with three pieces");
    check_arreglo_texto(&result,expected,2,
                         "medium exact range chooses one large two small");
    bigint_array_free(&result);
    check_int(calcular_cambio_optimo_stock_con_rango(&amount,&denominations,
              &stock,3,3,&result),"medium nonminimal stocked exact");
    check_arreglo_texto(&result,expected,2,
                         "medium stocked exact respects inventory");
    bigint_array_free(&result);
    check_int(!calcular_cambio_optimo_stock_con_rango(&amount,&denominations,
               &stock,4,4,&result),"stocked exact impossible four-piece range");
    bigint_array_free(&result);
    bigint_array_free(&stock);bigint_array_free(&denominations);
    bigint_free(&amount);
}

static void test_more_than_size_t_max_coins(void) {
    const char *d[]={"75001"}, *single[]={"999999999999999999999999999999999999999999999999"};
    BigIntArray denom{},stock{},solution{};
    BigInt amount{nullptr},million{nullptr};
    check_int(crear_arreglo(&denom,d,1),"huge single denomination");
    check_int(crear_arreglo(&stock,single,1),"huge piece count");
    check_int(bigint_multiply(&denom.items[0],&stock.items[0],&amount),
              "huge amount from count and denomination");
    check_int(calcular_cambio_optimo(&amount,&denom,&solution),
              "SIZE_MAX must mean unbounded number of pieces");
    check_arreglo_texto(&solution,single,1,
                        "the result preserves counts larger than size_t");
    bigint_array_free(&solution);
    check_int(!calcular_cambio_optimo_con_limite(&amount,&denom,100,&solution),
              "explicit max 100 pieces must reject astronomical count");
    bigint_array_free(&solution);
    bigint_array_free(&stock);bigint_array_free(&denom);
    bigint_free(&amount);bigint_free(&million);
}


static void test_sparse_large_inventory(void) {
    const char *d[]={"800000","700000","100000"},
               *limited[]={"1","2","0"}, *solution[]={"0","2","0"};
    BigInt amount{nullptr},nearAmount{nullptr},covered{nullptr};
    BigIntArray denominations{},stock{},result{},nearestStock{};
    check_int(bigint_init(&amount,"1400000"),"large inventory amount");
    check_int(crear_arreglo(&denominations,d,3),"large inventory denominations");
    check_int(crear_arreglo(&stock,limited,3),"large inventory stock");
    check_int(calcular_cambio_optimo_stock(&amount,&denominations,&stock,&result),
              "exact sparse stock beats greedy NO");
    check_arreglo_texto(&result,solution,3,"two 700000-cent coins");
    bigint_array_free(&result);
    bigint_array_free(&stock);
    const char *nearStockText[]={"1","1","0"},*nearExpected[]={"1","1","0"};
    check_int(crear_arreglo(&nearestStock,nearStockText,3),"near stock");
    check_int(bigint_init(&nearAmount,"1600000"),"closest requested amount");
    check_int(calcular_cambio_cercano_stock_con_rango(&nearAmount,&denominations,
       &nearestStock,0,std::numeric_limits<std::size_t>::max(),&covered,&result),
       "closest BigInt stock via sparse DP");
    check_int(covered.digits && std::string(covered.digits)=="1500000",
       "correct closest large sum");
    check_arreglo_texto(&result,nearExpected,3,"closest two coins");
    bigint_array_free(&result);bigint_free(&covered);
    bigint_array_free(&nearestStock);bigint_array_free(&denominations);
    bigint_free(&nearAmount);bigint_free(&amount);

    const char *d2[]={"800000","400000","200000"},
               *s2[]={"1","3","2"};
    check_int(crear_arreglo(&denominations,d2,3),"ranged sparse denominations");
    check_int(crear_arreglo(&stock,s2,3),"ranged sparse stock");
    check_int(bigint_init(&amount,"1200000"),"ranged sparse amount");
    check_int(calcular_cambio_optimo_stock_con_rango(&amount,&denominations,
       &stock,3,3,&result),"exact three coin result with sparse DP");
    std::size_t total=0;
    for(std::size_t i=0;i<result.len;++i)
        total+=static_cast<std::size_t>(std::stoull(result.items[i].digits));
    check_int(total==3,"exact lower coin bound retained");
    bigint_array_free(&result);bigint_array_free(&stock);
    bigint_array_free(&denominations);bigint_free(&amount);
}

static void test_large_noncanonical_gcd1_small_coin_witness(void)
{
    const char *d[]={"800001","700000","100001","1"};
    const char *s[]={"100","2","100","100"};
    const char *optimal[]={"0","2","0","0"};
    BigInt amount{nullptr}; BigIntArray denom{},stock{},answer{};
    check_int(crear_arreglo(&denom,d,4),"noncanonical GCD=1 denom");
    check_int(bigint_init(&amount,"1400000"),"noncanonical GCD=1 amount");
    check_int(calcular_cambio_optimo(&amount,&denom,&answer),
              "noncanonical GCD=1 exact optimum exists");
    check_arreglo_texto(&answer,optimal,4,"two 700000 pieces beat greedy 100000");
    bigint_array_free(&answer);
    check_int(crear_arreglo(&stock,s,4),"noncanonical GCD=1 stock");
    check_int(calcular_cambio_optimo_stock(&amount,&denom,&stock,&answer),
              "noncanonical GCD=1 exact optimum with stock");
    check_arreglo_texto(&answer,optimal,4,"finite-stock two-coin optimum");
    bigint_array_free(&answer);bigint_array_free(&stock);bigint_free(&amount);

    const char *three[]={"0","3","0","0"};
    check_int(bigint_init(&amount,"2100000"),"three-coin large amount");
    check_int(calcular_cambio_optimo_con_limite(&amount,&denom,3,&answer),
              "noncanonical three-piece exact solution exists");
    check_arreglo_texto(&answer,three,4,"three pieces certified minimal");
    bigint_array_free(&answer);bigint_free(&amount);

    const char *four[]={"0","4","0","0"};
    check_int(bigint_init(&amount,"2800000"),"four-coin large amount");
    check_int(calcular_cambio_optimo_con_limite(&amount,&denom,4,&answer),
              "noncanonical four-piece exact solution exists");
    check_arreglo_texto(&answer,four,4,"four pieces certified minimal");
    bigint_array_free(&answer);bigint_free(&amount);bigint_array_free(&denom);
}

static void test_bigint_unsorted_large_greedy_fallback(void)
{
    // More than four pieces: bypass the short-witness solver and exercise
    // the BigInt fallback with user-supplied unsorted coin values.
    const char *d[]={"1","1000001"};
    const char *expected[]={"0","5"};
    const char *st[]={"6000000","5"};
    BigInt amount{nullptr};BigIntArray denom{},stock{},out{};
    check_int(bigint_init(&amount,"5000005"),"unsorted BigInt amount");
    check_int(crear_arreglo(&denom,d,2),"unsorted BigInt denom");
    check_int(calcular_cambio_optimo(&amount,&denom,&out),
              "unsorted BigInt exact result");
    check_arreglo_texto(&out,expected,2,
              "unsorted BigInt fallback chooses largest value first");
    bigint_array_free(&out);
    check_int(crear_arreglo(&stock,st,2),"unsorted BigInt stock");
    check_int(calcular_cambio_optimo_stock(&amount,&denom,&stock,&out),
              "unsorted BigInt stock result");
    check_arreglo_texto(&out,expected,2,
              "unsorted BigInt stock fallback preserves input order");
    bigint_array_free(&out);bigint_array_free(&stock);
    bigint_array_free(&denom);bigint_free(&amount);
}


static void test_sparse_limit_not_false_impossibility(void)
{
    // 19 distinct powers of two produce 2^18 > 250,000 sparse states,
    // although the whole stock sum has the obvious exact solution.
    // Prior to the fix the sparse solver returned 0 ("impossible") and
    // prevented the correct large-value fallback from being attempted.
    std::vector<std::string> values, ones(19,"1");
    for(int i=18;i>=0;--i)
        values.push_back(std::to_string((1ULL<<i)*1000000ULL));
    std::vector<const char*> valuePtrs, stockPtrs;
    for(const auto &v:values) valuePtrs.push_back(v.c_str());
    for(const auto &v:ones) stockPtrs.push_back(v.c_str());
    BigInt amount{nullptr};
    BigIntArray coins{},stock{},answer{};
    check_int(bigint_init(&amount,"524287000000"),"sparse limit amount");
    check_int(crear_arreglo(&coins,valuePtrs.data(),valuePtrs.size()),
              "sparse limit binary denominations");
    check_int(crear_arreglo(&stock,stockPtrs.data(),stockPtrs.size()),
              "sparse limit unit stocks");
    check_int(calcular_cambio_optimo_stock(&amount,&coins,&stock,&answer),
              "sparse resource cap must not masquerade as impossibility");
    check_int(answer.len==19,"sparse cap result denomination count");
    if(answer.len==19)
        for(std::size_t i=0;i<answer.len;++i)
            check_int(answer.items[i].digits &&
                      std::string(answer.items[i].digits)=="1",
                      "sparse cap exact binary-coin solution");
    bigint_array_free(&answer);
    bigint_array_free(&coins);bigint_array_free(&stock);bigint_free(&amount);
}


static void test_nearest_bigint_two_denominations(void)
{
    const char *d[]={"50000","30000"};
    BigIntArray denom{},answer{};
    BigInt amount{nullptr},covered{nullptr};
    check_int(crear_arreglo(&denom,d,2),"nearest 2-coin setup");

    check_int(bigint_init(&amount,"500001"),"nearest 2-coin amount");
    check_int(calcular_cambio_cercano_con_rango(&amount,&denom,0,
        std::numeric_limits<std::size_t>::max(),&covered,&answer),
        "closest BigInt two-coin solution above DP limit");
    check_int(covered.digits && std::string(covered.digits)=="500000",
        "two-coin closest coverage");
    const char *expected[]={"10","0"};
    check_arreglo_texto(&answer,expected,2,"two-coin nearest optimum");
    bigint_array_free(&answer);bigint_free(&covered);bigint_free(&amount);

    check_int(bigint_init(&amount,"610001"),"near two-coin mixed amount");
    check_int(calcular_cambio_cercano_con_rango(&amount,&denom,0,
        std::numeric_limits<std::size_t>::max(),&covered,&answer),
        "mixed nearest BigInt two denominations");
    check_int(covered.digits && std::string(covered.digits)=="610000",
        "mixed nearest BigInt coverage");
    const char *mixed[]={"11","2"};
    check_arreglo_texto(&answer,mixed,2,"mixed nearest BigInt optimum");
    bigint_array_free(&answer);bigint_free(&covered);bigint_free(&amount);
    bigint_array_free(&denom);
}

static void test_nearest_coprime_bigint_two_denominations(void)
{
    const char *d[]={"50000","29999"};
    BigIntArray denom{},answer{};
    BigInt amount{nullptr},covered{nullptr};
    check_int(crear_arreglo(&denom,d,2),"coprime nearest BigInt setup");
    check_int(bigint_init(&amount,"500001"),"coprime amount");
    check_int(calcular_cambio_cercano_con_rango(&amount,&denom,0,
        std::numeric_limits<std::size_t>::max(),&covered,&answer),
        "coprime nearest above 500000");
    check_int(covered.digits && std::string(covered.digits)=="500000",
        "coprime nearest coverage");
    const char *simple[]={"10","0"};
    check_arreglo_texto(&answer,simple,2,"coprime nearest fewest coins");
    bigint_array_free(&answer);bigint_free(&covered);bigint_free(&amount);

    check_int(bigint_init(&amount,"610001"),"coprime mixed amount");
    check_int(calcular_cambio_cercano_con_rango(&amount,&denom,0,
        std::numeric_limits<std::size_t>::max(),&covered,&answer),
        "coprime mixed closest");
    check_int(covered.digits && std::string(covered.digits)=="609998",
        "coprime mixed maximizes coverage");
    const char *mixed[]={"11","2"};
    check_arreglo_texto(&answer,mixed,2,"coprime mixed exact count");
    bigint_array_free(&answer);bigint_free(&covered);bigint_free(&amount);
    bigint_array_free(&denom);
}

static void test_nearest_bigint_two_denominations_stock_large(void)
{
    const char *d[]={"50000","29999"};
    const char *s1[]={"1000","1000"};
    const char *s2[]={"1","513"};
    const char *s3[]={"0","513"};
    const char *expect1[]={"10","0"};
    const char *expect2[]={"1","15"};
    const char *expect3[]={"0","16"};
    BigIntArray denom{},stock{},answer{};
    BigInt amount{nullptr},covered{nullptr};
    check_int(crear_arreglo(&denom,d,2),"stock nearest two denom setup");
    check_int(bigint_init(&amount,"500001"),"stock nearest target");

    check_int(crear_arreglo(&stock,s1,2),"stock nearest 2000 available coins");
    check_int(calcular_cambio_cercano_stock_con_rango(&amount,&denom,&stock,
        0,std::numeric_limits<std::size_t>::max(),&covered,&answer),
        "closest with stock>512 and two denominations");
    check_int(covered.digits && std::string(covered.digits)=="500000",
        "closest with stock>512 covers 500000");
    check_arreglo_texto(&answer,expect1,2,"nearest large stock minimal count");
    bigint_array_free(&answer);bigint_free(&covered);bigint_array_free(&stock);

    check_int(crear_arreglo(&stock,s2,2),"stock nearest high capped");
    check_int(calcular_cambio_cercano_stock_con_rango(&amount,&denom,&stock,
        0,std::numeric_limits<std::size_t>::max(),&covered,&answer),
        "closest with stock>512 and one high denomination");
    check_int(covered.digits && std::string(covered.digits)=="499985",
        "closest high capped covers 499985");
    check_arreglo_texto(&answer,expect2,2,"nearest high capped minimal count");
    bigint_array_free(&answer);bigint_free(&covered);bigint_array_free(&stock);

    check_int(crear_arreglo(&stock,s3,2),"stock nearest high exhausted");
    check_int(calcular_cambio_cercano_stock_con_rango(&amount,&denom,&stock,
        0,std::numeric_limits<std::size_t>::max(),&covered,&answer),
        "closest with high denomination exhausted");
    check_int(covered.digits && std::string(covered.digits)=="479984",
        "closest high exhausted covers 479984");
    check_arreglo_texto(&answer,expect3,2,"nearest high exhausted counts");
    bigint_array_free(&answer);bigint_free(&covered);bigint_array_free(&stock);
    bigint_free(&amount);bigint_array_free(&denom);
}


static void test_five_piece_large_noncanonical_gcd_one(void)
{
    // Four coins cannot pay 3,500,000 because each is at most 800,001.
    // Five 700,000-unit coins are therefore a globally optimal witness.
    const char *d[]={"800001","700000","100001","1"};
    const char *st[]={"100","5","100","1000000"};
    const char *expected[]={"0","5","0","0"};
    BigInt amount{nullptr};
    BigIntArray denom{},stock{},out{};
    check_int(bigint_init(&amount,"3500000"),"five-piece target");
    check_int(crear_arreglo(&denom,d,4),"five-piece denominations");
    check_int(calcular_cambio_optimo(&amount,&denom,&out),
              "five-piece gcd-one unbounded solution exists");
    check_arreglo_texto(&out,expected,4,
                        "five-piece optimum must beat BigInt greedy fallback");
    bigint_array_free(&out);
    check_int(crear_arreglo(&stock,st,4),"five-piece finite inventory");
    check_int(calcular_cambio_optimo_stock(&amount,&denom,&stock,&out),
              "five-piece exact solution with stock");
    check_arreglo_texto(&out,expected,4,"five-piece stock optimum");
    bigint_array_free(&out);
    check_int(calcular_cambio_optimo_con_limite(&amount,&denom,5,&out),
              "five-piece constrained exact solution");
    check_arreglo_texto(&out,expected,4,"five-piece maximum respected");
    bigint_array_free(&out);
    bigint_array_free(&denom);
    bigint_array_free(&stock);
    bigint_free(&amount);
}

static void test_nine_and_twelve_piece_gcd_one(void)
{
    // Every coin except 700000 contributes residue 1 modulo 100000.
    // For at most 12 coins and a multiple of 100000, all coins must
    // therefore be 700000, proving both optima.
    const char *d[]={"800001","700000","100001","1"};
    const char *st[]={"100","12","100","100000"};
    const char *nine[]={"0","9","0","0"};
    const char *twelve[]={"0","12","0","0"};
    BigInt amount{nullptr};
    BigIntArray denom{},stock{},result{};
    check_int(crear_arreglo(&denom,d,4),"nine-piece denominations");
    check_int(crear_arreglo(&stock,st,4),"nine-piece stock");
    check_int(bigint_init(&amount,"6300000"),"nine-piece amount");
    check_int(calcular_cambio_optimo(&amount,&denom,&result),
              "nine-piece BigInt change exists");
    check_arreglo_texto(&result,nine,4,"nine-piece globally minimal");
    bigint_array_free(&result);
    check_int(calcular_cambio_optimo_stock(&amount,&denom,&stock,&result),
              "nine-piece bounded BigInt change exists");
    check_arreglo_texto(&result,nine,4,"nine-piece stock optimum");
    bigint_array_free(&result);
    check_int(calcular_cambio_optimo_con_limite(&amount,&denom,9,&result),
              "nine-piece maximum respected");
    check_arreglo_texto(&result,nine,4,"nine-piece bounded count");
    bigint_array_free(&result);
    bigint_free(&amount);
    check_int(bigint_init(&amount,"8400000"),"twelve-piece amount");
    check_int(calcular_cambio_optimo(&amount,&denom,&result),
              "twelve-piece BigInt change exists");
    check_arreglo_texto(&result,twelve,4,"twelve-piece globally minimal");
    bigint_array_free(&result);
    check_int(calcular_cambio_optimo_stock(&amount,&denom,&stock,&result),
              "twelve-piece bounded BigInt change exists");
    check_arreglo_texto(&result,twelve,4,"twelve-piece stock optimum");
    bigint_array_free(&result);
    bigint_array_free(&denom);
    bigint_array_free(&stock);
    bigint_free(&amount);
}

static void test_thirteen_and_twenty_piece_gcd_one(void)
{
    // For <=20 pieces, only the 700000 denomination is 0 mod 100000.
    // The remaining coin types contribute residue 1 and cannot appear
    // in any solution for a multiple of 100000.
    const char *d[]={"800001","700000","100001","1"};
    const char *st[]={"100","20","100","100000"};
    const char *thirteen[]={"0","13","0","0"};
    const char *twenty[]={"0","20","0","0"};
    BigInt amount{nullptr};
    BigIntArray denom{},stock{},out{};
    check_int(crear_arreglo(&denom,d,4),"thirteen-piece denominations");
    check_int(crear_arreglo(&stock,st,4),"thirteen-piece stock");
    check_int(bigint_init(&amount,"9100000"),"thirteen-piece amount");
    check_int(calcular_cambio_optimo(&amount,&denom,&out),
              "thirteen-piece BigInt unlimited change");
    check_arreglo_texto(&out,thirteen,4,"thirteen-piece certified optimum");
    bigint_array_free(&out);
    check_int(calcular_cambio_optimo_stock(&amount,&denom,&stock,&out),
              "thirteen-piece finite stock");
    check_arreglo_texto(&out,thirteen,4,"thirteen-piece stock optimum");
    bigint_array_free(&out);
    check_int(calcular_cambio_optimo_con_limite(&amount,&denom,13,&out),
              "thirteen-piece maximum");
    check_arreglo_texto(&out,thirteen,4,"thirteen-piece max respected");
    bigint_array_free(&out);
    bigint_free(&amount);

    check_int(bigint_init(&amount,"14000000"),"twenty-piece amount");
    check_int(calcular_cambio_optimo(&amount,&denom,&out),
              "twenty-piece BigInt unlimited change");
    check_arreglo_texto(&out,twenty,4,"twenty-piece certified optimum");
    bigint_array_free(&out);
    check_int(calcular_cambio_optimo_stock(&amount,&denom,&stock,&out),
              "twenty-piece finite stock");
    check_arreglo_texto(&out,twenty,4,"twenty-piece stock optimum");
    bigint_array_free(&out);
    check_int(calcular_cambio_optimo_con_limite(&amount,&denom,20,&out),
              "twenty-piece maximum");
    check_arreglo_texto(&out,twenty,4,"twenty-piece max respected");
    bigint_array_free(&out);
    bigint_free(&amount);
    bigint_array_free(&denom);
    bigint_array_free(&stock);
}

static void test_large_nearest_at_most_two_coins(void)
{
    const char *d[]={"800000","700000","400000"};
    const char *st[]={"0","1","2"};
    const char *expected[]={"1","0","1"};
    const char *expectedStock[]={"0","1","1"};
    BigInt amount{nullptr}, covered{nullptr};
    BigIntArray denom{},stock{},out{};
    check_int(bigint_init(&amount,"1200001"),"near two big amount");
    check_int(crear_arreglo(&denom,d,3),"near two big denoms");
    check_int(calcular_cambio_cercano_con_rango(&amount,&denom,0,2,&covered,&out),
              "near two big non-exact solution");
    check_int(covered.digits && std::string(covered.digits)=="1200000",
              "near two big amount covered");
    check_arreglo_texto(&out,expected,3,"near two big optimum counts");
    bigint_array_free(&out); bigint_free(&covered);
    check_int(crear_arreglo(&stock,st,3),"near two big stock");
    check_int(calcular_cambio_cercano_stock_con_rango(&amount,&denom,&stock,
              0,2,&covered,&out),"near two big limited stock");
    check_int(covered.digits && std::string(covered.digits)=="1100000",
              "near two big stock covered");
    check_arreglo_texto(&out,expectedStock,3,"near two big stock counts");
    bigint_array_free(&out); bigint_free(&covered);
    bigint_array_free(&stock); bigint_array_free(&denom); bigint_free(&amount);

    const char *d2[]={"600000","350000","200000"};
    const char *s2[]={"0","1","1"};
    const char *expect2[]={"0","1","1"};
    check_int(bigint_init(&amount,"700001"),"nearest no double amount");
    check_int(crear_arreglo(&denom,d2,3),"nearest no double denoms");
    check_int(crear_arreglo(&stock,s2,3),"nearest no double stock");
    check_int(calcular_cambio_cercano_stock_con_rango(&amount,&denom,&stock,
              0,2,&covered,&out),"nearest cannot spend unavailable second coin");
    check_int(covered.digits && std::string(covered.digits)=="550000",
              "nearest cannot double a single physical coin");
    check_arreglo_texto(&out,expect2,3,"nearest single-stock bound");
    bigint_array_free(&out); bigint_free(&covered); bigint_array_free(&stock);
    bigint_array_free(&denom); bigint_free(&amount);

    const char *d3[]={"800000","400000","200000"};
    const char *expect3[]={"0","2","0"};
    check_int(bigint_init(&amount,"800001"),"nearest minimum amount");
    check_int(crear_arreglo(&denom,d3,3),"nearest minimum denom");
    check_int(calcular_cambio_cercano_con_rango(&amount,&denom,2,2,&covered,&out),
              "nearest exactly two pieces");
    check_int(covered.digits && std::string(covered.digits)=="800000",
              "nearest exactly two amount");
    check_arreglo_texto(&out,expect3,3,"nearest exactly two pieces counts");
    bigint_array_free(&out); bigint_free(&covered);
    bigint_array_free(&denom); bigint_free(&amount);
}

static void test_large_nearest_minimum_two_denominations(void)
{
    const char *d[]={"50000","49999"};
    const char *expected[]={"5","0"};
    BigInt amount{nullptr},covered{nullptr};
    BigIntArray denom{},out{};
    check_int(crear_arreglo(&denom,d,2),"nearest minimum types");
    check_int(bigint_init(&amount,"250001"),"nearest minimum amount");
    check_int(calcular_cambio_cercano_con_rango(&amount,&denom,5,
              std::numeric_limits<std::size_t>::max(),&covered,&out),
              "nearest with minimum five pieces must return best lower sum");
    check_int(covered.digits && std::string(covered.digits)=="250000",
              "nearest with minimum five pieces coverage");
    check_arreglo_texto(&out,expected,2,"nearest with minimum five pieces counts");
    bigint_array_free(&out);bigint_free(&covered);bigint_free(&amount);
    check_int(bigint_init(&amount,"200000"),"nearest impossible minimum");
    check_int(!calcular_cambio_cercano_con_rango(&amount,&denom,5,
              std::numeric_limits<std::size_t>::max(),&covered,&out),
              "nearest minimum five cannot fit below five times smallest");
    bigint_free(&amount);
    check_int(bigint_init(&amount,"250000"),"nearest exact minimum amount");
    check_int(calcular_cambio_cercano_con_rango(&amount,&denom,5,
              std::numeric_limits<std::size_t>::max(),&covered,&out),
              "nearest minimum should retain exact sum");
    check_int(covered.digits && std::string(covered.digits)=="250000",
              "nearest exact minimum covered");
    check_arreglo_texto(&out,expected,2,"nearest exact minimum counts");
    bigint_array_free(&out);bigint_free(&covered);bigint_free(&amount);
    bigint_array_free(&denom);
}

int main(void)
{
    test_large_nearest_minimum_two_denominations();
    test_large_nearest_at_most_two_coins();
    test_thirteen_and_twenty_piece_gcd_one();
    test_nine_and_twelve_piece_gcd_one();
    test_five_piece_large_noncanonical_gcd_one();
    test_nearest_bigint_two_denominations_stock_large();
    test_nearest_coprime_bigint_two_denominations();
    test_nearest_bigint_two_denominations();
    test_sparse_limit_not_false_impossibility();
    test_bigint_unsorted_large_greedy_fallback();
    test_large_noncanonical_gcd1_small_coin_witness();
    test_medium_exact_two_nonminimal_range();
    test_nearest_single_bigint_boundary();
    test_sparse_large_inventory();
    test_more_than_size_t_max_coins();
    test_bigint_two_denominations();
    test_noncanonical_large_residue();
    test_near_high_amount();
    differential_property_tests();
    test_rangos_stock_bigint();
    test_cambio_no_canonico_ilimitado();

    if (fallos != 0)
    {
        printf("%d prueba(s) fallaron.\n", fallos);
        return 1;
    }

    printf("Todas las pruebas pasaron.\n");
    return 0;
}
