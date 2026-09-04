#ifndef FLOYD_COMMON_H
#define FLOYD_COMMON_H

#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FLOYD_INF 1000000000
#define FLOYD_TAG 601

typedef struct {
    int n;
    unsigned seed;
    const char *dump_path;
} floyd_options;

static void die_usage(const char *program, const char *message) {
    if (message != NULL) {
        fprintf(stderr, "error: %s\n", message);
    }
    fprintf(stderr, "usage: %s N [seed] [output_matrix.txt]\n", program);
    fprintf(stderr, "  N                  positive matrix dimension\n");
    fprintf(stderr, "  seed               optional deterministic graph seed (default: 1)\n");
    fprintf(stderr, "  output_matrix.txt  optional root-only final matrix dump\n");
    MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
}

static floyd_options parse_options(int argc, char **argv) {
    floyd_options options = {.n = 0, .seed = 1U, .dump_path = NULL};
    if (argc < 2 || argc > 4) {
        die_usage(argv[0], "expected N, with optional seed and output path");
    }

    char *end = NULL;
    long n = strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' || n <= 0 || n > 20000) {
        die_usage(argv[0], "N must be an integer in [1, 20000]");
    }
    options.n = (int)n;

    if (argc >= 3) {
        unsigned long seed = strtoul(argv[2], &end, 10);
        if (*argv[2] == '\0' || *end != '\0' || seed > UINT32_MAX) {
            die_usage(argv[0], "seed must be a non-negative 32-bit integer");
        }
        options.seed = (unsigned)seed;
    }
    if (argc == 4) {
        options.dump_path = argv[3];
    }
    return options;
}

static void checked_matrix_bytes(int n) {
    size_t count = (size_t)n * (size_t)n;
    if (count > SIZE_MAX / sizeof(int)) {
        die_usage("floyd", "matrix allocation would overflow");
    }
}

static int *alloc_matrix(int rows, int n) {
    size_t count = (size_t)rows * (size_t)n;
    int *matrix = malloc((count == 0 ? 1 : count) * sizeof(*matrix));
    if (matrix == NULL) {
        fprintf(stderr, "error: unable to allocate %zu matrix elements\n", count);
        MPI_Abort(MPI_COMM_WORLD, EXIT_FAILURE);
    }
    return matrix;
}

static void row_range(int n, int processes, int rank, int *begin, int *end) {
    *begin = (rank * n) / processes;
    *end = ((rank + 1) * n) / processes;
}

static int row_owner(int n, int processes, int row) {
    for (int rank = 0; rank < processes; ++rank) {
        int begin, end;
        row_range(n, processes, rank, &begin, &end);
        if (begin <= row && row < end) {
            return rank;
        }
    }
    return processes - 1;
}

static void make_graph(int *matrix, int n, unsigned seed) {
    uint64_t state = (uint64_t)seed + UINT64_C(0x9e3779b97f4a7c15);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                matrix[(size_t)i * (size_t)n + (size_t)j] = 0;
            } else {
                state ^= state >> 12;
                state ^= state << 25;
                state ^= state >> 27;
                state *= UINT64_C(2685821657736338717);
                matrix[(size_t)i * (size_t)n + (size_t)j] = (int)(state % 100U) + 1;
            }
        }
    }
}

static void floyd_serial(int *matrix, int n) {
    for (int k = 0; k < n; ++k) {
        const int *pivot = &matrix[(size_t)k * (size_t)n];
        for (int i = 0; i < n; ++i) {
            int *row = &matrix[(size_t)i * (size_t)n];
            if (row[k] >= FLOYD_INF) {
                continue;
            }
            for (int j = 0; j < n; ++j) {
                int candidate = row[k] + pivot[j];
                if (candidate < row[j]) {
                    row[j] = candidate;
                }
            }
        }
    }
}

static long long matrix_sum(const int *matrix, int n) {
    long long sum = 0;
    for (size_t i = 0; i < (size_t)n * (size_t)n; ++i) {
        sum += matrix[i];
    }
    return sum;
}

static int dump_matrix(const char *path, const int *matrix, int n) {
    FILE *file = fopen(path, "w");
    if (file == NULL) {
        perror(path);
        return 0;
    }
    fprintf(file, "%d\n", n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            fprintf(file, "%d%c", matrix[(size_t)i * (size_t)n + (size_t)j],
                    j + 1 == n ? '\n' : ' ');
        }
    }
    fclose(file);
    return 1;
}

static void build_scatter_layout(int n, int processes, int *counts, int *displacements) {
    int offset = 0;
    for (int rank = 0; rank < processes; ++rank) {
        int begin, end;
        row_range(n, processes, rank, &begin, &end);
        counts[rank] = (end - begin) * n;
        displacements[rank] = offset;
        offset += counts[rank];
    }
}

static void update_local_rows_at_k(int *local, int local_rows, int n, int k,
                                   const int *pivot) {
    for (int i = 0; i < local_rows; ++i) {
        int *row = &local[(size_t)i * (size_t)n];
        if (row[k] >= FLOYD_INF) {
            continue;
        }
        for (int j = 0; j < n; ++j) {
            int candidate = row[k] + pivot[j];
            if (candidate < row[j]) {
                row[j] = candidate;
            }
        }
    }
}

static int verify_result(const int *actual, const int *expected, int n) {
    size_t count = (size_t)n * (size_t)n;
    for (size_t i = 0; i < count; ++i) {
        if (actual[i] != expected[i]) {
            fprintf(stderr, "mismatch at flat index %zu: actual=%d expected=%d\n",
                    i, actual[i], expected[i]);
            return 0;
        }
    }
    return 1;
}

#endif
