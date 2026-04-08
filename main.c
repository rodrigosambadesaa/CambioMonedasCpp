/*
 * main.c
 *
 * PROPOSITO GENERAL
 * -----------------
 * Aplicacion de cambio de monedas con soporte para enteros de tamano arbitrario.
 * Toda la aritmetica de montos, denominaciones, stock y cantidades se realiza
 * mediante BigInt (enteros no negativos sin limite practico de digitos).
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bigint.h"
#include "monedagestion.h"

#define MAX_MONEDA_NOMBRE 20

static void limpiarPantalla(void)
{
    printf("\033[2J\033[H");
}

static void dibujarLinea(void)
{
    printf("+--------------------------------------------------+\n");
}

static void dibujarTitulo(void)
{
    limpiarPantalla();
    dibujarLinea();
    printf("|          SISTEMA DE CAMBIO DE MONEDAS            |\n");
    dibujarLinea();
}

static int leerLinea(char *buffer, size_t tam)
{
    if (fgets(buffer, (int)tam, stdin) == NULL)
        return 0;

    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

static void aMinusculas(char *texto)
{
    size_t i;

    if (texto == NULL)
        return;

    for (i = 0; texto[i] != '\0'; i++)
        texto[i] = (char)tolower((unsigned char)texto[i]);
}

/*
 * Normaliza texto de clave para comparacion con archivos:
 * - pasa a minusculas
 * - elimina acentos comunes (UTF-8 y Latin-1/CP1252)
 */
static void normalizarClave(const char *origen, char *destino, size_t tamDestino)
{
    size_t i = 0;
    size_t j = 0;

    if (destino == NULL || tamDestino == 0)
        return;

    destino[0] = '\0';
    if (origen == NULL)
        return;

    while (origen[i] != '\0' && j + 1 < tamDestino)
    {
        unsigned char c = (unsigned char)origen[i];

        if (c == 0xC3 && origen[i + 1] != '\0')
        {
            unsigned char s = (unsigned char)origen[i + 1];
            char reemplazo = '\0';

            if (s == 0xA1 || s == 0x81)
                reemplazo = 'a';
            else if (s == 0xA9 || s == 0x89)
                reemplazo = 'e';
            else if (s == 0xAD || s == 0x8D)
                reemplazo = 'i';
            else if (s == 0xB3 || s == 0x93)
                reemplazo = 'o';
            else if (s == 0xBA || s == 0x9A || s == 0xBC || s == 0x9C)
                reemplazo = 'u';
            else if (s == 0xB1 || s == 0x91)
                reemplazo = 'n';

            if (reemplazo != '\0')
            {
                destino[j++] = reemplazo;
                i += 2;
                continue;
            }
        }

        if (c == 0xE1 || c == 0xC1 || c == 0xA0)
            destino[j++] = 'a';
        else if (c == 0xE9 || c == 0xC9 || c == 0x82 || c == 0x90)
            destino[j++] = 'e';
        else if (c == 0xED || c == 0xCD || c == 0xA1 || c == 0xD6)
            destino[j++] = 'i';
        else if (c == 0xF3 || c == 0xD3 || c == 0xA2 || c == 0xE0)
            destino[j++] = 'o';
        else if (c == 0xFA || c == 0xDA || c == 0xFC || c == 0xDC || c == 0xA3 || c == 0x81 || c == 0x9A)
            destino[j++] = 'u';
        else if (c == 0xF1 || c == 0xD1 || c == 0xA4 || c == 0xA5)
            destino[j++] = 'n';
        else
            destino[j++] = (char)tolower(c);

        i++;
    }

    destino[j] = '\0';
}

/*
 * Muestra menu inicial y devuelve opcion normalizada en minuscula.
 * Retorna:
 * - 'a' o 'b' si la entrada es valida
 * - 0 si la entrada es invalida/vacia
 * - -1 si hubo EOF/error de lectura
 * - -2 si usuario pide salir
 */
