/* Step 1: store only odd candidates, but retain Quinn's broadcasts. */
#include "sieve_common.h"

int main(int argc, char **argv)
{
    int rank, processes;
    uint64_t n, root, candidates, begin, end, count, low;
    uint64_t prime = 3, root_index = 0, local_count = 0;
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

    if (root >= 3) {
        const uint64_t root_count = block_end(0, processes, candidates);
        if (root_count == 0 || odd_value(root_count - 1) < root) {
            abortf(rank, "too many processes for sieve_odd: rank 0 must contain "
                         "odd candidates through sqrt(n)");
        }
    }

    marked = checked_malloc(count, rank);
    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();
    memset(marked, 0, (size_t) count);

    while (prime <= root) {
        const uint64_t high = count == 0 ? 0 : low + 2 * (count - 1);
        mark_odd_multiples(marked, count, low, high, prime);

        if (rank == 0) {
            const uint64_t root_count = block_end(0, processes, candidates);
            do {
                ++root_index;
            } while (root_index < root_count && marked[root_index]);
            prime = odd_value(root_index);
        }
        MPI_Bcast(&prime, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    }

    for (uint64_t i = 0; i < count; ++i) {
        local_count += marked[i] == 0;
    }
    if (rank == 0) {
        ++local_count; /* candidate 2 is not stored */
    }
    elapsed = MPI_Wtime() - start;
    reduce_and_print("odd_broadcast", n, rank, processes, local_count, elapsed, 0);

    free(marked);
    MPI_Finalize();
    return 0;
}
