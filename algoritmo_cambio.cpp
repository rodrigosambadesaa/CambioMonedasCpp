#include "algoritmo_cambio.h"
#include <algorithm>
#include <limits>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <queue>
#include <tuple>
#include <numeric>
#include <cstdint>
#include <stdexcept>
#include <functional>

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

// Exact two-denomination Diophantine solver using the native arbitrary-size
// BigInt implementation. All modular-inverse arithmetic uses small (<=50000)
// coprime coefficients; money, stock, quotient and coin counts stay BigInt.
struct ManagedBig {
    BigInt number{nullptr};
    ManagedBig() = default;
    explicit ManagedBig(const std::string& digits) {
        if (!bigint_init(&number,digits.c_str()))
            throw std::runtime_error("BigInt initialization failed");
    }
    ManagedBig(const ManagedBig&)=delete;
    ManagedBig& operator=(const ManagedBig&)=delete;
    ~ManagedBig() {bigint_free(&number);}
};

long long inverse_mod(long long x,long long modulus) {
    if (modulus==1) return 0;
    long long oldR=modulus,r=x%modulus,oldT=0,t=1;
    while(r!=0) {
        long long quotient=oldR/r;
        long long nextR=oldR-quotient*r;
        long long nextT=oldT-quotient*t;
        oldR=r;r=nextR;oldT=t;t=nextT;
    }
    if(oldR!=1) return -1;
    return ((oldT%modulus)+modulus)%modulus;
}

// 1=success, 0=provably infeasible, -1=coefficients too large for this route.
int solve_large_two(const BigInt* amount, const BigIntArray* denom,
                    const BigIntArray* stock, std::size_t minCoins,
                    std::size_t maxCoins, BigIntArray* result) {
    if (!amount || !denom || !denom->items || denom->len!=2 || !result ||
        stock && (!stock->items || stock->len!=2))
        return -1;
    std::size_t d0=0,d1=0;
    if (!to_size(&denom->items[0],50000,&d0) ||
        !to_size(&denom->items[1],50000,&d1) ||
        d0==0 || d1==0 || d0==d1) return -1;
    const std::size_t high=d0>d1?0:1, low=1-high;
    std::size_t a=std::max(d0,d1),b=std::min(d0,d1);
    std::size_t divisor=std::gcd(a,b), A=a/divisor, B=b/divisor;
    ManagedBig g(std::to_string(divisor)), highCoin(std::to_string(a)),
               lowCoin(std::to_string(b)), one("1"),
               period(std::to_string(B));
    ManagedBig C, remainderG;
    if(!bigint_divmod(amount,&g.number,&C.number,&remainderG.number)) return -1;
    if(!bigint_is_zero(&remainderG.number)) return 0;
    ManagedBig cQuotient,cResidue;
    if(!bigint_divmod(&C.number,&period.number,&cQuotient.number,&cResidue.number))
        return -1;
    std::size_t residue=0;
    if(!to_size(&cResidue.number,B-1,&residue)) return -1;
    long long inverse=inverse_mod(static_cast<long long>(A),static_cast<long long>(B));
    if(inverse<0) return -1;
    std::size_t x0=static_cast<std::size_t>(
        (static_cast<std::uint64_t>(residue)*static_cast<std::uint64_t>(inverse))%B);
    ManagedBig initial(std::to_string(x0)), qMax,remainderA;
    if(!bigint_divmod(amount,&highCoin.number,&qMax.number,&remainderA.number))
        return -1;
    ManagedBig upper;
    if(stock && bigint_compare(&qMax.number,&stock->items[high])>0) {
        if(!bigint_set(&upper.number,&stock->items[high])) return -1;
    } else {
        if(!bigint_set(&upper.number,&qMax.number)) return -1;
    }
    if(bigint_compare(&upper.number,&initial.number)<0) return 0;
    ManagedBig upperDelta,numberOfSteps,unused,scaled,x;
    if(!bigint_subtract(&upper.number,&initial.number,&upperDelta.number) ||
        !bigint_divmod(&upperDelta.number,&period.number,&numberOfSteps.number,&unused.number) ||
        !bigint_multiply(&numberOfSteps.number,&period.number,&scaled.number) ||
        !bigint_add(&initial.number,&scaled.number,&x.number)) return -1;
    ManagedBig paid,remaining,y,unusedY,total;
    if(!bigint_multiply(&x.number,&highCoin.number,&paid.number) ||
        !bigint_subtract(amount,&paid.number,&remaining.number) ||
        !bigint_divmod(&remaining.number,&lowCoin.number,&y.number,&unusedY.number) ||
        !bigint_is_zero(&unusedY.number) ||
        !bigint_add(&x.number,&y.number,&total.number)) return -1;
    if(stock && bigint_compare(&y.number,&stock->items[low])>0) return 0;
    ManagedBig minimum(std::to_string(minCoins));
    if(bigint_compare(&total.number,&minimum.number)<0) {
        // Moving one period B from x to y increases coin count by A-B.
        ManagedBig deficit,delta(std::to_string(A-B)),adjusted,division,
                   rest,steps,withdraw,received,xNext,yNext,totalNext;
        if(!bigint_subtract(&minimum.number,&total.number,&deficit.number) ||
            !bigint_add(&deficit.number,&delta.number,&adjusted.number) ||
            !bigint_subtract(&adjusted.number,&one.number,&division.number) ||
            !bigint_divmod(&division.number,&delta.number,&steps.number,&rest.number) ||
            !bigint_multiply(&steps.number,&period.number,&withdraw.number))
            return -1;
        if(bigint_compare(&x.number,&withdraw.number)<0) return 0;
        ManagedBig otherPeriod(std::to_string(A));
        if(!bigint_subtract(&x.number,&withdraw.number,&xNext.number) ||
            !bigint_multiply(&steps.number,&otherPeriod.number,&received.number) ||
            !bigint_add(&y.number,&received.number,&yNext.number) ||
            !bigint_add(&xNext.number,&yNext.number,&totalNext.number)) return -1;
        if(stock && bigint_compare(&yNext.number,&stock->items[low])>0) return 0;
        if(!bigint_set(&x.number,&xNext.number) ||
            !bigint_set(&y.number,&yNext.number) ||
            !bigint_set(&total.number,&totalNext.number)) return -1;
    }
    if(maxCoins!=std::numeric_limits<std::size_t>::max()) {
        ManagedBig maximum(std::to_string(maxCoins));
        if(bigint_compare(&total.number,&maximum.number)>0) return 0;
    }
    if(!bigint_array_create(result,2)) return -1;
    if(!bigint_array_set(result,high,&x.number) ||
       !bigint_array_set(result,low,&y.number)) {
        bigint_array_free(result);return -1;
    }
    return 1;
}

