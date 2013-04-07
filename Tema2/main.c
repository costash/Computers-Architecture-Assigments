#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <cblas.h>
#include "mmio.h"


#define EPS ((double)1.e-3)
#define MAX_ULPS 16048576

#define FALSE 0
#define TRUE 1

/**
 * Gets the time in miliseconds between two timeval structures
 * If one of the parameters is NULL it returns 0
 */
double get_elapsed_time_milisec(const struct timeval *tv1, const struct timeval *tv2) {
	if (tv1 == NULL || tv2 == NULL)
		return 0;
	double elapsed;
	elapsed = (tv2->tv_sec - tv1->tv_sec) * 1000.0;      /* sec to ms */
	elapsed += (tv2->tv_usec - tv1->tv_usec) / 1000.0;   /* us to ms */

	return elapsed;
}

/**
 * Gets the time of the system in a timeval structure
 */
struct timeval get_time() {
	struct timeval tv;

	if (gettimeofday(&tv, NULL) < 0) {
		perror("gettimeofday");
		exit(EXIT_FAILURE);
	}
	return tv;
}

/**
 * Get index for square linearized matrix
 */
inline int get_index(int i, int j, int size) {
	return size * i + j;
}

double rand_double();

/**
 * Generate random square symmetric matrix of given size
 */
double *generate_matrix(int size) {
	double *mat = calloc(size * size, sizeof(double));
	int i, j;
	for (i = 0; i < size; ++i) {
		for (j = 0; j <= i; ++j) {
			double num = rand_double();
			int index = get_index(i, j, size);
			mat[index] = num;
			index = get_index(j, i, size);
			mat[index] = num;
		}
	}

	return mat;
}

/**
 * Loads a sparse matrix from a file in matrix market format
 * into a linearized matrix
 */
int load_matrix(double **mat, int *size, char *file_name) {
	int nr_rows, nr_cols, nr_nonzero;
	double *val;
	int *I, *J;
	int ret_val;

	ret_val = mm_read_unsymmetric_sparse(file_name, &nr_rows, &nr_cols, &nr_nonzero,
			&val, &I, &J);

	if (ret_val != 0) {
		fprintf(stderr, "Could not load matrix\n");
		return -1;
	}

	/* Update size and matrix */
	*size = nr_rows;

	*mat = calloc((*size) * (*size), sizeof(double));
	int i;
	for (i = 0; i < nr_nonzero; ++i) {
		int index = get_index(I[i], J[i], *size);
		(*mat)[index] = val[i];
		index = get_index(J[i], I[i], *size);
		(*mat)[index] = val[i];
	}
	free(I);
	free(J);
	free(val);

	return 0;
}

/**
 * Prints a matrix to a file
 */
void print_matrix(double *mat, int size, FILE *file) {
	int i, j;
	for (i = 0; i < size; ++i) {
		for (j = 0; j < size; ++j) {
			fprintf(file, "%2.14lg ", mat[get_index(i, j, size)]);
		}
		fprintf(file, "\n");
	}
}

/**
 * Print array to a file
 */
void print_array(double *array, int size, FILE *file) {
	int i;
	for (i = 0; i < size; ++i)
		fprintf(file, "%2.14lg ", array[i]);
	fprintf(file, "\n");
}

/**
 * Generate a random floating point number from min to max
 */
double rand_range(double min, double max)
{
    double range = (max - min);
    double div = RAND_MAX / range;
    return min + (rand() / div);
}

/**
 * Generate a random floating point number between LONG_MIN
 * and LONG_MAX
 */
double rand_double() {
	return rand_range(LONG_MIN, LONG_MAX);
}

/**
 * Generate random array of doubles
 */
void rand_array(double **array, int size) {
	*array = calloc(size, sizeof(double));
	int i;
	for (i = 0; i < size; ++i)
		(*array)[i] = rand_double();
}

/**
 * Checks for equality between two doubles, within a precision
 */
int doubles_equal_eps(double a, double b, double epsilon) {
	if (fabs(a - b) < epsilon)
		return TRUE;
	return FALSE;
}

/**
 * Good floating point comparison
 */