static int pedirOpcion(void)
{
    char buffer[16];
    char comando[16];

    dibujarTitulo();
    printf("| Elija modo de trabajo:                            |\n");
    printf("|   a) Monedas infinitas                            |\n");
    printf("|   b) Monedas limitadas (usa stock)                |\n");
    dibujarLinea();
    printf("Opcion (o 'salir'): ");

    if (!leerLinea(buffer, sizeof(buffer)))
        return -1;

    if (buffer[0] == '\0')
        return 0;

    strncpy(comando, buffer, sizeof(comando) - 1);
    comando[sizeof(comando) - 1] = '\0';
    aMinusculas(comando);
    if (strcmp(comando, "salir") == 0)
        return -2;

    return (int)tolower((unsigned char)buffer[0]);
}

/*
 * Solicita monto y lo parsea como BigInt.
 * Retorna:
 * - 1 si lectura valida
 * - 0 si entrada invalida
 * - -1 si EOF/error
 * - 2 si usuario pide volver a seleccion de moneda ("0" o "volver")
 * - 3 si usuario pide salir
 */
static int pedirCantidad(BigInt *cantidad)
{
    char buffer[2048];
    char comando[2048];
    BigInt tmp = {0};

    printf("Cantidad en centimos (0, volver o salir): ");
    if (!leerLinea(buffer, sizeof(buffer)))
        return -1;

    if (buffer[0] == '\0')
        return 0;

    strncpy(comando, buffer, sizeof(comando) - 1);
    comando[sizeof(comando) - 1] = '\0';
    aMinusculas(comando);

    if (strcmp(comando, "salir") == 0)
        return 3;

    if (strcmp(comando, "volver") == 0 || strcmp(comando, "0") == 0)
        return 2;

    if (!bigint_init(&tmp, buffer))
        return 0;

    bigint_free(cantidad);
    *cantidad = tmp;
    return 1;
}

static int copiarArregloBigInt(const BigIntArray *origen, BigIntArray *destino)
{
    if (origen == NULL || destino == NULL)
        return 0;

    if (!bigint_array_create(destino, origen->len))
        return 0;

    for (size_t i = 0; i < origen->len; i++)
    {
        if (!bigint_array_set(destino, i, &origen->items[i]))
        {
            bigint_array_free(destino);
            return 0;
        }
    }

    return 1;
}

static void limpiarArreglo(BigIntArray *arr)
{
    if (arr != NULL)
        bigint_array_free(arr);
}

static int cambio(const BigInt *monto, const BigIntArray *denominaciones, BigIntArray *solucion)
{
    BigInt restante = {0};

    if (monto == NULL || denominaciones == NULL || solucion == NULL)
        return 0;

    if (!bigint_array_create(solucion, denominaciones->len))
        return 0;

    if (!bigint_set(&restante, monto) && !bigint_init(&restante, monto->digits))
    {
        limpiarArreglo(solucion);
        return 0;
    }

    for (size_t i = 0; i < denominaciones->len; i++)
    {
        BigInt q = {0};
        BigInt r = {0};

        if (bigint_is_zero(&denominaciones->items[i]))
            continue;

        if (!bigint_divmod(&restante, &denominaciones->items[i], &q, &r))
        {
            bigint_free(&q);
            bigint_free(&r);
            bigint_free(&restante);
            limpiarArreglo(solucion);
            return 0;
        }

        if (!bigint_array_set(solucion, i, &q))
        {
            bigint_free(&q);
            bigint_free(&r);
            bigint_free(&restante);
            limpiarArreglo(solucion);
            return 0;
        }

        bigint_free(&restante);
        restante = r;
        bigint_free(&q);

        if (bigint_is_zero(&restante))
            break;
    }

    {
        int exito = bigint_is_zero(&restante);
        bigint_free(&restante);
        return exito;
    }
}

