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
int main(void)
{
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
