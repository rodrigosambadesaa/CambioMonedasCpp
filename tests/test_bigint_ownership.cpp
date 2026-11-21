#include "bigint.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>

int main() {
    BigInt source{}, left{}, zero{}, result{}, quotient{}, remainder{};
    assert(bigint_init(&source, "001234567890123456789012345678901234567890"));
    assert(bigint_init(&left, "777"));
    // Replacing a live value must release the old buffer.
    assert(bigint_init(&left, source.digits));
    assert(std::strcmp(left.digits, "1234567890123456789012345678901234567890") == 0);
    // Self-alias initialization must duplicate before freeing.
    assert(bigint_init(&left, left.digits));
    assert(std::strcmp(left.digits, "1234567890123456789012345678901234567890") == 0);
    assert(bigint_init(&zero, "0"));
    assert(bigint_init(&result, "99999999999999999"));
    assert(bigint_multiply(&left, &zero, &result));
    assert(std::strcmp(result.digits, "0") == 0);
    // An existing destination must be replaced without leaks or double free.
    assert(bigint_add(&left, &left, &result));
    assert(bigint_divmod(&result, &left, &quotient, &remainder));
    assert(std::strcmp(quotient.digits, "2") == 0);
    assert(std::strcmp(remainder.digits, "0") == 0);
    // Arithmetic must remain exact far beyond 64 bits, without leaking
    // intermediate allocations even for a ten-thousand-digit carry chain.
    BigInt big{}, one{}, large_sum{};
    std::string large(10000, '9');
    assert(bigint_init(&big, large.c_str()));
    assert(bigint_init(&one, "1"));
    assert(bigint_add(&big, &one, &large_sum));
    assert(std::string(large_sum.digits) == "1" + std::string(10000, '0'));
    bigint_free(&big);
    bigint_free(&one);
    bigint_free(&large_sum);
    BigIntArray array{};
    assert(bigint_array_create(&array, 3));
    assert(bigint_array_set(&array, 0, &left));
    assert(bigint_array_create(&array, 2));
    assert(array.len == 2 && std::strcmp(array.items[0].digits, "0") == 0);
    bigint_array_free(&array);
    bigint_array_free(&array);
    bigint_free(&source);
    bigint_free(&left);
    bigint_free(&zero);
    bigint_free(&result);
    bigint_free(&quotient);
    bigint_free(&remainder);
    std::cout << "PASS: BigInt ownership/reinitialize/zero-multiply/recreate\n";
}
