#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "bigint.h"

static char *duplicar(const char *s)
{
    size_t len;
    char *copia;

    if (s == NULL)
        return NULL;

    len = strlen(s) + 1;
    copia = (char *)malloc(len);
    if (copia == NULL)
        return NULL;

    memcpy(copia, s, len);
    return copia;
}

static int texto_valido_entero_no_negativo(const char *text)
{
    size_t i;

    if (text == NULL || *text == '\0')
        return 0;

    for (i = 0; text[i] != '\0'; i++)
    {
        if (!isdigit((unsigned char)text[i]))
            return 0;
    }

    return 1;
}

static char *normalizar(const char *text)
{
    size_t i = 0;

    while (text[i] == '0' && text[i + 1] != '\0')
        i++;

    return duplicar(text + i);
}

int bigint_init(BigInt *n, const char *text)
{
    char *normalizado;

    if (n == NULL || !texto_valido_entero_no_negativo(text))
        return 0;

    normalizado = normalizar(text);
    if (normalizado == NULL)
        return 0;

    n->digits = normalizado;
    return 1;
}

int bigint_set(BigInt *dest, const BigInt *src)
{
    char *copia;

    if (dest == NULL || src == NULL || src->digits == NULL)
        return 0;

    copia = duplicar(src->digits);
    if (copia == NULL)
        return 0;

    free(dest->digits);
    dest->digits = copia;
    return 1;
}

void bigint_free(BigInt *n)
{
    if (n == NULL)
        return;

    free(n->digits);
    n->digits = NULL;
}

int bigint_compare(const BigInt *a, const BigInt *b)
{
    size_t la;
    size_t lb;
    int cmp;

    if (a == NULL || b == NULL || a->digits == NULL || b->digits == NULL)
        return 0;

    la = strlen(a->digits);
    lb = strlen(b->digits);

    if (la < lb)
        return -1;
    if (la > lb)
        return 1;

    cmp = strcmp(a->digits, b->digits);
    if (cmp < 0)
        return -1;
    if (cmp > 0)
        return 1;
    return 0;
}

int bigint_is_zero(const BigInt *n)
{
    return n != NULL && n->digits != NULL && strcmp(n->digits, "0") == 0;
}

int bigint_add(const BigInt *a, const BigInt *b, BigInt *out)
{
    size_t la;
    size_t lb;
    size_t maxlen;
    char *tmp;
    int carry = 0;

    if (a == NULL || b == NULL || out == NULL || a->digits == NULL || b->digits == NULL)
        return 0;

    la = strlen(a->digits);
    lb = strlen(b->digits);
    maxlen = la > lb ? la : lb;

    tmp = (char *)malloc(maxlen + 2);
    if (tmp == NULL)
        return 0;

    tmp[maxlen + 1] = '\0';

    for (size_t k = 0; k < maxlen; k++)
    {
        int da = (la > k) ? (a->digits[la - 1 - k] - '0') : 0;
        int db = (lb > k) ? (b->digits[lb - 1 - k] - '0') : 0;
        int s = da + db + carry;
        tmp[maxlen - k] = (char)('0' + (s % 10));
        carry = s / 10;
    }

    tmp[0] = (char)('0' + carry);

    {
        BigInt res = {0};
        char *norm = carry ? duplicar(tmp) : duplicar(tmp + 1);
        free(tmp);
        if (norm == NULL)
            return 0;
        res.digits = norm;
        bigint_free(out);
        *out = res;
    }

    return 1;
}

int bigint_subtract(const BigInt *a, const BigInt *b, BigInt *out)
{
    size_t la;
    size_t lb;
    char *tmp;
    int borrow = 0;

    if (a == NULL || b == NULL || out == NULL || a->digits == NULL || b->digits == NULL)
        return 0;

    if (bigint_compare(a, b) < 0)
        return 0;

    la = strlen(a->digits);
    lb = strlen(b->digits);

    tmp = (char *)malloc(la + 1);
    if (tmp == NULL)
        return 0;

    tmp[la] = '\0';

    for (size_t k = 0; k < la; k++)
    {
        int da = a->digits[la - 1 - k] - '0';
        int db = (lb > k) ? (b->digits[lb - 1 - k] - '0') : 0;
        int d = da - db - borrow;

        if (d < 0)
        {
            d += 10;
            borrow = 1;
        }
        else
        {
            borrow = 0;
        }

        tmp[la - 1 - k] = (char)('0' + d);
    }

    {
        BigInt res = {0};
        char *norm = normalizar(tmp);
        free(tmp);
        if (norm == NULL)
            return 0;
        res.digits = norm;
        bigint_free(out);
        *out = res;
    }

    return 1;
}

