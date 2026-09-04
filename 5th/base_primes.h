#ifndef BASE_PRIMES_H
#define BASE_PRIMES_H

#include "sieve_common.h"

/* Build the small prime table locally on every rank. */
static inline uint64_t *build_base_primes(uint64_t limit,
                                          uint64_t *prime_count, int rank)
{
    const uint64_t candidates = limit < 3 ? 0 : (limit - 1) / 2;
    unsigned char *composite;
    uint64_t *primes;
    uint64_t count = 0;

    *prime_count = 0;
    if (candidates == 0) {
        return NULL;
    }

    composite = checked_malloc(candidates, rank);
    memset(composite, 0, (size_t) candidates);

    for (uint64_t i = 0; i < candidates; ++i) {
        const uint64_t p = odd_value(i);
        if (p > limit / p) {
            break;
        }
        if (composite[i] == 0) {
            const uint64_t first = (p * p - 3) / 2;
            for (uint64_t j = first; j < candidates; j += p) {
                composite[j] = 1;
            }
        }
    }

    for (uint64_t i = 0; i < candidates; ++i) {
        count += composite[i] == 0;
    }
    if (count > UINT64_MAX / sizeof(*primes)) {
        abortf(rank, "base-prime table is too large");
    }
    primes = checked_malloc(count * sizeof(*primes), rank);
    count = 0;
    for (uint64_t i = 0; i < candidates; ++i) {
        if (composite[i] == 0) {
            primes[count++] = odd_value(i);
        }
    }

    free(composite);
    *prime_count = count;
    return primes;
}

#endif
