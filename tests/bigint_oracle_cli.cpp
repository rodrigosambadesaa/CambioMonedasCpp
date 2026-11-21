/* Small process-per-operation harness for independent Python arithmetic oracles. */
#include <stdio.h>
#include <string.h>
#include "bigint.h"

int main(int argc, char **argv)
{
    BigInt a = {0}, b = {0}, out = {0}, rem = {0};
    int ok = 0;
    int rc = 3;

    if (argc != 4)
        return 2;
    if (!bigint_init(&a, argv[2]) || !bigint_init(&b, argv[3]))
        goto cleanup;

    if (strcmp(argv[1], "add") == 0)
        ok = bigint_add(&a, &b, &out);
    else if (strcmp(argv[1], "sub") == 0)
        ok = bigint_subtract(&a, &b, &out);
    else if (strcmp(argv[1], "mul") == 0)
        ok = bigint_multiply(&a, &b, &out);
    else if (strcmp(argv[1], "mul-reuse") == 0)
    {
        /* A multiplication yielding zero must release an existing destination. */
        if (!bigint_init(&out, "777"))
            goto cleanup;
        ok = bigint_multiply(&a, &b, &out);
    }
    else if (strcmp(argv[1], "init-reuse") == 0)
    {
        /* Reinitialize a live object: any old digits must be freed. */
        if (!bigint_init(&out, "777"))
            goto cleanup;
        ok = bigint_init(&out, argv[2]);
    }
    else if (strcmp(argv[1], "div") == 0)
        ok = bigint_divmod(&a, &b, &out, &rem);
    else
    {
        rc = 2;
        goto cleanup;
    }

    if (!ok || !out.digits || (strcmp(argv[1], "div") == 0 && !rem.digits))
        goto cleanup;
    if (strcmp(argv[1], "div") == 0)
        printf("%s %s\n", out.digits, rem.digits);
    else
        printf("%s\n", out.digits);
    rc = 0;

cleanup:
    bigint_free(&a);
    bigint_free(&b);
    bigint_free(&out);
    bigint_free(&rem);
    return rc;
}
