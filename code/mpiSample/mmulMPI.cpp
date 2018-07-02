// Simple naive matrix multiplication.
// author: Schuchardt Martin, csap9442
// compile: mpicc -O3 -std=c99 -Wall -Werror -lm  mmul.c mmulMPI.c -o mmulMPI -DVERIFY=1

#include "mmul.h"
#include <mpi.h>
#include <iomanip>
#include <string>
#include "../utils/time_ms.h"


using namespace std;


void multiplyMPI(int *A, int rank, int ROWS, int *B, int *C) {
    for (int i = 0; i < ROWS; ++i) {
        for (int j = 0; j < N; ++j) {
            int sum = 0;
            for (int k = 0; k < N; ++k) {
                sum += *(A + i*N + k) * *(B + k*N + j);
            }
            *(C + i*N + j) = sum;
        }
    }
}

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    cout << "size: " << size << ", rank: " << rank << endl;
    
    if (argc == 2) {
        N = atoi(argv[1]);
    }
    cout << "Matrix multiplication, using " << N << "x" << N << " matrix (int)" << endl;
	if (N%size != 0) {
		cout << "ERROR: matrix size needs to be a multiple of MPI size (" << to_string(size) << ")" << endl;
		return EXIT_FAILURE;
	}

    int *A = new int[N*N];
    int *B = new int[N*N];
    int *C = new int[N*N];

    int ROWS = N/size;
    int *myA = new int[ROWS*N];
    int *myC = new int[ROWS*N];

    if (rank == 0) {
        initMatrices(A, B);
        cout << "  executing..." << endl;
    }
    unsigned long long start_time = time_ms();

    MPI_Scatter(A, N*ROWS, MPI_INT, myA, N*ROWS, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(B, N*N, MPI_INT, 0, MPI_COMM_WORLD);

    multiplyMPI(myA, rank, ROWS, B, myC);

    MPI_Gather(myC, N*ROWS, MPI_INT, C, N*ROWS, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        unsigned long long duration = time_ms()-start_time;
        cout << "  elapsed time for MPI matrix multiplication: \t" << setw(6) << duration << "  ms" << endl;
        testResults(A, C);
    }

    MPI_Finalize();
    delete A;
    delete B;
    delete C;

	delete myA;
	delete myC;
}