int almost_equal_2s_omplement(double a, double b, int max_ulps)
{
    // Make sure maxUlps is non-negative and small enough that the
    // default NAN won't compare as equal to anything.
    assert(max_ulps > 0 && max_ulps < 16 * 1024 * 1024);
    long long a_int = *(long long*)&a;
    // Make aInt lexicographically ordered as a twos-complement int
    if (a_int < 0)
        a_int = 0x8000000000000000 - a_int;
    // Make bInt lexicographically ordered as a twos-complement int
    long long b_int = *(long long*)&b;
    if (b_int < 0)
        b_int = 0x8000000000000000 - b_int;
    long long int_diff = llabs(a_int - b_int);
    if (int_diff <= max_ulps)
        return TRUE;
    return FALSE;
}

/**
 * Wrapper over doubles_equal_eps
 */
int doubles_equal(double a, double b) {
	//return doubles_equal_eps(a, b, EPS);
	return almost_equal_2s_omplement(a, b, MAX_ULPS);
}

/**
 * Check for equality in two arrays of double. Assume same size
 */
int arrays_double_equal(double *arr1, double *arr2, int size) {
	int i;
	for (i = 0; i < size; ++i) {
		if (doubles_equal(arr1[i], arr2[i]) == FALSE) {
			fprintf(stderr, "Not equal: %2.14lg != %2.14lg at element %d\n", arr1[i], arr2[i], i);
			return FALSE;
		}
	}
	return TRUE;
}

/**
 * Executes the operation y := alpha * A * x + beta * y
 * with A being a symetric matrix of size @size, alpha and beta scalars
 * and x and y arrays of size @size
 * This is the non-optimized version
 */
void dsymv_brute(const int size, const double alpha, const double *A,
		const double *x, const double beta, double *y) {
	int i, j;
	for (i = 0; i < size; ++i) {
		double accum = 0;
		for (j = 0; j < size; ++j) {
			int index = get_index(i, j, size);
			accum += alpha * A[index] * x[j];
		}
		accum += beta * y[i];
		y[i] = accum;
	}
}

/**
 * Executes the DSYMV operation, but optimized with access only for half of
 * the matrix using only pointers for matrix instead of indices
 */
void dsymv_optimized(const int size, const double alpha, const double *A,
		const double *x, const double beta, double *y) {
	const double *p_elem = A;

	register double temp_alpha;
	register double accum;
	register double temp_elem;

	int i, j;

	for (i = 0; i < size; ++i) {
		y[i] += beta * y[i];
	}

	/* for j = [0, size) */
	for (j = 0; j < size; ++j) {
		temp_alpha = alpha * x[j];
		accum = 0.;

		/* for i = [0, j) */
		for (i = 0; i < j; ++i) {
			temp_elem = (*(p_elem + i));
			y[i] += temp_alpha * temp_elem;
			accum += temp_elem * x[i];
		}
		temp_elem = (*(p_elem + j));
		y[j] += temp_alpha * temp_elem + alpha * accum;

		p_elem += size;
	}
}

/**
 * Executes the DSYMV operation, using pointers only
 */
void dsymv_optimized_pointers(const int size, const double alpha, const double *A,
		const double *x, const double beta, double *y) {
	const double *p_elem = A;

	register double temp_alpha;
	register double accum;
	register double temp_elem;

	//int i, j;
	double *pyi = y;
	double *py_end = y + size;

	for (; pyi != py_end; ++pyi) {
		*pyi += beta * *pyi;
	}

	double *pyj = y;
	const double *pxj = x;

	const double *pxi;

	const double *p_elem_j;

	/* for j = [0, size) */
	for (; pyj != py_end; ++pyj, ++pxj, p_elem += size) {
		temp_alpha = alpha * *pxj;
		accum = 0.;

		/* for i = [0, j) */
		for (pxi = x, pyi = y, p_elem_j = p_elem; pyi != pyj; ++pyi, ++pxi, ++p_elem_j) {
			temp_elem = *p_elem_j;
			*pyi += temp_alpha * temp_elem;
			accum += temp_elem * *pxi;
		}
		temp_elem = *p_elem_j;
		*pyj += temp_alpha * temp_elem + alpha * accum;
	}
}


