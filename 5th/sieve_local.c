/* Step 2: every rank builds its own base-prime table. */
#include "base_primes.h"

int main(int argc, char **argv)
{
    int rank, processes;
    uint64_t n, root, candidates, begin, end, count, low;
    uint64_t *primes;
    uint64_t prime_count, local_count = 0;
    unsigned char *marked;
    double start, elapsed;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &processes);

    if (argc != 2) {
        abortf(rank, "Usage: %s <n>", argv[0]);
    }
    n = parse_u64_or_abort(argc, argv, rank, "<n>");
    root = floor_sqrt_u64(n);
    candidates = n >= 3 ? (n - 1) / 2 : 0;
    begin = block_begin(rank, processes, candidates);
    end = block_end(rank, processes, candidates);
    count = end - begin;
    low = odd_value(begin);

    marked = checked_malloc(count, rank);
    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();
    memset(marked, 0, (size_t) count);
    primes = build_base_primes(root, &prime_count, rank);

    for (uint64_t k = 0; k < prime_count; ++k) {
        const uint64_t high = count == 0 ? 0 : low + 2 * (count - 1);
        mark_odd_multiples(marked, count, low, high, primes[k]);
    }

    for (uint64_t i = 0; i < count; ++i) {
        local_count += marked[i] == 0;
    }
    if (rank == 0) {
        ++local_count;
    }
    elapsed = MPI_Wtime() - start;
    reduce_and_print("odd_local_primes", n, rank, processes, local_count, elapsed, 0);

    free(primes);
    free(marked);
    MPI_Finalize();
    return 0;
}
