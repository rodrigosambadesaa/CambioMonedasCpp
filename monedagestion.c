#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif
#include "monedagestion.h"

#define MAX_TOKEN 256
#define MAX_DENOMINACIONES 64
#define MAX_LINEA_STOCK 256
#define MAX_LINEAS_STOCK 512

static int esTextoBigIntValido(const char *texto)
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

static char *duplicarCadena(const char *texto)
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

static void recortarFinLinea(char *texto)
{
    if (texto == NULL)
        return;

    texto[strcspn(texto, "\r\n")] = '\0';
}

static int cargarDatosMoneda(const char *archivo, const char *nombreMoneda, int convertirMenosUnoACero, BigIntArray *resultado)
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

        if (!esTextoBigIntValido(token))
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

int cargarDenominacionesMoneda(const char *nombreMoneda, BigIntArray *resultado)
{
    return cargarDatosMoneda("monedas.txt", nombreMoneda, 0, resultado);
}

int cargarStockMoneda(const char *nombreMoneda, BigIntArray *resultado)
{
    return cargarDatosMoneda("stock.txt", nombreMoneda, 1, resultado);
}

int actualizarStockMoneda(const char *nombreMoneda, const BigIntArray *stock)
{
    FILE *archivo = fopen("stock.txt", "r+");
    char *lineas[MAX_LINEAS_STOCK];
    char buffer[MAX_LINEA_STOCK];
    char comparable[MAX_LINEA_STOCK];
    size_t i;
    size_t totalLineas = 0;
    int ok = 1;
    int actualizado = 0;

    if (archivo == NULL || stock == NULL || stock->items == NULL)
        return 0;

    for (i = 0; i < MAX_LINEAS_STOCK; i++)
        lineas[i] = NULL;

    while (fgets(buffer, sizeof(buffer), archivo) != NULL)
    {
        size_t len = strlen(buffer);

        if (totalLineas >= MAX_LINEAS_STOCK)
        {
            ok = 0;
            break;
        }

        if (len > 0 && buffer[len - 1] != '\n' && !feof(archivo))
        {
            ok = 0;
            break;
        }

        lineas[totalLineas] = duplicarCadena(buffer);
        if (lineas[totalLineas] == NULL)
        {
            ok = 0;
            break;
        }
        totalLineas++;
    }

    if (ok)
    {
        for (size_t linea = 0; linea < totalLineas && !actualizado; linea++)
        {
            strncpy(comparable, lineas[linea], sizeof(comparable) - 1);
            comparable[sizeof(comparable) - 1] = '\0';
            recortarFinLinea(comparable);

            if (strcmp(comparable, nombreMoneda) != 0)
                continue;

            if (stock->len > totalLineas - (linea + 1))
            {
                ok = 0;
                break;
            }

            for (i = 0; i < stock->len; i++)
            {
                size_t objetivo = linea + 1 + i;
                char nuevaLinea[MAX_LINEA_STOCK];

                if (bigint_is_zero(&stock->items[i]))
                    snprintf(nuevaLinea, sizeof(nuevaLinea), "-1\n");
                else
                    snprintf(nuevaLinea, sizeof(nuevaLinea), "%s\n", stock->items[i].digits);

                free(lineas[objetivo]);
                lineas[objetivo] = duplicarCadena(nuevaLinea);
                if (lineas[objetivo] == NULL)
                {
                    ok = 0;
                    break;
                }
            }

            if (ok)
                actualizado = 1;
        }
    }

    if (!ok || !actualizado)
    {
        for (i = 0; i < totalLineas; i++)
            free(lineas[i]);
        fclose(archivo);
        return 0;
    }

    rewind(archivo);
    for (i = 0; i < totalLineas; i++)
    {
        if (fputs(lineas[i], archivo) == EOF)
        {
            ok = 0;
            break;
        }
    }

    if (ok && fflush(archivo) != 0)
        ok = 0;

    if (ok)
    {
        long fin = ftell(archivo);
        if (fin < 0)
        {
            ok = 0;
        }
        else
        {
#ifdef _WIN32
            if (_chsize_s(_fileno(archivo), (__int64)fin) != 0)
                ok = 0;
#else
            if (ftruncate(fileno(archivo), (off_t)fin) != 0)
                ok = 0;
#endif
        }
    }

    for (i = 0; i < totalLineas; i++)
        free(lineas[i]);
    fclose(archivo);

    return ok ? 1 : 0;
}