// 1 = mathematically certified optimum, 0 = proven no solution in max range,
// -1 = unsupported graph (leave legacy large-number branch unchanged).
// For largest denomination A, every smaller denomination d incurs penalty A-d.
// Minimize A*coin_count - amount in each remainder class mod A. Fill the
// remaining sum with A-coins: a shortest path then proves global optimality.
int solve_large_residue(const BigInt *amount, const BigIntArray *denom,
                        std::size_t min_coins, std::size_t max_coins,
                        BigIntArray *result) {
    if (!amount || !denom || !denom->items || !denom->len || !result ||
        min_coins != 0) return -1;
    constexpr std::size_t kResidueLimit = 50000;
    std::vector<std::size_t> values;
    std::size_t largest=0, indexLargest=0;
    for (std::size_t i=0;i<denom->len;++i) {
        std::size_t v=0;
        if (!to_size(&denom->items[i],kResidueLimit,&v) || v==0) return -1;
        values.push_back(v);
        if (v>largest) { largest=v; indexLargest=i; }
    }
    if (largest<2) return -1;
    BigInt divisor{nullptr}, quotient{nullptr}, remainder{nullptr};
    if (!bigint_init(&divisor,std::to_string(largest).c_str()) ||
        !bigint_divmod(amount,&divisor,&quotient,&remainder)) {
        bigint_free(&divisor);bigint_free(&quotient);bigint_free(&remainder);
        return -1;
    }
    std::size_t target=0;
    bool ok=to_size(&remainder,largest-1,&target);
    bigint_free(&quotient);
    bigint_free(&remainder);
    if (!ok) {bigint_free(&divisor);return -1;}
    constexpr std::uint64_t kNoPath=std::numeric_limits<std::uint64_t>::max();
    std::vector<std::uint64_t> penalty(largest,kNoPath), money(largest,kNoPath);
    std::vector<std::size_t> predecessor(largest,largest), chosen(largest,values.size());
    using Cost=std::tuple<std::uint64_t,std::uint64_t,std::size_t>;
    std::priority_queue<Cost,std::vector<Cost>,std::greater<Cost>> work;
    penalty[0]=money[0]=0;
    work.emplace(0,0,0);
    while (!work.empty()) {
        auto [cost,base,r]=work.top(); work.pop();
        if (penalty[r]!=cost || money[r]!=base) continue;
        for (std::size_t i=0;i<values.size();++i) {
            std::size_t d=values[i];
            if (d==largest) continue;
            std::size_t next=(r+d)%largest;
            auto newCost=cost+(largest-d), newBase=base+d;
            if (newCost>penalty[next] ||
                (newCost==penalty[next] && newBase>=money[next])) continue;
            penalty[next]=newCost;
            money[next]=newBase;
            predecessor[next]=r;
            chosen[next]=i;
            work.emplace(newCost,newBase,next);
        }
    }
    if (penalty[target]==kNoPath) {bigint_free(&divisor);return -1;}
    BigInt baseAmount{nullptr},extra{nullptr},largePieces{nullptr},unused{nullptr};
    bool ready=bigint_init(&baseAmount,std::to_string(money[target]).c_str()) &&
               bigint_compare(amount,&baseAmount)>=0 &&
               bigint_subtract(amount,&baseAmount,&extra) &&
               bigint_divmod(&extra,&divisor,&largePieces,&unused) &&
               bigint_is_zero(&unused);
    bigint_free(&divisor);
    bigint_free(&baseAmount);
    bigint_free(&extra);
    bigint_free(&unused);
    if (!ready) {bigint_free(&largePieces);return -1;}
    std::vector<std::size_t> counts(values.size(),0);
    std::size_t r=target;
    for (std::size_t steps=0;r!=0 && steps<largest;++steps) {
        if (chosen[r]>=values.size() || predecessor[r]>=largest) {
            bigint_free(&largePieces);return -1;
        }
        ++counts[chosen[r]];
        r=predecessor[r];
    }
    if (r!=0) {bigint_free(&largePieces);return -1;}
    std::size_t smallPieces=0;
    for (auto count:counts) smallPieces+=count;
    BigInt small{nullptr},total{nullptr},bound{nullptr};
    ready=bigint_init(&small,std::to_string(smallPieces).c_str()) &&
          bigint_add(&largePieces,&small,&total);
    if (ready && max_coins!=std::numeric_limits<std::size_t>::max()) {
        ready=bigint_init(&bound,std::to_string(max_coins).c_str());
        if (ready && bigint_compare(&total,&bound)>0) {
            bigint_free(&small);bigint_free(&total);bigint_free(&bound);
            bigint_free(&largePieces);
            return 0; // The minimal possible number of coins exceeds max.
        }
    }
    bigint_free(&small);bigint_free(&total);bigint_free(&bound);
    if (!ready) {bigint_free(&largePieces);return -1;}
    counts[indexLargest]=0; // Include largest as BigInt without truncation.
    if (!bigint_array_create(result,values.size())) {
        bigint_free(&largePieces);return -1;
    }
    for (std::size_t i=0;i<values.size();++i) {
        BigInt value{nullptr};
        bool assigned=(i==indexLargest)
            ? bigint_array_set(result,i,&largePieces)
            : bigint_init(&value,std::to_string(counts[i]).c_str()) &&
              bigint_array_set(result,i,&value);
        bigint_free(&value);
        if (!assigned) {bigint_array_free(result);bigint_free(&largePieces);return -1;}
    }
    bigint_free(&largePieces);
    return 1;
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
    // SIZE_MAX signals NO upper constraint, not an upper limit at 2^64-1.
    // An arbitrary-precision amount can legitimately require more pieces
    // than size_t can represent.
    ok = ok && bigint_is_zero(&remaining) && bigint_init(&lower, lo.c_str())
            && bigint_compare(&count, &lower) >= 0;
    if (ok && max_coins != std::numeric_limits<std::size_t>::max())
        ok = bigint_init(&upper, hi.c_str()) &&
             bigint_compare(&count, &upper) <= 0;
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
    if (!to_size(amount, kMaxAmount, &requested)) {
        if (denom->len==2) {
            int exact=solve_large_two(amount,denom,stock,min_coins,max_coins,result);
            if (exact>=0) return exact;
        }
        if (!stock) {
            int exact=solve_large_residue(amount,denom,min_coins,max_coins,result);
            if (exact>=0) return exact;
        }
        return solve_large(amount, denom, stock, min_coins, max_coins, result);
    }
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


// Closest change without rebuilding the entire DP for every smaller amount.
// Used when the lower bound on the coin count is zero.
int solve_near_single_pass(const BigInt *amount, const BigIntArray *denom,
                           const BigIntArray *stock, std::size_t max_coins,
                           BigInt *covered, BigIntArray *result) {
    std::size_t requested=0;
    if (!amount || !denom || !result || !covered ||
        !to_size(amount,kMaxAmount,&requested) || requested==0) return 0;
    std::vector<std::size_t> values;
    if (!read_values(denom,requested,&values)) return 0;
    if (stock && (stock->len!=denom->len || !stock->items)) return 0;
    std::vector<std::size_t> limits;
    if (stock && !read_values(stock,requested,&limits)) return 0;
    std::vector<std::size_t> best(requested+1,kInf);
    best[0]=0;
    std::vector<std::size_t> previous(requested+1,0), index(requested+1,values.size());
    struct Node {std::shared_ptr<const Node> prior;std::size_t index,count;};
    using Plan=std::shared_ptr<const Node>;
    std::vector<Plan> plans;
    if (!stock) {
        for (std::size_t sum=1;sum<=requested;++sum)
            for (std::size_t i=0;i<values.size();++i) {
                auto value=values[i];
                if (value==0 || value>sum || best[sum-value]==kInf) continue;
                if (best[sum-value]+1<best[sum]) {
                    best[sum]=best[sum-value]+1;
                    previous[sum]=sum-value;index[sum]=i;
                }
            }
    } else {
        plans.resize(requested+1);
        for (std::size_t i=0;i<values.size();++i) {
            std::size_t value=values[i];
            if (value==0 || value>requested) continue;
            std::size_t copies=std::min(limits[i],requested/value);
            for (std::size_t chunk=1;copies;) {
                std::size_t take=std::min(chunk,copies);
                copies-=take;
                std::size_t cost=take*value;
                for (std::size_t sum=requested;sum>=cost;--sum) {
                    if (best[sum-cost]!=kInf &&
                        best[sum-cost]+take<best[sum]) {
                        best[sum]=best[sum-cost]+take;
                        plans[sum]=std::make_shared<Node>(Node{plans[sum-cost],i,take});
                    }
                    if (sum==cost) break;
                }
                if (chunk>requested/2) chunk=requested;else chunk*=2;
            }
        }
    }
    for (std::size_t sum=requested;sum>0;--sum) {
        if (best[sum]==kInf || best[sum]>max_coins) continue;
        std::vector<std::size_t> counts(values.size(),0);
        if (stock) {
            for (auto path=plans[sum];path;path=path->prior)
                counts[path->index]+=path->count;
        } else {
            for (std::size_t s=sum;s>0;s=previous[s]) {
                if (index[s]>=values.size() || previous[s]>=s) return 0;
                ++counts[index[s]];
            }
        }
        BigInt candidate{nullptr};
        std::string value=std::to_string(sum);
        if (!bigint_init(&candidate,value.c_str())) return 0;
        if (!set_result(counts,result)) {bigint_free(&candidate);return 0;}
        bigint_free(covered);
        *covered=candidate;
        return 1;
    }
    return 0;
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
    if (lo==0) return solve_near_single_pass(m,d,nullptr,hi,covered,s);
    return solve_near(m, covered, s, [=](const BigInt *candidate, BigIntArray *out) { return solve(candidate, d, nullptr, lo, hi, out); });
}
int calcular_cambio_cercano_stock_con_rango(const BigInt *m, const BigIntArray *d, const BigIntArray *st, std::size_t lo, std::size_t hi, BigInt *covered, BigIntArray *s) {
    if (lo==0) return solve_near_single_pass(m,d,st,hi,covered,s);
    return solve_near(m, covered, s, [=](const BigInt *candidate, BigIntArray *out) { return solve(candidate, d, st, lo, hi, out); });
}
