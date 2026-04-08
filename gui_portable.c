#ifndef _WIN32

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bigint.h"
#include "moneda_gestion.h"

#define MAX_MONEDAS 64
#define MAX_NOMBRE 64

static int leer_linea(char *buffer, size_t tam)
{
    if (fgets(buffer, (int)tam, stdin) == NULL)
        return 0;

    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

static void a_minusculas(char *texto)
{
    size_t i;

    if (texto == NULL)
        return;

    for (i = 0; texto[i] != '\0'; i++)
        texto[i] = (char)tolower((unsigned char)texto[i]);
}

static int es_numero(const char *s)
{
    size_t i;

    if (s == NULL || *s == '\0')
        return 0;

    for (i = 0; s[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }

    return 1;
}

static int cargar_nombres_moneda(char monedas[MAX_MONEDAS][MAX_NOMBRE], int *cantidad)
{
    FILE *fp = fopen("monedas.txt", "r");
    char token[256];

    if (fp == NULL || cantidad == NULL)
        return 0;

    *cantidad = 0;

    while (fscanf(fp, "%255s", token) == 1)
    {
        int existe = 0;
        if (es_numero(token))
            continue;

        for (int i = 0; i < *cantidad; i++)
        {
            if (strcmp(monedas[i], token) == 0)
            {
                existe = 1;
                break;
            }
        }

        if (!existe && *cantidad < MAX_MONEDAS)
        {
            size_t n = strlen(token);
            if (n >= MAX_NOMBRE)
                n = MAX_NOMBRE - 1;
            memcpy(monedas[*cantidad], token, n);
            monedas[*cantidad][n] = '\0';
            (*cantidad)++;
        }
    }

    fclose(fp);
    return *cantidad > 0;
}

static void imprimir_panel(const char *moneda, const BigIntArray *denom, const BigIntArray *stock)
{
    printf("\n==============================================\n");
    printf("Panel Administrador (portable) - moneda: %s\n", moneda);
    printf("==============================================\n");
    for (size_t i = 0; i < denom->len; i++)
        printf("[%zu] Denom %s c -> stock %s\n", i + 1, denom->items[i].digits, stock->items[i].digits);
    printf("==============================================\n");
}

static int pedir_indice(size_t max, size_t *indice)
{
    char buffer[64];
    char *fin;
    long v;

    printf("Indice (1-%zu): ", max);
    if (!leer_linea(buffer, sizeof(buffer)))
        return -1;

    v = strtol(buffer, &fin, 10);
    if (*fin != '\0' || v < 1 || (size_t)v > max)
        return 0;

    *indice = (size_t)(v - 1);
    return 1;
}

static int pedir_cantidad(BigInt *delta)
{
    char buffer[2048];
    BigInt tmp = {0};

    printf("Cantidad (entero no negativo): ");
    if (!leer_linea(buffer, sizeof(buffer)))
        return -1;

    if (!bigint_init(&tmp, buffer))
        return 0;

    bigint_free(delta);
    *delta = tmp;
    return 1;
}

int main(void)
{
#if defined(__APPLE__)
    const char *so = "macOS";
#elif defined(__linux__)
    const char *so = "Linux";
#elif defined(__FreeBSD__)
    const char *so = "FreeBSD";
#elif defined(__OpenBSD__)
    const char *so = "OpenBSD";
#elif defined(__NetBSD__)
    const char *so = "NetBSD";
#elif defined(__sun)
    const char *so = "Solaris";
#else
    const char *so = "Unix-like";
#endif
    char monedas[MAX_MONEDAS][MAX_NOMBRE];
    int nMonedas = 0;

    printf("ProgVoraz GUI portable iniciado en %s\n", so);
    printf("Nota: en este SO se usa panel administrador en terminal (sin WinAPI).\n");

    if (!cargar_nombres_moneda(monedas, &nMonedas))
    {
        printf("No se pudieron cargar monedas desde monedas.txt\n");
        return 1;
    }

    while (1)
    {
        char entrada[128];
        char cmd[128];
        BigIntArray denom = {0};
        BigIntArray stock = {0};
        int idxMoneda = -1;

        printf("\nMonedas disponibles:\n");
        for (int i = 0; i < nMonedas; i++)
            printf("  [%d] %s\n", i + 1, monedas[i]);

        printf("Selecciona moneda por indice o nombre (o salir): ");
        if (!leer_linea(entrada, sizeof(entrada)))
            break;

        strncpy(cmd, entrada, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        a_minusculas(cmd);
        if (strcmp(cmd, "salir") == 0)
            break;

        {
            char *fin;
            long v = strtol(entrada, &fin, 10);
            if (*fin == '\0' && v >= 1 && v <= nMonedas)
                idxMoneda = (int)(v - 1);
        }

        if (idxMoneda < 0)
        {
            for (int i = 0; i < nMonedas; i++)
            {
                if (strcmp(monedas[i], entrada) == 0)
                {
                    idxMoneda = i;
                    break;
                }
            }
        }

        if (idxMoneda < 0)
        {
            printf("Moneda invalida.\n");
            continue;
        }

        if (!cargar_denominaciones_moneda(monedas[idxMoneda], &denom) || !cargar_stock_moneda(monedas[idxMoneda], &stock) || denom.len != stock.len)
        {
            printf("No se pudo cargar denominaciones/stock para esa moneda.\n");
            bigint_array_free(&denom);
            bigint_array_free(&stock);
            continue;
        }

        while (1)
        {
            char accion[32];
            size_t idxDen = 0;
            BigInt delta = {0};
            BigInt nuevo = {0};
            int esSuma;

            imprimir_panel(monedas[idxMoneda], &denom, &stock);
            printf("Accion (anadir/quitar/volver/salir): ");
            if (!leer_linea(accion, sizeof(accion)))
            {
                bigint_free(&delta);
                bigint_array_free(&denom);
                bigint_array_free(&stock);
                return 0;
            }

            a_minusculas(accion);
            if (strcmp(accion, "salir") == 0)
            {
                bigint_free(&delta);
                bigint_array_free(&denom);
                bigint_array_free(&stock);
                return 0;
            }
            if (strcmp(accion, "volver") == 0)
            {
                bigint_free(&delta);
                break;
            }

            if (strcmp(accion, "anadir") == 0)
                esSuma = 1;
            else if (strcmp(accion, "quitar") == 0)
                esSuma = 0;
            else
            {
                printf("Accion invalida.\n");
                bigint_free(&delta);
                continue;
            }

            if (pedir_indice(denom.len, &idxDen) <= 0)
            {
                printf("Indice invalido.\n");
                bigint_free(&delta);
                continue;
            }

            if (pedir_cantidad(&delta) <= 0)
            {
                printf("Cantidad invalida.\n");
                bigint_free(&delta);
                continue;
            }

            if (esSuma)
            {
                if (!bigint_add(&stock.items[idxDen], &delta, &nuevo))
                {
                    printf("No se pudo sumar stock.\n");
                    bigint_free(&delta);
                    bigint_free(&nuevo);
                    continue;
                }
            }
            else
            {
                if (bigint_compare(&stock.items[idxDen], &delta) < 0 || !bigint_subtract(&stock.items[idxDen], &delta, &nuevo))
                {
                    printf("No se pudo quitar stock (insuficiente o error).\n");
                    bigint_free(&delta);
                    bigint_free(&nuevo);
                    continue;
                }
            }

            if (!bigint_array_set(&stock, idxDen, &nuevo))
            {
                printf("No se pudo aplicar cambio en memoria.\n");
                bigint_free(&delta);
                bigint_free(&nuevo);
                continue;
            }

            if (!actualizar_stock_moneda(monedas[idxMoneda], &stock))
                printf("No se pudo persistir stock en archivo.\n");
            else
                printf("Stock actualizado correctamente.\n");

            bigint_free(&delta);
            bigint_free(&nuevo);
        }

        bigint_array_free(&denom);
        bigint_array_free(&stock);
    }

    return 0;
}

#else
int main(void)
{
    return 0;
}
#endif
