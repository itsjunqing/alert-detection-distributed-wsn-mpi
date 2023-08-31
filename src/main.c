#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>

#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1

int baseStation(MPI_Comm commWorld);
int node(MPI_Comm commWorld, MPI_Comm comm, int rows, int cols);


int main(int argc, char *argv[]) {

	int size, rank, color;
	int rows = 0, cols = 0;
	MPI_Comm newComm;

	// setup initial MPI environment
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	// parse command line arguments
	if (argc == 3) {
		rows = atoi(argv[1]);
		cols = atoi(argv[2]);
		// output error message if size given is not the same
		if ((rows*cols) != size-1) {
			if (rank == 0) printf("ERROR: (rows * cols) + 1 = (%d * %d) + 1 = %d != %d\n", rows, cols, (rows*cols)+1, size);
			MPI_Finalize();
			return 0;
		}
	} else {
		printf("NOTE: No rows and cols provided, rows and cols will be automatically assigned.\n");
	}

	color = !(rank == size-1); // set last rank color = 0, others = 1
	MPI_Comm_split(MPI_COMM_WORLD, color, 0, &newComm);

	if (rank == size-1) {
		baseStation(MPI_COMM_WORLD);
	} else {
		node(MPI_COMM_WORLD, newComm, rows, cols);
	}


	MPI_Finalize();
	return 0;
}


int baseStation(MPI_Comm commWorld) {

	
	/**
	 * base station code here
	 */
	

	return 0;
}


int node(MPI_Comm commWorld, MPI_Comm comm, int rows, int cols) {
	/**
	 * commWorld: includes base station, to be used for getting last rank id (base) and sending messages to base station
	 * comm: excludes base station, to be used to create cartesian topology
	 */

	// setup variables
	int rank, cartRank, size, reorder, ierr;
	int ndims = 2;
	int leftRank, rightRank, topRank, bottomRank;
	int coord[ndims], wrapAround[ndims], dims[ndims];
	MPI_Comm cartComm;

	// setup assignments 
	MPI_Comm_size(comm, &size);
	MPI_Comm_rank(comm, &rank);
	dims[0] = rows; // automatically set to 0 if no CL arguments
	dims[1] = cols;
	wrapAround[0] = wrapAround[1] = 0; // zero periodic shift 
	reorder = 1;
	ierr = 0;

	// create cartesian mapping
	MPI_Dims_create(size, ndims, dims);
	if (rank == 0) 
		printf("Root Rank: %d. Comm Size: %d: Grid Dimension = [%d x %d] \n", rank, size, dims[0], dims[1]);

	ierr = MPI_Cart_create(comm, ndims, dims, wrapAround, reorder, &cartComm);
	if(ierr != 0) printf("ERROR[%d] creating CART\n", ierr);

	// get the rank coordinates
	MPI_Cart_coords(cartComm, rank, ndims, coord);
	MPI_Cart_rank(cartComm, coord, &cartRank); // Disclaimer: cartRank is for vertification only, it's the same as rank

	// getting the rank of the neighbours
	MPI_Cart_shift(cartComm, SHIFT_ROW, DISP, &topRank, &bottomRank);
	MPI_Cart_shift(cartComm, SHIFT_COL, DISP, &leftRank, &rightRank);
	
	// Testing
	printf("Global rank: %d. Cart rank: %d. Coord: (%d, %d). Left: %d. Right: %d. Top: %d. Bottom: %d\n", rank, cartRank, coord[0], coord[1], leftRank, rightRank, topRank, bottomRank);


	/**
	 * node code here
	 */



	MPI_Comm_free(&cartComm);
	return 0;
}


