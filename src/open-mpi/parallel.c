#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>

#define MAX_N 512

struct Matrix {
    int size;
    double mat[MAX_N][MAX_N];
};

struct FreqMatrix {
    int size;
    double complex mat[MAX_N][MAX_N];
};

void readMatrix(struct Matrix *m) {
    scanf("%d", &(m->size));
    for (int i = 0; i < m->size; i++)
        for (int j = 0; j < m->size; j++)
            scanf("%lf", &(m->mat[i][j]));
}

double complex handleElement(struct Matrix *mat, int k, int l, int i, int j) {
    double complex arg = (k * i / (double) mat->size) + (l * j / (double) mat->size);
    double complex exponent = cexp(-2.0 * I * M_PI * arg);
    double complex element = mat->mat[i][j] * exponent;

    return element;
}

double complex handleRow(struct Matrix *mat, int k, int l, int i) {
    double complex row = 0.0;
    for (int j = 0; j < mat->size; j++) {
        row += handleElement(mat, k, l, i, j);
    }

    return row;
}

double complex handleColumn(struct Matrix *mat, int k, int l) {
    double complex element = 0.0;

    for (int i = 0; i < mat->size; i++) {
        element += handleRow(mat, k, l, i);
    }

    return element;
}

double complex dft(struct Matrix *mat, int k, int l) {
    double complex element = handleColumn(mat, k, l);

    return element / (double) (mat->size*mat->size);
}

void fillFreqMatrix(struct Matrix *mat, struct FreqMatrix *freq_domain) {
    MPI_Init(NULL, NULL);

    freq_domain->size = mat->size;

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    printf("World Size: %d\n", world_size);

    if (world_rank == 0) {
        /* Master Process */
        printf("Master Process %d\n", world_rank);

        int element_per_process = mat->size / world_size;
        int extra_elements = mat->size % world_size;

        /* Send Divisible Process */
        for (int i = 1; i < world_size; i++) {
            int processed_size = i * element_per_process;
            int current_size = processed_size < mat->size ? element_per_process : extra_elements;

            printf("Sending: %d\n", i);

            MPI_Send(&current_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&processed_size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&mat->size, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
            MPI_Send(&mat->mat[processed_size][0], current_size * mat->size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD);
        }

        /* Work on Master Process */
        for (int i = 0; i < element_per_process; i++) {
            for (int j = 0; j < mat->size; j++) {
                freq_domain->mat[i][j] = dft(mat, i, j);
            }
        }

        /* Receive Row */
        for (int i = 1; i < world_size; i++) {
            printf("Waiting: %d\n", i);

            int processed_size = i * element_per_process;
            int current_size = processed_size < mat->size ? element_per_process : extra_elements;

            MPI_Recv(&freq_domain->mat[processed_size][0], current_size * mat->size, MPI_DOUBLE, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }

        printf("Master Process Finished %d\n", world_rank);

        /* Print Result */
        double complex sum = 0.0;
        for (int m = 0; m < mat->size; m++) {
            for (int n = 0; n < mat->size; n++) {
                double complex el = freq_domain->mat[m][n];
                sum += el;
                printf("(%lf, %lf) ", creal(el), cimag(el));
            }
            printf("\n");
        }
        printf("Sum : (%lf, %lf)\n", creal(sum), cimag(sum));

    } else {
        /* Slave Process */
        printf("Slave Process %d\n", world_rank);

        /* Receive Process */
        int elements_received;
        int index_received;

        MPI_Recv(&elements_received, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&index_received, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&mat->size, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&mat->mat[index_received][0], elements_received * mat->size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        /* Work on Slave Process */
        for (int i = index_received; i < index_received + elements_received; i++) {
            for (int j = 0; j < mat->size; j++) {
                freq_domain->mat[i][j] = dft(mat, i, j);
            }
        }

        /* Send Row */
        MPI_Send(&freq_domain->mat[index_received][0], elements_received * mat->size, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);

        printf("Slave Process %d Finished\n", world_rank);
    }

    MPI_Finalize();
}

int main(void) {
    struct Matrix source;
    struct FreqMatrix freq_domain;

    readMatrix(&source);
    fillFreqMatrix(&source, &freq_domain);

    return 0;
}