static int cambioStock(const BigInt *monto, const BigIntArray *denominaciones, BigIntArray *solucion, BigIntArray *stock)
{
    BigInt restante = {0};
    BigIntArray stockTrabajo = {0};

    if (monto == NULL || denominaciones == NULL || solucion == NULL || stock == NULL)
        return 0;
    if (denominaciones->len != stock->len)
        return 0;

    if (!bigint_array_create(solucion, denominaciones->len))
        return 0;

    if (!copiarArregloBigInt(stock, &stockTrabajo))
    {
        limpiarArreglo(solucion);
        return 0;
    }

    if (!bigint_set(&restante, monto) && !bigint_init(&restante, monto->digits))
    {
        limpiarArreglo(solucion);
        limpiarArreglo(&stockTrabajo);
        return 0;
    }

    for (size_t i = 0; i < denominaciones->len; i++)
    {
        BigInt maxUsables = {0};
        BigInt r = {0};
        BigInt tomar = {0};

        if (bigint_is_zero(&denominaciones->items[i]))
            continue;

        if (!bigint_divmod(&restante, &denominaciones->items[i], &maxUsables, &r))
            goto fallo;

        if (bigint_compare(&maxUsables, &stockTrabajo.items[i]) > 0)
        {
            if (!bigint_set(&tomar, &stockTrabajo.items[i]) && !bigint_init(&tomar, stockTrabajo.items[i].digits))
                goto fallo;
        }
        else
        {
            if (!bigint_set(&tomar, &maxUsables) && !bigint_init(&tomar, maxUsables.digits))
                goto fallo;
        }

        if (!bigint_array_set(solucion, i, &tomar))
            goto fallo;

        if (!bigint_is_zero(&tomar))
        {
            BigInt uso = {0};
            BigInt nuevoRestante = {0};
            BigInt nuevoStock = {0};

            if (!bigint_multiply(&denominaciones->items[i], &tomar, &uso))
            {
                bigint_free(&uso);
                goto fallo;
            }

            if (!bigint_subtract(&restante, &uso, &nuevoRestante))
            {
                bigint_free(&uso);
                bigint_free(&nuevoRestante);
                goto fallo;
            }

            if (!bigint_subtract(&stockTrabajo.items[i], &tomar, &nuevoStock))
            {
                bigint_free(&uso);
                bigint_free(&nuevoRestante);
                bigint_free(&nuevoStock);
                goto fallo;
            }

            bigint_free(&restante);
            restante = nuevoRestante;

            if (!bigint_set(&stockTrabajo.items[i], &nuevoStock))
            {
                bigint_free(&uso);
                bigint_free(&nuevoStock);
                goto fallo;
            }

            bigint_free(&uso);
            bigint_free(&nuevoStock);
        }

        bigint_free(&maxUsables);
        bigint_free(&r);
        bigint_free(&tomar);

        if (bigint_is_zero(&restante))
            break;

        continue;

    fallo:
        bigint_free(&maxUsables);
        bigint_free(&r);
        bigint_free(&tomar);
        bigint_free(&restante);
        limpiarArreglo(solucion);
        limpiarArreglo(&stockTrabajo);
        return 0;
    }

    if (!bigint_is_zero(&restante))
    {
        bigint_free(&restante);
        limpiarArreglo(solucion);
        limpiarArreglo(&stockTrabajo);
        return 0;
    }

    for (size_t i = 0; i < stock->len; i++)
    {
        if (!bigint_array_set(stock, i, &stockTrabajo.items[i]))
        {
            bigint_free(&restante);
            limpiarArreglo(solucion);
            limpiarArreglo(&stockTrabajo);
            return 0;
        }
    }

    bigint_free(&restante);
    limpiarArreglo(&stockTrabajo);
    return 1;
}

static void imprimirResultado(const BigIntArray *monedas, const BigIntArray *solucion, const BigIntArray *stock, int usarStock)
{
    dibujarLinea();
    printf("Resultado del cambio\n");
    dibujarLinea();

    for (size_t i = 0; i < monedas->len; i++)
    {
        if (usarStock)
            printf("Moneda %s c  -> cantidad %s | stock %s\n", monedas->items[i].digits, solucion->items[i].digits, stock->items[i].digits);
        else
            printf("Moneda %s c  -> cantidad %s\n", monedas->items[i].digits, solucion->items[i].digits);
    }

    dibujarLinea();
}

