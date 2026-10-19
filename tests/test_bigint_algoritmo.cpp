#include <stdio.h>
#include <string.h>
#include "bigint.h"
#include "algoritmo_cambio.h"

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

int main(void)
{
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
