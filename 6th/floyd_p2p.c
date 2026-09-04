#include "floyd_common.h"

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, processes;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &processes);
    floyd_options options = parse_options(argc, argv);
    checked_matrix_bytes(options.n);

    int begin, end;
    row_range(options.n, processes, rank, &begin, &end);
    int local_rows = end - begin;

    int *counts = malloc((size_t)processes * sizeof(*counts));
    int *displacements = malloc((size_t)processes * sizeof(*displacements));
    if (counts == NULL || displacements == NULL) {
        fprintf(stderr, "error: unable to allocate scatter metadata\n");
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    build_scatter_layout(options.n, processes, counts, displacements);

    int *global = NULL;
    int *reference = NULL;
    if (rank == 0) {
        global = alloc_matrix(options.n, options.n);
        reference = alloc_matrix(options.n, options.n);
        make_graph(global, options.n, options.seed);
        memcpy(reference, global, (size_t)options.n * (size_t)options.n * sizeof(int));
    }
    int *local = alloc_matrix(local_rows, options.n);
    int *pivot = alloc_matrix(1, options.n);

    MPI_Scatterv(global, counts, displacements, MPI_INT,
                 local, counts[rank], MPI_INT, 0, MPI_COMM_WORLD);
    if (rank == 0) {
        floyd_serial(reference, options.n);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();
    for (int k = 0; k < options.n; ++k) {
        int owner = row_owner(options.n, processes, k);
        if (rank == owner) {
            int local_index = k - begin;
            memcpy(pivot, &local[(size_t)local_index * (size_t)options.n],
                   (size_t)options.n * sizeof(int));
            for (int destination = 0; destination < processes; ++destination) {
                if (destination != owner) {
                    MPI_Send(pivot, options.n, MPI_INT, destination,
                             FLOYD_TAG, MPI_COMM_WORLD);
                }
            }
        } else {
            MPI_Recv(pivot, options.n, MPI_INT, owner, FLOYD_TAG,
                     MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        update_local_rows_at_k(local, local_rows, options.n, k, pivot);
    }
    double elapsed = MPI_Wtime() - start;

    double maximum_elapsed = 0.0;
    MPI_Reduce(&elapsed, &maximum_elapsed, 1, MPI_DOUBLE, MPI_MAX, 0,
               MPI_COMM_WORLD);

    MPI_Gatherv(local, counts[rank], MPI_INT,
                global, counts, displacements, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        int correct = verify_result(global, reference, options.n);
        if (options.dump_path != NULL && !dump_matrix(options.dump_path, global, options.n)) {
            correct = 0;
        }
        printf("N %d\nPROCESSES %d\nDIST_SUM %lld\nTIME %.9f\nCORRECT %d\n",
               options.n, processes, matrix_sum(global, options.n),
               maximum_elapsed, correct);
        free(global);
        free(reference);
    }

    free(pivot);
    free(local);
    free(counts);
    free(displacements);
    MPI_Finalize();
    return 0;
}
