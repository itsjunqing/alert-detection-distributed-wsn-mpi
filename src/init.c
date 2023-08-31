#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>


#include "./init.h"
#include "./node.h"
#include "./base.h"


int main(int argc, char *argv[]) {
	/**
	 * Main program 
	 */
	int rank, size, color;
	MPI_Comm newComm;

	// Initialize MPI
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	// Parse command line arguments
	if (argc == 3) {
		rows = atoi(argv[1]);
		cols = atoi(argv[2]);
		
		// Output error message if size given is not the same
		if ((rows*cols) != size-1) {
			if (rank == 0) {
				printf("ERROR: (rows * cols) + 1 = (%d * %d) + 1 = %d != %d\n", rows, cols, (rows*cols)+1, size);
				printf("HELPER: mpirun -np <no-of-processes> --oversubscribe wsn <rows> <cols>\n");
			}
			MPI_Finalize();
			return 0;
		}	
	} else {
		if (rank == 0) {
			printf("NOTE: No rows and cols provided, please provide rows and cols.\n");
			printf("HELPER: mpirun -np <no-of-processes> --oversubscribe wsn <rows> <cols>\n");
		}
		MPI_Finalize();
		return 0;
	}

	// Get inputs from user
	getUserInputs(MPI_COMM_WORLD, rank, size);

	// Split the sensor nodes (into color 0) and base station (into color 1)
	color = (rank == 0);
	MPI_Comm_split(MPI_COMM_WORLD, color, 0, &newComm);

	// Initialize MPI Datatypes
	initAlertType(&AlertType);
	initNodeInfoType(&NodeInfoType);

	// Execute the base or node function respectively
	if (rank == 0) {
		base(MPI_COMM_WORLD, newComm);
	} else {
		node(MPI_COMM_WORLD, newComm);
	}

	// Finalize the MPI program
	MPI_Finalize();
	return 0;
	
}

void printGuide() {
	printf("===========================================================================\n");
	printf("This guide describes how to navigate and run the code efficiently.\n");
	printf("This program has 3 user-defined parameters which alters the results of the simulation:\n");
	printf("1) nodeInterval: the duration of each iteration for each sensor node; the smaller the nodeInterval, the more reports sent\n");
	printf("2) baseInterval: the duration of each iteration for base station; the smaller the baseInterval, the more reports received\n");
	printf("3) baseIterationsCount: total number of iterations for base station to run\n");	
	printf("Program will use the default values if these parameters are not provided.\n");
	printf("While running, type \"stop\" to end the program\n");
	printf("===========================================================================\n");

}
	

void getUserInputs(MPI_Comm commWorld, int rank, int size) {
	/**
	 * Gets the user inputs from the start of the program
	 */
	
	int baseRank = 0;
	
	// let base station to get input from user 
	if (rank == baseRank) {
		// Print program guide
		printGuide();

		// Gets response
		char response;
		printf("Creating a grid size of (%d x %d) using rank 0 to rank %d\n", rows, cols, size-2);
		fflush(stdout);

		printf("Creating base station using rank %d\n", size-1);
		fflush(stdout);

		printf("Do you want to provide simulation inputs or use default values? (y/n): ");
		fflush(stdout);

		fflush(stdin);
		scanf("%s",&response);

		if (response == 'y') {
			// Get the duration of each interval for node
			printf("Getting node simulation details.....\n");
			fflush(stdout);
			printf("How long does each iteration (seconds) for the node run? (eg: 0.2, 0.5, etc): ");
			fflush(stdout);
			scanf("%f", &nodeInterval);

			// Get the duration of each interval for base station
			printf("Getting base station simulation details.....\n");
			fflush(stdout);
			printf("How long does each iteration (seconds) for the base station run? (eg: 0.2, 0.5, etc): ");
			fflush(stdout);
			scanf("%f", &baseInterval);

			// Get the iterations count for base station
			printf("How many iterations (integer) does the base station run? (eg: 10, 20, etc): ");
			fflush(stdout);			
			scanf("%d", &baseIterationsCount);

		} else {
			// Use default values
			nodeInterval = 0.5;
			baseInterval = 1.0;
			baseIterationsCount = 20;
		}

		printf("Summary:\n");
		printf("Iteration interval for sensor nodes: %.2fs\n", nodeInterval);
		printf("Iteration interval for base station: %.2fs\n", baseInterval);
		printf("Total number of iterations to be run: %d\n", baseIterationsCount);
		printf("Program will now start running...\n");
		printf("===========================================================================\n");
		fflush(stdout);
	}

	// Broadcast these value to all sensor nodes
	MPI_Bcast(&nodeInterval, 1, MPI_FLOAT, baseRank, commWorld);
	MPI_Bcast(&baseIterationsCount, 1, MPI_INT, baseRank, commWorld);
	MPI_Bcast(&baseInterval, 1, MPI_FLOAT, baseRank, commWorld);

	// Wait for all processes to complete
	MPI_Barrier(MPI_COMM_WORLD);
}

 
void initAlertType(MPI_Datatype* AlertType) {
	/**
	 * Initializes MPI datatype for Alert struct
	 */	
	
	int alertBlockLen[3] = {1, 1, 1};
	MPI_Datatype alertTypes[3] = {MPI_LONG, MPI_INT, MPI_DOUBLE};
	MPI_Aint alertDisp[3];

	alertDisp[0] = offsetof(Alert, timestamp);
	alertDisp[1] = offsetof(Alert, matchCount);
	alertDisp[2] = offsetof(Alert, commStartTime);
	
	MPI_Type_create_struct(3, alertBlockLen, alertDisp, alertTypes, AlertType);
	MPI_Type_commit(AlertType);
}


void initNodeInfoType(MPI_Datatype* NodeInfoType) {
	/**
	 * Initializes MPI datatype for NodeInfo struct
	 */
	
	int nodeInfoBlockLen[3] = {1, 2, 1};
	MPI_Datatype nodeInfoTypes[5] = {MPI_INT, MPI_INT, MPI_INT};
	MPI_Aint nodeInfoDisp[3];

	nodeInfoDisp[0] = offsetof(NodeInfo, rank);
	nodeInfoDisp[1] = offsetof(NodeInfo, coord);
	nodeInfoDisp[2] = offsetof(NodeInfo, temperature);

	MPI_Type_create_struct(3, nodeInfoBlockLen, nodeInfoDisp, nodeInfoTypes, NodeInfoType);
	MPI_Type_commit(NodeInfoType);
}


int getRandomNumber(int rank, int count) {
	/**
	 * Returns a random number 
	 */
	
	unsigned int seed = (unsigned) (time(NULL) ^ (rank + count)); 
	return (rand_r(&seed) % (MAX_TEMP - MIN_TEMP + 1)) + MIN_TEMP;
}




