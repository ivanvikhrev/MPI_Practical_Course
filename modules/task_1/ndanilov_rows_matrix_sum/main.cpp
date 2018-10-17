#include <mpi.h>
#include <iostream>
#include <assert.h>
#include <cstdlib>
#include <time.h>

#define MASTER_PROCESS_ID 0
#define DEFAULT_ROWS_COUNT 7
#define ROWS_NUM (rows / proc_num)
#define BUSY_ELEMENTS (rows_num * (proc_num - 1) * cols)
#define REST_ELEMENTS (mtrx_size - BUSY_ELEMENTS)
#define DEFINITELY_MAGIC_NUMBER 42
#define MAX_OUTPUT_SIZE 25

int main(int argc, char* argv[])
{
	/* Mpi variables */
	int status, 
	    rank,
	    size,
	    proc_num;

	/* Time variables */
	double begin_time = 0,
	       end_time = 0;

	/* Usual variables */
	int  cols, rows,
	     mtrx_size,
	     rows_num,
	     i, j;

	double *matrix = NULL,
	       *part_of_matrix,
	       *result_vector,
	       *part_of_vector;

	/* Reading a number of rows and cols from argv */
	if (argc > 2) {
		rows = atoi(argv[1]);
		cols =  atoi(argv[2]);
	} else {
		rows = cols = DEFAULT_ROWS_COUNT;
	}

	mtrx_size = rows * cols;

	/* Mpi init block */
	status = MPI_Init(&argc, &argv);
	if (status)
		goto err_exit;

	status = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	if (status)
		goto err_exit;

	status = MPI_Comm_size(MPI_COMM_WORLD, &size);
	if (status)
		goto err_exit;

	/* Maximum number of processes is 42 because
	 * The Ultimate Question of Life, the Universe, and Everything
	 */
	if (size > DEFINITELY_MAGIC_NUMBER) {
		status = -1;
		goto err_exit;
	}


	proc_num = (rows >= size) ? size : rows;
	rows_num = ROWS_NUM;

	/* Sending part */
	if (rank == MASTER_PROCESS_ID)
	{
		matrix = new double[mtrx_size];
		result_vector = new double[rows];
		part_of_vector = new double[rows_num * cols];
		part_of_matrix = new double[rows_num * cols];

		std::srand((unsigned)time(NULL));

		for (i = 0; i < mtrx_size; i++)
			matrix[i] = (std::rand() % 100);

		if (mtrx_size < OutputSize) {
			for (i = 0; i < mtrx_size; i++) {
				std::cout << matrix[i] << " ";

				if((i + 1) % cols == 0)
					std::cout << std::endl;
			}
		}

		std::cout << std::endl;
		part_of_matrix = matrix;

		/* One process calculations */
		begin_time = MPI_Wtime();

		for (i = 0; i < mtrx_size; i++)
			result_vector[i / cols] += matrix[i];

		end_time = MPI_Wtime();

		std::cout << "Sequential Time: " << end_time - begin_time << std::endl;
		std::cout << "Sequential Sum = " << std::endl;

		for (i = 0; i < rows; i++)
			std::cout << result_vector[i] << " ";

		std::cout << std::endl;

		/* Let's start MPI magic */
		begin_time = MPI_Wtime();

		for (i = 1; i < proc_num - 1; i++)
			status = MPI_Send(&(matrix[i * rows_num * cols]),
					  rows_num * cols, MPI_DOUBLE, i, i,
					  MPI_COMM_WORLD);

		/* In the case of rows % proc_num! = 0,
		 * send the rest to the last process
		 */
		if (size != 1)
			MPI_Send(&matrix[BUSY_ELEMENTS],
				 REST_ELEMENTS,
				 MPI_DOUBLE, proc_num - 1,
				 proc_num - 1, MPI_COMM_WORLD);

	} else {
		if (rank == proc_num - 1) {
			rows_num = REST_ELEMENTS / cols;
			part_of_matrix = new double[rows_num * cols];
			part_of_vector = new double[rows_num];
			MPI_Recv(part_of_matrix, rows_num * cols, MPI_DOUBLE,
				 MASTER_PROCESS_ID, rank, MPI_COMM_WORLD,
				 MPI_STATUSES_IGNORE);
		} else {
			part_of_matrix = new double[rows_num * cols];
			part_of_vector = new double[rows_num];
			MPI_Recv(part_of_matrix, rows_num * cols, MPI_DOUBLE,
				 MASTER_PROCESS_ID, rank, MPI_COMM_WORLD,
				 MPI_STATUSES_IGNORE);
		}
	}

	/* Common calculating */
	for (i = 0; i < rows_num * cols; i++) {
		part_of_vector[i / cols] += part_of_matrix[i];
	}

	/* resulting vector collecting */
	if (rank == MASTER_PROCESS_ID) {
		/* Flush result vector */
		for (i = 0; i < rows; i++)
			result_vector[i] = 0;

		/* Fill in MASTER part */
		for (i = 0; i < rows_num; i++)
			result_vector[i] += part_of_vector[i];

		/* Fill in other parts */
		for (i = 1; i < proc_num; i++) {
			if (i == proc_num - 1) {
				rows_num = REST_ELEMENTS / cols;
				part_of_matrix = new double[rows_num * cols];
				MPI_Recv(part_of_matrix, rows_num, MPI_DOUBLE,
					 i , 0, MPI_COMM_WORLD,
					 MPI_STATUSES_IGNORE);
			} else {
				rows_num = ROWS_NUM;
				part_of_matrix = new double[rows_num * cols];
				MPI_Recv(part_of_matrix, rows_num, MPI_DOUBLE,
					 i , 0, MPI_COMM_WORLD,
					 MPI_STATUSES_IGNORE);
			}

			for (j = 0; j < rows_num; j++)
				result_vector[(ROWS_NUM) * i + j] += part_of_matrix[j];

			delete[] part_of_matrix;
		}

		end_time = MPI_Wtime();
		std::cout << std::endl;
		std::cout << "Parallel Time: " << end_time - begin_time << std::endl;
		std::cout << "Parallel Sum = " << std::endl;

		for (i = 0; i < rows; i++)
			std::cout << result_vector[i] << " ";

		std::cout << std::endl;
	} else {
		MPI_Send(part_of_vector, rows_num, MPI_DOUBLE,
			 MASTER_PROCESS_ID, 0, MPI_COMM_WORLD);
	}

	/* Memory free */
	if (rank == 0)
		delete[] matrix;
	else
		delete[] part_of_matrix;

	status = MPI_Finalize();
	if (status)
		goto err_exit;

	return 0;

err_exit:
	return status;
}
