/* Step 3: cache-blocked odd sieve with a local base-prime table. */
#include "base_primes.h"

int main(int argc, char **argv)
{
    int rank, processes;
    uint64_t n, root, candidates, begin, end, count, low, block_bytes;
    uint64_t *primes;
    uint64_t prime_count, local_count = 0;
    unsigned char *block;
    double start, elapsed;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &processes);

    if (argc < 2 || argc > 3) {
        abortf(rank, "Usage: %s <n> [block_KiB]", argv[0]);
    }
    n = parse_u64_or_abort(argc, argv, rank, "<n> [block_KiB]");
    block_bytes = parse_block_bytes_or_abort(argc, argv, rank);
    root = floor_sqrt_u64(n);
    candidates = n >= 3 ? (n - 1) / 2 : 0;
    begin = block_begin(rank, processes, candidates);
    end = block_end(rank, processes, candidates);
    count = end - begin;
    low = odd_value(begin);

    block = checked_malloc(block_bytes, rank);
    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();
    primes = build_base_primes(root, &prime_count, rank);

    /* Process a small block with all relevant primes before moving on. */
    for (uint64_t offset = 0; offset < count; offset += block_bytes) {
        const uint64_t size = (count - offset < block_bytes)
                                  ? count - offset : block_bytes;
        const uint64_t block_low = low + 2 * offset;
        const uint64_t block_high = block_low + 2 * (size - 1);

        memset(block, 0, (size_t) size);
        for (uint64_t k = 0; k < prime_count; ++k) {
            mark_odd_multiples(block, size, block_low, block_high, primes[k]);
        }
        for (uint64_t i = 0; i < size; ++i) {
            local_count += block[i] == 0;
        }
    }

    if (rank == 0) {
        ++local_count;
    }
    elapsed = MPI_Wtime() - start;
    reduce_and_print("odd_local_blocked", n, rank, processes, local_count,
                     elapsed, block_bytes);

    free(primes);
    free(block);
    MPI_Finalize();
    return 0;
}