int main(int argc, char* argv[]) {
	struct timeval tv1, tv2;
	double *A;		/* Matrix to be multiplied with vector */
	double *x;		/* Vector to be multiplied */
	double alpha, beta;	/* Scalars */
	double *y_generated;
	double *y_brute;
	double *y_blas;
	double *y_optimized;
	int SIZE;

	srand(42);

	if (argc < 2 && argc > 3) {
		fprintf(stderr, "Usage: %s test_name\n", __FILE__);
		exit(EXIT_FAILURE);
	}

	if (strcmp(argv[1], "RANDOM") != 0) {
		/* Load matrix A from file */
		tv1 = get_time();

		load_matrix(&A, &SIZE, argv[1]);

		tv2 = get_time();
		//printf("Loaded matrix of size %d in %lf miliseconds\n", SIZE, get_elapsed_time_milisec(&tv1, &tv2));
		printf("Running test %s of size %d\n", argv[1], SIZE);
		printf("Loaded matrix in %lf miliseconds\n", get_elapsed_time_milisec(&tv1, &tv2));
	}
	else {
		/* Generate random symmetric matrix of size given by argv[2] */
		SIZE = atoi(argv[2]);
		assert(SIZE > 0 && SIZE < 100000);
		tv1 = get_time();

		A = generate_matrix(SIZE);

		tv2 = get_time();
		printf("Running test %s of size %d\n", argv[1], SIZE);
		printf("Generated matrix in %lf miliseconds\n", get_elapsed_time_milisec(&tv1, &tv2));
	}

	/* Generate arrays x, y and scalars alpha and beta */
	tv1 = get_time();

	rand_array(&x, SIZE);
	rand_array(&y_generated, SIZE);
	alpha = rand_double();
	beta = rand_double();

	y_brute = calloc(SIZE, sizeof(double));
	memcpy(y_brute, y_generated, SIZE * sizeof(double));
	y_blas = calloc(SIZE, sizeof(double));
	memcpy(y_blas, y_generated, SIZE * sizeof(double));
	y_optimized = calloc(SIZE, sizeof(double));
	memcpy(y_optimized, y_generated, SIZE * sizeof(double));

	tv2 = get_time();
	//printf("Generated arrays x and y of size %d in %lf miliseconds\n", SIZE, get_elapsed_time_milisec(&tv1, &tv2));
	printf("Generated arrays in %lf miliseconds\n", get_elapsed_time_milisec(&tv1, &tv2));

	//print_matrix(A, SIZE, stdout);

	/* Generate arrays x, y and scalars alpha and beta */
	tv1 = get_time();

	dsymv_brute(SIZE, alpha, A, x, beta, y_brute);

	tv2 = get_time();
	printf("dsymv_brute in %lf miliseconds\n", get_elapsed_time_milisec(&tv1, &tv2));
	fflush(stdout);

	//print_array(y_brute, SIZE, stdout);

	/* Generate arrays x, y and scalars alpha and beta */
	tv1 = get_time();

	dsymv_optimized_pointers(SIZE, alpha, A, x, beta, y_optimized);

	//print_array(y_optimized, SIZE, stdout);

	tv2 = get_time();
	printf("dsymv_optimized in %lf miliseconds\n", get_elapsed_time_milisec(&tv1, &tv2));
	fflush(stdout);

	/* Execute BLAS implementation */
	tv1 = get_time();
	cblas_dsymv (CblasRowMajor,
			CblasUpper,
			SIZE,
			alpha,
			A,
			SIZE,
			x,
			1,
			beta,
			y_blas,
			1);
	tv2 = get_time();
	printf("dsymv_BLAS in %lf miliseconds\n", get_elapsed_time_milisec(&tv1, &tv2));
	fflush(stdout);

	int equal = arrays_double_equal(y_brute, y_blas, SIZE);
	fprintf(stdout, "Results are %s\n", equal == TRUE? "Equal" : "Not equal");

	if (!equal) {
		fprintf(stderr, "Test %s failed\n", argv[1]);
		fprintf(stderr, "Brute array:\n");
		print_array(y_brute, SIZE, stderr);
		fprintf(stderr, "Blas array:\n");
		print_array(y_blas, SIZE, stderr);
	}

	equal = arrays_double_equal(y_optimized, y_blas, SIZE);
	fprintf(stdout, "Results are %s\n", equal == TRUE? "Equal" : "Not equal");
	if (!equal) {
		fprintf(stderr, "Test %s failed\n", argv[1]);
		fprintf(stderr, "Optimized array:\n");
		print_array(y_optimized, SIZE, stderr);
		fprintf(stderr, "Blas array:\n");
		print_array(y_blas, SIZE, stderr);
	}

	free(A);
	free(x);
	free(y_generated);
	free(y_brute);
	free(y_blas);
	free(y_optimized);

	return 0;
}
