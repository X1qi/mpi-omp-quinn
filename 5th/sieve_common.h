#ifndef SIEVE_COMMON_H
#define SIEVE_COMMON_H

#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <mpi.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Balanced half-open interval [begin, end) without rank * size overflow. */
static inline uint64_t block_begin(int rank, int processes, uint64_t items)
{
    const uint64_t p = (uint64_t) processes;
    const uint64_t r = (uint64_t) rank;
    return (items / p) * r + ((items % p) * r) / p;
}

static inline uint64_t block_end(int rank, int processes, uint64_t items)
{
    return block_begin(rank + 1, processes, items);
}

static inline uint64_t floor_sqrt_u64(uint64_t n)
{
    uint64_t root = (uint64_t) sqrtl((long double) n);

    while (root < UINT64_MAX && root + 1 <= n / (root + 1)) {
        ++root;
    }
    while (root != 0 && root > n / root) {
        --root;
    }
    return root;
}

static inline void abortf(int rank, const char *format, ...)
{
    va_list args;

    if (rank == 0) {
        va_start(args, format);
        vfprintf(stderr, format, args);
        va_end(args);
        fputc('\n', stderr);
    }
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
}

static inline uint64_t parse_u64_or_abort(int argc, char **argv, int rank,
                                          const char *usage)
{
    char *end = NULL;
    unsigned long long value;

    if (argc < 2) {
        abortf(rank, "Usage: %s %s", argv[0], usage);
    }

    errno = 0;
    value = strtoull(argv[1], &end, 10);
    if (errno != 0 || end == argv[1] || *end != '\0' || value < 2) {
        abortf(rank, "n must be an integer >= 2");
    }
    return (uint64_t) value;
}

static inline uint64_t parse_block_bytes_or_abort(int argc, char **argv,
                                                  int rank)
{
    const uint64_t default_kib = 32;
    char *end = NULL;
    unsigned long long kib = default_kib;

    if (argc > 3) {
        abortf(rank, "Usage: %s <n> [block_KiB]", argv[0]);
    }
    if (argc == 3) {
        errno = 0;
        kib = strtoull(argv[2], &end, 10);
        if (errno != 0 || end == argv[2] || *end != '\0' || kib == 0 ||
            kib > UINT64_MAX / 1024u) {
            abortf(rank, "block_KiB must be a positive integer");
        }
    }
    return (uint64_t) kib * 1024u;
}

static inline void *checked_malloc(uint64_t bytes, int rank)
{
    size_t request;
    void *memory;

    if (bytes > (uint64_t) SIZE_MAX) {
        abortf(rank, "allocation is too large: %" PRIu64 " bytes", bytes);
    }
    request = bytes == 0 ? 1u : (size_t) bytes;
    memory = malloc(request);
    if (memory == NULL) {
        abortf(rank, "cannot allocate %zu bytes", request);
    }
    return memory;
}

static inline uint64_t odd_value(uint64_t odd_index)
{
    return 3 + 2 * odd_index;
}

/* Mark multiples of odd prime p in odd candidates [low, high]. */
static inline void mark_odd_multiples(unsigned char *marked, uint64_t count,
                                      uint64_t low, uint64_t high,
                                      uint64_t p)
{
    uint64_t first, remainder, index;

    if (p > high / p) {
        return; /* p*p is beyond this segment. */
    }

    first = p * p;
    if (first < low) {
        remainder = low % p;
        first = remainder == 0 ? low : low + (p - remainder);
    }
    if ((first & 1u) == 0) {
        first += p;
    }
    if (first > high) {
        return;
    }

    index = (first - low) / 2;
    for (; index < count; index += p) {
        marked[index] = 1;
    }
}

static inline void reduce_and_print(const char *version, uint64_t n, int rank,
                                    int processes, uint64_t local_count,
                                    double local_elapsed, uint64_t extra_bytes)
{
    uint64_t global_count = 0;
    double elapsed = 0.0;

    MPI_Reduce(&local_count, &global_count, 1, MPI_UINT64_T, MPI_SUM, 0,
               MPI_COMM_WORLD);
    MPI_Reduce(&local_elapsed, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0,
               MPI_COMM_WORLD);

    if (rank == 0) {
        printf("VERSION %s\n", version);
        printf("N %" PRIu64 "\n", n);
        printf("PROCESSES %d\n", processes);
        printf("PRIMES %" PRIu64 "\n", global_count);
        printf("TIME_SECONDS %.9f\n", elapsed);
        if (extra_bytes != 0) {
            printf("BLOCK_KIB %" PRIu64 "\n", extra_bytes / 1024u);
        }
    }
}

#endif
