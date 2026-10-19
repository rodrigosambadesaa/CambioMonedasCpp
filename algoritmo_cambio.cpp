#include "algoritmo_cambio.h"
#include <algorithm>
#include <limits>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

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
    for (std::size_t i = 0; i < a->len; ++i) {
        const char *p = a->items[i].digits;
        if (!p || !*p) return false;
        for (const char *c = p; *c; ++c) if (*c < '0' || *c > '9') return false;
        if (!to_size(&a->items[i], amount, &(*out)[i])) (*out)[i] = amount + 1;
    }
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
    struct Node {
        std::shared_ptr<const Node> prev;
        std::size_t index, quantity;
    };
    std::vector<std::shared_ptr<const Node>> plans(amount + 1);
    std::vector<std::size_t> best(amount + 1, kInf);
    best[0] = 0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::size_t v = values[i];
        if (!v || v > amount) continue;
        std::size_t copies = std::min(stock[i], amount / v);
        // Binary grouping makes bounded knapsack O(amount * log(stock)).
        for (std::size_t chunk = 1; copies;) {
            std::size_t take = std::min(chunk, copies);
            copies -= take;
            std::size_t cost = take * v;
            for (std::size_t sum = amount; sum >= cost; --sum) {
                if (best[sum - cost] != kInf && best[sum - cost] + take < best[sum]) {
                    best[sum] = best[sum - cost] + take;
                    plans[sum] = std::make_shared<Node>(Node{plans[sum - cost], i, take});
                }
                if (sum == cost) break;
            }
            if (chunk > amount / 2) chunk = amount; else chunk *= 2;
        }
    }
    if (best[amount] == kInf || best[amount] < min_coins || best[amount] > max_coins) return false;
    counts->assign(values.size(), 0);
    auto plan = plans[amount];
    while (plan) {
        (*counts)[plan->index] += plan->quantity;
        plan = plan->prev;
    }
    return true;
}

int solve_large(const BigInt *amount, const BigIntArray *denom, const BigIntArray *stock,
                std::size_t min_coins, std::size_t max_coins, BigIntArray *result) {
    if (!amount || !denom || !denom->items || !result || (stock && stock->len != denom->len)) return 0;
    BigInt remaining{nullptr}, count{nullptr};
    if (!bigint_set(&remaining, amount) || !bigint_init(&count, "0") ||
        !bigint_array_create(result, denom->len)) {
        bigint_free(&remaining); bigint_free(&count); return 0;
    }
    bool ok = true;
    for (std::size_t i = 0; i < denom->len && ok; ++i) {
        BigInt q{nullptr}, r{nullptr}, product{nullptr}, next{nullptr}, sum{nullptr};
        ok = !bigint_is_zero(&denom->items[i]) && bigint_divmod(&remaining, &denom->items[i], &q, &r);
        if (ok && stock && bigint_compare(&q, &stock->items[i]) > 0) {
            bigint_free(&q); q = {nullptr};
            ok = bigint_set(&q, &stock->items[i]);
        }
        if (ok) ok = bigint_multiply(&q, &denom->items[i], &product)
                   && bigint_subtract(&remaining, &product, &next)
                   && bigint_add(&count, &q, &sum)
                   && bigint_array_set(result, i, &q);
        if (ok) { bigint_free(&remaining); remaining = next; next = {nullptr};
                  bigint_free(&count); count = sum; sum = {nullptr}; }
        bigint_free(&q); bigint_free(&r); bigint_free(&product);
        bigint_free(&next); bigint_free(&sum);
    }
    BigInt lower{nullptr}, upper{nullptr};
    std::string lo = std::to_string(min_coins), hi = std::to_string(max_coins);
    ok = ok && bigint_is_zero(&remaining) && bigint_init(&lower, lo.c_str())
            && bigint_init(&upper, hi.c_str())
            && bigint_compare(&count, &lower) >= 0 && bigint_compare(&count, &upper) <= 0;
    bigint_free(&remaining); bigint_free(&count); bigint_free(&lower); bigint_free(&upper);
    if (!ok) bigint_array_free(result);
    return ok ? 1 : 0;
}


// Rango exacto de piezas con estados (importe, piezas) y trayectorias inmutables.
bool solve_constrained(std::size_t amount, const std::vector<std::size_t> &values,
                       const std::vector<std::size_t> *stock, std::size_t min_coins,
                       std::size_t max_coins, std::vector<std::size_t> *counts) {
    max_coins = std::min(max_coins, amount);
    if (min_coins > max_coins || amount > 20000 || max_coins > 300) return false;
    struct Node { std::shared_ptr<const Node> previous; std::size_t coin, count; };
    using Plan = std::shared_ptr<const Node>;
    std::size_t radix = max_coins + 1;
    std::unordered_map<std::size_t, Plan> states{{0, nullptr}};
    for (std::size_t i = 0; i < values.size(); ++i) {
        std::size_t value = values[i];
        if (!value || value > amount) continue;
        auto next = states;
        for (const auto &[key, path] : states) {
            std::size_t sum = key / radix, used = key % radix;
            std::size_t max = std::min(max_coins - used, (amount - sum) / value);
            if (stock) max = std::min(max, (*stock)[i]);
            for (std::size_t qty = 1; qty <= max; ++qty) {
                auto new_key = (sum + qty * value) * radix + used + qty;
                next.try_emplace(new_key, std::make_shared<Node>(Node{path, i, qty}));
            }
        }
        states.swap(next);
        if (states.size() > 1000000) return false;
    }
    for (std::size_t n = min_coins; n <= max_coins; ++n) {
        auto it = states.find(amount * radix + n);
        if (it == states.end()) continue;
        counts->assign(values.size(), 0);
        for (auto path = it->second; path; path = path->previous)
            (*counts)[path->coin] += path->count;
        return true;
    }
    return false;
}

int solve(const BigInt *amount, const BigIntArray *denom, const BigIntArray *stock,
          std::size_t min_coins, std::size_t max_coins, BigIntArray *result) {
    std::size_t requested = 0;
    if (!amount || !denom || !result) return 0;
    if (!to_size(amount, kMaxAmount, &requested))
        return solve_large(amount, denom, stock, min_coins, max_coins, result);
    std::vector<std::size_t> values;
    if (!read_values(denom, requested, &values)) return 0;
    std::vector<std::size_t> counts;
    bool ok = false;
    if (!stock) {
        ok = solve_unlimited(requested, values, min_coins, max_coins, &counts);
        if (!ok && min_coins) ok = solve_constrained(requested, values, nullptr, min_coins, max_coins, &counts);
    }
    else {
        if (!stock->items || stock->len != denom->len) return 0;
        std::vector<std::size_t> limits;
        if (!read_values(stock, requested, &limits)) return 0;
        ok = solve_limited(requested, values, limits, min_coins, max_coins, &counts);
        if (!ok && min_coins) ok = solve_constrained(requested, values, &limits, min_coins, max_coins, &counts);
    }
    return ok && set_result(counts, result) ? 1 : 0;
}

template <typename Solver>
int solve_near(const BigInt *amount, BigInt *covered, BigIntArray *result, Solver solver) {
    std::size_t requested = 0;
    if (!amount || !covered || !to_size(amount, kMaxAmount, &requested)) return 0;
    for (std::size_t value = requested; value > 0; --value) {
        BigInt candidate{nullptr}; std::string text = std::to_string(value);
        if (!bigint_init(&candidate, text.c_str())) return 0;
        if (solver(&candidate, result)) { bigint_free(covered); *covered = candidate; return 1; }
        bigint_free(&candidate);
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