int main(void)
{
    char moneda[MAX_MONEDA_NOMBRE + 1];
    char monedaClave[MAX_MONEDA_NOMBRE + 1];
    char monedaCmd[MAX_MONEDA_NOMBRE + 1];
    int opcion = 0;
    int ejecutando = 1;
    BigIntArray monedas = {0};
    BigIntArray stock = {0};

    while (ejecutando)
    {
        if (opcion != 'a' && opcion != 'b')
        {
            while (1)
            {
                opcion = pedirOpcion();
                if (opcion == 'a' || opcion == 'b')
                    break;

                if (opcion == -2)
                {
                    ejecutando = 0;
                    break;
                }

                if (opcion == -1)
                {
                    printf("Entrada finalizada.\n");
                    ejecutando = 0;
                    break;
                }

                printf("Opcion no valida. Intente de nuevo.\n");
            }
        }

        if (!ejecutando)
            break;

        while (1)
        {
            limpiarArreglo(&stock);
            limpiarArreglo(&monedas);

            printf("Nombre de la moneda (ej: euro, dolar, yen), 'modo' o 'salir': ");
            if (!leerLinea(moneda, sizeof(moneda)))
            {
                printf("Entrada finalizada.\n");
                ejecutando = 0;
                break;
            }

            if (moneda[0] == '\0')
            {
                printf("Nombre de moneda no valido. Intente de nuevo.\n");
                continue;
            }

            normalizarClave(moneda, monedaCmd, sizeof(monedaCmd));
            if (strcmp(monedaCmd, "salir") == 0)
            {
                ejecutando = 0;
                break;
            }

            if (strcmp(monedaCmd, "modo") == 0)
            {
                opcion = 0;
                break;
            }

            normalizarClave(moneda, monedaClave, sizeof(monedaClave));

            if (!cargarDenominacionesMoneda(monedaClave, &monedas))
            {
                printf("No se encontro la moneda en monedas.txt. Intente de nuevo.\n");
                continue;
            }

            if (opcion == 'b')
            {
                if (!cargarStockMoneda(monedaClave, &stock))
                {
                    limpiarArreglo(&monedas);
                    printf("No se encontro stock para la moneda seleccionada. Intente de nuevo.\n");
                    continue;
                }

                if (stock.len != monedas.len)
                {
                    limpiarArreglo(&stock);
                    limpiarArreglo(&monedas);
                    printf("Inconsistencia entre denominaciones y stock. Intente otra moneda.\n");
                    continue;
                }
            }

            while (1)
            {
                BigInt cantidad = {0};
                int estadoCantidad;
                int resultado;
                BigIntArray solucion = {0};

                estadoCantidad = pedirCantidad(&cantidad);
                if (estadoCantidad == -1)
                {
                    printf("Entrada finalizada.\n");
                    bigint_free(&cantidad);
                    ejecutando = 0;
                    break;
                }

                if (estadoCantidad == 2)
                {
                    bigint_free(&cantidad);
                    break;
                }

                if (estadoCantidad == 3)
                {
                    bigint_free(&cantidad);
                    ejecutando = 0;
                    break;
                }

                if (estadoCantidad == 0)
                {
                    printf("Entrada invalida. Introduzca un entero no negativo.\n");
                    bigint_free(&cantidad);
                    continue;
                }

                if (opcion == 'a')
                {
                    resultado = cambio(&cantidad, &monedas, &solucion);
                    if (resultado)
                        imprimirResultado(&monedas, &solucion, NULL, 0);
                    else
                        printf("No existe cambio exacto para esa cantidad.\n");
                }
                else
                {
                    resultado = cambioStock(&cantidad, &monedas, &solucion, &stock);
                    if (resultado)
                    {
                        imprimirResultado(&monedas, &solucion, &stock, 1);
                        if (!actualizarStockMoneda(monedaClave, &stock))
                            printf("No se pudo actualizar el archivo de stock.\n");
                    }
                    else
                    {
                        printf("No existe cambio exacto para esa cantidad con el stock actual.\n");
                    }
                }

                limpiarArreglo(&solucion);
                bigint_free(&cantidad);
            }

            if (!ejecutando)
                break;

            /* Vuelve a seleccionar moneda dentro del mismo modo. */
            break;
        }

        if (!ejecutando)
            break;

        /* Si opcion se reinicio a 0, el usuario pidio cambiar de modo. */
        if (opcion == 0)
            continue;
    }

    limpiarArreglo(&stock);
    limpiarArreglo(&monedas);
    printf("Gracias por utilizar este programa.\n");
    return EXIT_SUCCESS;
}
