/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp);
 * A is the source matrix, B is the destination
 * tmp points to a region of memory able to hold TMPCOUNT (set to 256) doubles as temporaries
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 2KB direct mapped cache with a block size of 64 bytes.
 *
 * Programming restrictions:
 *   No out-of-bounds references are allowed
 *   No alterations may be made to the source array A
 *   Data in tmp can be read or written
 *   This file cannot contain any local or global doubles or arrays of doubles
 *   You may not use unions, casting, global variables, or 
 *     other tricks to hide array data in other forms of local or global memory.
 */ 
#include <stdio.h>
#include <stdbool.h>
#include "cachelab.h"
#include "contracts.h"

/* Forward declarations */
bool is_transpose(size_t M, size_t N, double A[N][M], double B[M][N]);
void trans(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp);
void trans_tmp(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded.
 */
char transpose_submit_desc[] = "Transpose submission";


void blockTranspose32(size_t M, size_t N, double A[N][M], double B[M][N], 
size_t I, size_t J, size_t CI, size_t CJ){
	if(I != J){
		//different I and J will map to different cache
		//just transfer the datas
		for(int i = 0; i < 8; i++){
			for(int j = 0; j < 8; j++){
				B[8*J+j][8*I+i] = A[8*I+i][8*J+j];
			}
		}
	}
	else{
		//same I and J, use the B used at next block to serve as cache
		//B[8*CI][8*CJ] is the start of the cache block
		for(int i = 0; i < 8; i++){
			for(int j = 0; j < 8; j++){
				B[8*CI+i][8*CJ+j] = A[8*I+i][8*J+j];
			}
		}
		for(int i = 0; i < 8; i++){
			for(int j = 0; j < 8; j++){
				B[8*J+j][8*I+i] = B[8*CI+i][8*CJ+j];
			}
		}
	}
}


void blockTranspose64(size_t M, size_t N, double A[N][M], double B[M][N], 
double *tmp, int tag, size_t I, size_t J, size_t C1I, size_t C1J, size_t C2I, 
size_t C2J, size_t C3I, size_t C3J){
	//tag: at the last block we should use tmp
	//at this time, tag = 1
	//else, tag = 0
	if(I == J){
		//source and target map to same block
		//use the next two B blocks to cache first and last 4*8
		//of A blocks
		for(int i = 0; i < 4; i++){
			for(int j = 0; j < 8; j++){
				B[8*C3I+i][8*C3J+j] = A[8*I+i][8*J+j];
			}
		}
		for(int i = 0; i < 4; i++){
			for(int j = 0; j < 8; j++){
				B[8*C1I+i][8*C1J+j] = A[8*I+i+4][8*J+j];
			}
		}
		for(int j = 0; j < 8; j++){
			for(int i = 0; i < 4; i++){
				B[8*J+j][8*I+i] = B[8*C3I+i][8*C3J+j];
			}
			for(int i = 0; i < 4; i++){
				B[8*J+j][8*I+i+4] = B[8*C1I+i][8*C1J+j];
			}
		}
	}
	else if(!tag){
		//source and target not map to same in cache;
		//and not the last one
		//only use the next B block as cache
		for(int i = 0; i < 4; i++){
			for(int j = 0; j < 8; j++){
				B[8*C1I+i][8*C1J+j] = A[8*I+i+4][8*J+j];
			}
		}
		for(int j = 0; j < 8; j++){
			for(int i = 0; i < 4; i++){
				B[8*J+j][8*I+i] = A[8*I+i][8*J+j];
			}
			for(int i = 0; i < 4; i++){
				B[8*J+j][8*I+i+4] = B[8*C1I+i][8*C1J+j];
			}
		}
	}
	else{
		//the last one is A[6][7] and B[7][6]
		//so we can use the 0-series of tmp
		//i.e. 0~7, 64~71, 128~135, 192~199
		for(int i = 0; i < 4; i++){
			for(int j = 0; j < 8; j++){
				tmp[64*i+j] = A[8*I+i+4][8*J+j];
			}
		}
		for(int j = 0; j < 8; j++){
			for(int i = 0; i < 4; i++){
				B[8*J+j][8*I+i] = A[8*I+i][8*J+j];
			}
			for(int i = 0; i < 4; i++){
				B[8*J+j][8*I+i+4] = tmp[64*i+j];
			}
		}
	}
}

void blockTranspose6365(size_t M, size_t N, double A[N][M], double B[M][N], 
double *tmp, size_t blockM, size_t blockN){
	//use block size blockM * blockN
	//we practically choose blockM = 4, blockN = 16
	int hg = (N-1)/blockN + 1;//horizontal block number
	int zg = (M-1)/blockM + 1;//vertical block number
	for(int i = 0; i < hg; i++){
		for(int j = 0; j < zg; j++){
			for(int k = 0; k < blockN && i*blockN+k < N; k++){
				for(int l = 0; l < blockM && j*blockM+l < M; l++){
					B[j*blockM+l][i*blockN+k] = A[i*blockN+k][j*blockM+l];
				}
			}
		}
	}
}

void transpose_submit(size_t M, size_t N, double A[N][M], 
double B[M][N], double *tmp){
    /*
     * This is a good place to call your favorite transposition functions
     * It's OK to choose different functions based on array size, but
     * your code must be correct for all values of M and N
     */
    if(M==32 && N==32){
	//we make calls to blockTranspose32 in specific order.
	//to ensure that each I==J block is called with
	//the next prepared B block serving as cache.
	blockTranspose32(M,N,A,B,0,0,0,1);
	blockTranspose32(M,N,A,B,1,0,0,1);
	blockTranspose32(M,N,A,B,2,0,0,1);
	blockTranspose32(M,N,A,B,3,0,0,1);
	blockTranspose32(M,N,A,B,0,1,0,1);
	blockTranspose32(M,N,A,B,1,1,1,2);
	blockTranspose32(M,N,A,B,2,1,0,1);
	blockTranspose32(M,N,A,B,3,1,0,1);
	blockTranspose32(M,N,A,B,0,2,0,1);
	blockTranspose32(M,N,A,B,1,2,0,1);
	blockTranspose32(M,N,A,B,2,2,2,3);
	blockTranspose32(M,N,A,B,3,2,0,1);
	blockTranspose32(M,N,A,B,0,3,0,1);
	blockTranspose32(M,N,A,B,1,3,0,1);
	blockTranspose32(M,N,A,B,3,3,3,2);
	blockTranspose32(M,N,A,B,2,3,0,1);
    }
    else if(M==64 && N==64){
	//we make calls to blockTranspose64 in specific order.
	//to ensure that each B block is used immediately
	//(i.e. not contaminated again) after serving as cache.
	blockTranspose64(M,N,A,B,tmp,0,0,0,0,1,0,2,0,3);
	blockTranspose64(M,N,A,B,tmp,0,1,0,0,2,0,3,0,0);
	blockTranspose64(M,N,A,B,tmp,0,2,0,0,3,0,4,0,0);
	blockTranspose64(M,N,A,B,tmp,0,3,0,0,4,0,5,0,0);
	blockTranspose64(M,N,A,B,tmp,0,4,0,0,5,0,6,0,0);
	blockTranspose64(M,N,A,B,tmp,0,5,0,0,6,0,7,0,0);
	blockTranspose64(M,N,A,B,tmp,0,6,0,0,7,1,2,0,0);
	blockTranspose64(M,N,A,B,tmp,0,7,0,1,2,1,3,0,0);
	blockTranspose64(M,N,A,B,tmp,0,1,1,1,2,1,3,1,4);
	blockTranspose64(M,N,A,B,tmp,0,2,1,1,3,1,4,0,0);
	blockTranspose64(M,N,A,B,tmp,0,3,1,1,4,1,5,0,0);
	blockTranspose64(M,N,A,B,tmp,0,4,1,1,5,1,6,0,0);
	blockTranspose64(M,N,A,B,tmp,0,5,1,1,6,1,7,0,0);
	blockTranspose64(M,N,A,B,tmp,0,6,1,1,7,1,0,0,0);
	blockTranspose64(M,N,A,B,tmp,0,7,1,1,0,2,3,0,0);
	blockTranspose64(M,N,A,B,tmp,0,0,1,2,3,2,4,0,0);
	blockTranspose64(M,N,A,B,tmp,0,2,2,2,3,2,4,2,5);
	blockTranspose64(M,N,A,B,tmp,0,3,2,2,4,2,5,0,0);
	blockTranspose64(M,N,A,B,tmp,0,4,2,2,5,2,6,0,0);
	blockTranspose64(M,N,A,B,tmp,0,5,2,2,6,2,7,0,0);
	blockTranspose64(M,N,A,B,tmp,0,6,2,2,7,2,0,0,0);
	blockTranspose64(M,N,A,B,tmp,0,7,2,2,0,2,1,0,0);
	blockTranspose64(M,N,A,B,tmp,0,0,2,2,1,3,4,0,0);
	blockTranspose64(M,N,A,B,tmp,0,1,2,3,4,3,5,0,0);
	blockTranspose64(M,N,A,B,tmp,0,3,3,3,4,3,5,3,6);
	blockTranspose64(M,N,A,B,tmp,0,4,3,3,5,3,6,0,0);
	blockTranspose64(M,N,A,B,tmp,0,5,3,3,6,3,7,0,0);
	blockTranspose64(M,N,A,B,tmp,0,6,3,3,7,3,0,0,0);
	blockTranspose64(M,N,A,B,tmp,0,7,3,3,0,3,1,0,0);
	blockTranspose64(M,N,A,B,tmp,0,0,3,3,1,3,2,0,0);
	blockTranspose64(M,N,A,B,tmp,0,1,3,3,2,4,5,0,0);
	blockTranspose64(M,N,A,B,tmp,0,2,3,4,5,4,6,0,0);
	blockTranspose64(M,N,A,B,tmp,0,4,4,4,5,4,6,4,7);
	blockTranspose64(M,N,A,B,tmp,0,5,4,4,6,4,7,0,0);
	blockTranspose64(M,N,A,B,tmp,0,6,4,4,7,4,0,0,0);
	blockTranspose64(M,N,A,B,tmp,0,7,4,4,0,4,1,0,0);
	blockTranspose64(M,N,A,B,tmp,0,0,4,4,1,4,2,0,0);
	blockTranspose64(M,N,A,B,tmp,0,1,4,4,2,4,3,0,0);
	blockTranspose64(M,N,A,B,tmp,0,2,4,4,3,5,6,0,0);
	blockTranspose64(M,N,A,B,tmp,0,3,4,5,6,5,7,0,0);
	blockTranspose64(M,N,A,B,tmp,0,5,5,5,6,5,7,5,0);
	blockTranspose64(M,N,A,B,tmp,0,6,5,5,7,5,0,0,0);
	blockTranspose64(M,N,A,B,tmp,0,7,5,5,0,5,1,0,0);
	blockTranspose64(M,N,A,B,tmp,0,0,5,5,1,5,2,0,0);
	blockTranspose64(M,N,A,B,tmp,0,1,5,5,2,5,3,0,0);
	blockTranspose64(M,N,A,B,tmp,0,2,5,5,3,5,4,0,0);
	blockTranspose64(M,N,A,B,tmp,0,3,5,5,4,6,7,0,0);
	blockTranspose64(M,N,A,B,tmp,0,4,5,6,7,6,0,0,0);
	blockTranspose64(M,N,A,B,tmp,0,6,6,6,7,6,0,6,1);
	blockTranspose64(M,N,A,B,tmp,0,7,6,6,0,6,1,0,0);
	blockTranspose64(M,N,A,B,tmp,0,0,6,6,1,6,2,0,0);
	blockTranspose64(M,N,A,B,tmp,0,1,6,6,2,6,3,0,0);
	blockTranspose64(M,N,A,B,tmp,0,2,6,6,3,6,4,0,0);
	blockTranspose64(M,N,A,B,tmp,0,3,6,6,4,6,5,0,0);
	blockTranspose64(M,N,A,B,tmp,0,4,6,6,5,7,0,0,0);
	blockTranspose64(M,N,A,B,tmp,0,5,6,7,0,7,1,0,0);
	blockTranspose64(M,N,A,B,tmp,0,7,7,7,0,7,1,7,2);
	blockTranspose64(M,N,A,B,tmp,0,0,7,7,1,7,2,0,0);
	blockTranspose64(M,N,A,B,tmp,0,1,7,7,2,7,3,0,0);
	blockTranspose64(M,N,A,B,tmp,0,2,7,7,3,7,4,0,0);
	blockTranspose64(M,N,A,B,tmp,0,3,7,7,4,7,5,0,0);
	blockTranspose64(M,N,A,B,tmp,0,4,7,7,5,7,6,0,0);
	blockTranspose64(M,N,A,B,tmp,0,5,7,7,6,0,0,0,0);
	blockTranspose64(M,N,A,B,tmp,1,6,7,0,0,0,0,0,0);
	//the last one should use tmp as cache
    }
    else if(M==63 && N==65){
	//a simple 4 * 16 blocking is sufficient
	size_t blockM = 4, blockN = 16;
	blockTranspose6365(M,N,A,B,tmp,blockM,blockN);
    }
    else{
	//other cases
	trans(M,N,A,B,tmp);
    }
}
/* 
 * You can define additional transpose functions below. We've defined
 * some simple ones below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";

/*
 * The following shows an example of a correct, but cache-inefficient
 *   transpose function.  Note the use of macros (defined in
 *   contracts.h) that add checking code when the file is compiled in
 *   debugging mode.  See the Makefile for instructions on how to do
 *   this.
 *
 *   IMPORTANT: Enabling debugging will significantly reduce your
 *   cache performance.  Be sure to disable this checking when you
 *   want to measure performance.
 */
void trans(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp)
{
    size_t i, j;

    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            B[j][i] = A[i][j];
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * This is a contrived example to illustrate the use of the temporary array
 */

char trans_tmp_desc[] =
    "Simple row-wise scan transpose, using a 2X2 temporary array";

void trans_tmp(size_t M, size_t N, double A[N][M], double B[M][N], double *tmp)
{
    size_t i, j;
    /* Use the first four elements of tmp as a 2x2 array with row-major ordering. */
    REQUIRES(M > 0);
    REQUIRES(N > 0);

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
	    int di = i%2;
	    int dj = j%2;
            tmp[2*di+dj] = A[i][j];
            B[j][i] = tmp[2*di+dj];
        }
    }    

    ENSURES(is_transpose(M, N, A, B));
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 
    registerTransFunction(trans_tmp, trans_tmp_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
bool is_transpose(size_t M, size_t N, double A[N][M], double B[M][N])
{
    size_t i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return false;
            }
        }
    }
    return true;
}

