#ifndef BIGINT_H
#define BIGINT_H

#include <stddef.h>

typedef struct
{
    char *digits;
} BigInt;

typedef struct
{
    BigInt *items;
    size_t len;
} BigIntArray;

int bigint_init(BigInt *n, const char *text);
int bigint_set(BigInt *dest, const BigInt *src);
void bigint_free(BigInt *n);

int bigint_compare(const BigInt *a, const BigInt *b);
int bigint_is_zero(const BigInt *n);

int bigint_add(const BigInt *a, const BigInt *b, BigInt *out);
int bigint_subtract(const BigInt *a, const BigInt *b, BigInt *out);
int bigint_multiply(const BigInt *a, const BigInt *b, BigInt *out);
int bigint_divmod(const BigInt *a, const BigInt *b, BigInt *quotient, BigInt *remainder);

int bigint_array_create(BigIntArray *arr, size_t len);
int bigint_array_set(BigIntArray *arr, size_t idx, const BigInt *value);
void bigint_array_free(BigIntArray *arr);

#endif
