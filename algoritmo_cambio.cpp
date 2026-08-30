#include "algoritmo_cambio.h"
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace {
constexpr std::size_t kMaxAmount = 500000;
constexpr std::size_t kInf = std::numeric_limits<std::size_t>::max() / 4;

bool to_size(const BigInt *n, std::size_t limit, std::size_t *out) {
    if (!n || !n->digits || !out || !*n->digits) return false;
    std::size_t v = 0;
    for (const char *p = n->digits; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        std::size_t d = static_cast<std::size_t>(*p - '0');
        if (v > limit / 10 || (v == limit / 10 && d > limit % 10)) return false;
        v = v * 10 + d;
    }
    *out = v;
    return true;
}

bool set_result(const std::vector<std::size_t> &counts, BigIntArray *out) {
    if (!out || counts.empty() || !bigint_array_create(out, counts.size())) return false;
    for (std::size_t i = 0; i < counts.size(); ++i) {
        BigInt n{nullptr};
        std::string s = std::to_string(counts[i]);
        if (!bigint_init(&n, s.c_str()) || !bigint_array_set(out, i, &n)) {
            bigint_free(&n); bigint_array_free(out); return false;
        }
        bigint_free(&n);
    }
    return true;
}

bool read_values(const BigIntArray *a, std::size_t amount, std::vector<std::size_t> *out) {
    if (!a || !a->items || !a->len || !out) return false;
    out->assign(a->len, 0);
    for (std::size_t i = 0; i < a->len; ++i)
        if (!to_size(&a->items[i], amount, &(*out)[i])) return false;
    return true;
}

bool solve_unlimited(std::size_t amount, const std::vector<std::size_t> &values,
                     std::size_t min_coins, std::size_t max_coins,
                     std::vector<std::size_t> *counts) {
    max_coins = std::min(max_coins, amount + 1);
    if (min_coins > max_coins) return false;
    std::vector<std::size_t> best(amount + 1, kInf), prev(amount + 1), coin(amount + 1, values.size());
    best[0] = 0;
    for (std::size_t sum = 1; sum <= amount; ++sum) {
        for (std::size_t i = 0; i < values.size(); ++i) {
            std::size_t v = values[i];
            if (!v || v > sum || best[sum - v] == kInf) continue;
            if (best[sum - v] + 1 < best[sum]) {
                best[sum] = best[sum - v] + 1; prev[sum] = sum - v; coin[sum] = i;
            }
        }
    }
    if (best[amount] == kInf || best[amount] < min_coins || best[amount] > max_coins) return false;
    counts->assign(values.size(), 0);
    for (std::size_t sum = amount; sum; sum = prev[sum]) {
        if (coin[sum] >= values.size() || prev[sum] >= sum) return false;
        ++(*counts)[coin[sum]];
    }
    return true;
}

bool solve_limited(std::size_t amount, const std::vector<std::size_t> &values,
                   const std::vector<std::size_t> &stock, std::size_t min_coins,
                   std::size_t max_coins, std::vector<std::size_t> *counts) {
    max_coins = std::min(max_coins, amount + 1);
    if (values.size() != stock.size() || min_coins > max_coins) return false;
    std::vector<std::size_t> best(amount + 1, kInf), prev(amount + 1), coin(amount + 1, values.size());
    best[0] = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::size_t v = values[i];
        if (!v) continue;
        std::size_t copies = std::min(stock[i], amount / v);
        for (std::size_t k = 0; k < copies; ++k) {
            for (std::size_t sum = amount; sum >= v; --sum) {
                if (best[sum - v] != kInf && best[sum - v] + 1 < best[sum]) {
                    best[sum] = best[sum - v] + 1; prev[sum] = sum - v; coin[sum] = i;
                }
                if (sum == v) break;
            }
        }
    }
    if (best[amount] == kInf || best[amount] < min_coins || best[amount] > max_coins) return false;
    counts->assign(values.size(), 0);
    for (std::size_t sum = amount; sum; sum = prev[sum]) {
        if (coin[sum] >= values.size() || prev[sum] >= sum) return false;
        ++(*counts)[coin[sum]];
    }
    return true;
}

int solve(const BigInt *amount, const BigIntArray *denom, const BigIntArray *stock,
          std::size_t min_coins, std::size_t max_coins, BigIntArray *result) {
    std::size_t requested = 0;
    if (!amount || !denom || !result || !to_size(amount, kMaxAmount, &requested)) return 0;
    std::vector<std::size_t> values;
    if (!read_values(denom, requested, &values)) return 0;
    std::vector<std::size_t> counts;
    bool ok = false;
    if (!stock) ok = solve_unlimited(requested, values, min_coins, max_coins, &counts);
    else {
        if (!stock->items || stock->len != denom->len) return 0;
        std::vector<std::size_t> limits;
        if (!read_values(stock, requested, &limits)) return 0;
        ok = solve_limited(requested, values, limits, min_coins, max_coins, &counts);
    }
    return ok && set_result(counts, result) ? 1 : 0;
}

template <typename Solver>
int solve_near(const BigInt *amount, BigInt *covered, BigIntArray *result, Solver solver) {
    std::size_t requested = 0;
    if (!amount || !covered || !to_size(amount, kMaxAmount, &requested)) return 0;
    for (std::size_t value = requested;; --value) {
        BigInt candidate{nullptr}; std::string text = std::to_string(value);
        if (!bigint_init(&candidate, text.c_str())) return 0;
        if (solver(&candidate, result)) { bigint_free(covered); *covered = candidate; return 1; }
        bigint_free(&candidate);
        if (value == 0) break;
    }
    return 0;
}
}

int calcular_cambio_optimo(const BigInt *m, const BigIntArray *d, BigIntArray *s) { return solve(m, d, nullptr, 0, std::numeric_limits<std::size_t>::max(), s); }
int calcular_cambio_optimo_stock(const BigInt *m, const BigIntArray *d, const BigIntArray *st, BigIntArray *s) { return solve(m, d, st, 0, std::numeric_limits<std::size_t>::max(), s); }
int calcular_cambio_optimo_con_rango(const BigInt *m, const BigIntArray *d, std::size_t lo, std::size_t hi, BigIntArray *s) { return solve(m, d, nullptr, lo, hi, s); }
int calcular_cambio_optimo_con_limite(const BigInt *m, const BigIntArray *d, std::size_t hi, BigIntArray *s) { return solve(m, d, nullptr, 0, hi, s); }
int calcular_cambio_optimo_stock_con_rango(const BigInt *m, const BigIntArray *d, const BigIntArray *st, std::size_t lo, std::size_t hi, BigIntArray *s) { return solve(m, d, st, lo, hi, s); }
int calcular_cambio_optimo_stock_con_limite(const BigInt *m, const BigIntArray *d, const BigIntArray *st, std::size_t hi, BigIntArray *s) { return solve(m, d, st, 0, hi, s); }

int calcular_cambio_cercano_con_rango(const BigInt *m, const BigIntArray *d, std::size_t lo, std::size_t hi, BigInt *covered, BigIntArray *s) {
    return solve_near(m, covered, s, [=](const BigInt *candidate, BigIntArray *out) { return solve(candidate, d, nullptr, lo, hi, out); });
}
int calcular_cambio_cercano_stock_con_rango(const BigInt *m, const BigIntArray *d, const BigIntArray *st, std::size_t lo, std::size_t hi, BigInt *covered, BigIntArray *s) {
    return solve_near(m, covered, s, [=](const BigInt *candidate, BigIntArray *out) { return solve(candidate, d, st, lo, hi, out); });
}