int bigint_multiply(const BigInt *a, const BigInt *b, BigInt *out)
{
    size_t la;
    size_t lb;
    int *acc;
    size_t n;
    char *tmp;

    if (a == NULL || b == NULL || out == NULL || a->digits == NULL || b->digits == NULL)
        return 0;

    if (bigint_is_zero(a) || bigint_is_zero(b))
        return bigint_init(out, "0");

    la = strlen(a->digits);
    lb = strlen(b->digits);
    n = la + lb;

    acc = (int *)calloc(n, sizeof(int));
    if (acc == NULL)
        return 0;

    for (size_t i = 0; i < la; i++)
    {
        int da = a->digits[la - 1 - i] - '0';
        for (size_t j = 0; j < lb; j++)
        {
            int db = b->digits[lb - 1 - j] - '0';
            acc[n - 1 - (i + j)] += da * db;
        }
    }

    for (size_t k = n - 1; k > 0; k--)
    {
        acc[k - 1] += acc[k] / 10;
        acc[k] %= 10;
    }

    tmp = (char *)malloc(n + 1);
    if (tmp == NULL)
    {
        free(acc);
        return 0;
    }

    for (size_t k = 0; k < n; k++)
        tmp[k] = (char)('0' + acc[k]);
    tmp[n] = '\0';

    free(acc);

    {
        BigInt res = {0};
        char *norm = normalizar(tmp);
        free(tmp);
        if (norm == NULL)
            return 0;
        res.digits = norm;
        bigint_free(out);
        *out = res;
    }

    return 1;
}

int bigint_divmod(const BigInt *a, const BigInt *b, BigInt *quotient, BigInt *remainder)
{
    size_t la;
    char *qdigits;
    BigInt rem = {0};
    BigInt d = {0};
    BigInt ten = {0};
    int ok = 0;

    if (a == NULL || b == NULL || quotient == NULL || remainder == NULL)
        return 0;
    if (a->digits == NULL || b->digits == NULL)
        return 0;
    if (bigint_is_zero(b))
        return 0;

    if (!bigint_init(&rem, "0"))
        return 0;
    if (!bigint_set(&d, b))
    {
        bigint_free(&rem);
        return 0;
    }
    if (!bigint_init(&ten, "10"))
    {
        bigint_free(&rem);
        bigint_free(&d);
        return 0;
    }

    la = strlen(a->digits);
    qdigits = (char *)malloc(la + 1);
    if (qdigits == NULL)
        goto cleanup;

    for (size_t i = 0; i < la; i++)
    {
        BigInt rem10 = {0};
        BigInt digit = {0};
        BigInt nuevoRem = {0};
        int qd = 0;
        char digTxt[2];

        digTxt[0] = a->digits[i];
        digTxt[1] = '\0';

        if (!bigint_multiply(&rem, &ten, &rem10))
            goto cleanup;
        if (!bigint_init(&digit, digTxt))
        {
            bigint_free(&rem10);
            goto cleanup;
        }
        if (!bigint_add(&rem10, &digit, &nuevoRem))
        {
            bigint_free(&rem10);
            bigint_free(&digit);
            goto cleanup;
        }
        bigint_free(&rem);
        rem = nuevoRem;
        bigint_free(&rem10);
        bigint_free(&digit);

        while (bigint_compare(&rem, &d) >= 0)
        {
            BigInt tmp = {0};
            if (!bigint_subtract(&rem, &d, &tmp))
                goto cleanup;
            bigint_free(&rem);
            rem = tmp;
            qd++;
        }

        qdigits[i] = (char)('0' + qd);
    }

    qdigits[la] = '\0';

    {
        BigInt qtmp = {0};
        BigInt rtmp = {0};
        char *nq = normalizar(qdigits);
        if (nq == NULL)
            goto cleanup;
        qtmp.digits = nq;
        if (!bigint_set(&rtmp, &rem) && !bigint_init(&rtmp, rem.digits))
        {
            bigint_free(&qtmp);
            goto cleanup;
        }
        bigint_free(quotient);
        bigint_free(remainder);
        *quotient = qtmp;
        *remainder = rtmp;
        ok = 1;
    }

cleanup:
    free(qdigits);
    bigint_free(&ten);
    bigint_free(&rem);
    bigint_free(&d);
    return ok;
}

int bigint_array_create(BigIntArray *arr, size_t len)
{
    if (arr == NULL)
        return 0;

    arr->items = (BigInt *)calloc(len, sizeof(BigInt));
    if (arr->items == NULL)
        return 0;

    arr->len = len;
    for (size_t i = 0; i < len; i++)
    {
        if (!bigint_init(&arr->items[i], "0"))
        {
            bigint_array_free(arr);
            return 0;
        }
    }

    return 1;
}

int bigint_array_set(BigIntArray *arr, size_t idx, const BigInt *value)
{
    if (arr == NULL || value == NULL || idx >= arr->len)
        return 0;

    return bigint_set(&arr->items[idx], value);
}

void bigint_array_free(BigIntArray *arr)
{
    if (arr == NULL)
        return;

    if (arr->items != NULL)
    {
        for (size_t i = 0; i < arr->len; i++)
            bigint_free(&arr->items[i]);
        free(arr->items);
    }

    arr->items = NULL;
    arr->len = 0;
}
