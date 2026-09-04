/* Quinn 5.7: the direct distributed sieve, kept as the teaching baseline. */
#include "sieve_common.h"

int main(int argc, char **argv)
{
    int rank, processes;
    uint64_t n, root, candidates, begin, end, count, low;
    uint64_t prime = 2, root_index = 0, local_count = 0;
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
    candidates = n - 1; /* values 2, 3, ..., n */
    begin = block_begin(rank, processes, candidates);
    end = block_end(rank, processes, candidates);
    count = end - begin;
    low = 2 + begin;

    /* The book version requires rank 0 to find every prime. */
    if (root >= 2 && block_end(0, processes, candidates) < root - 1) {
        abortf(rank, "too many processes for sieve_book: rank 0 must contain "
                     "all candidates through sqrt(n)");
    }

    marked = checked_malloc(count, rank);
    MPI_Barrier(MPI_COMM_WORLD);
    start = MPI_Wtime();
    memset(marked, 0, (size_t) count);

    while (prime <= root) {
        uint64_t first, remainder, index;

        if (prime > low / prime) {
            first = prime * prime;
        } else {
            remainder = low % prime;
            first = remainder == 0 ? low : low + prime - remainder;
        }
        if (first <= n) {
            index = first - low;
            for (; index < count; index += prime) {
                marked[index] = 1;
            }
        }

        if (rank == 0) {
            const uint64_t root_count = block_end(0, processes, candidates);
            do {
                ++root_index;
            } while (root_index < root_count && marked[root_index]);
            prime = 2 + root_index;
        }
        MPI_Bcast(&prime, 1, MPI_UINT64_T, 0, MPI_COMM_WORLD);
    }

    for (uint64_t i = 0; i < count; ++i) {
        local_count += marked[i] == 0;
    }
    elapsed = MPI_Wtime() - start;
    reduce_and_print("book", n, rank, processes, local_count, elapsed, 0);

    free(marked);
    MPI_Finalize();
    return 0;
}
