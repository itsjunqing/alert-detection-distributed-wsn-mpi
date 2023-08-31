#ifndef INIT_H
#define INIT_H

#include <mpi.h>

// Create NodeInfo structure to store the information of a node
typedef struct {
	int rank;
	int coord[2];
	int temperature;
} NodeInfo;


// Create Alert structure to store the information of sending Alerts to base station
typedef struct {
	long timestamp;
	int matchCount;
	double commStartTime;
} Alert;


// Define MPI shfitings
#define SHIFT_ROW 0
#define SHIFT_COL 1
#define DISP 1
#define N_DIMS 2


// Define program constants
#define TIME_UNITS 10 // reduce this to increase more false alert, and vice-versa
#define TIME_WINDOW 8 // reduce this to increase more false alert, and vice-versa
#define MAX_TEMP 120
#define MIN_TEMP 50 
#define THRESHOLD 80 // "high temperature" threshold
#define TOLERANCE 5 // tolerance range of 5 to be "high temperature"
#define ADDRESS_BUFFER_SIZE 500
#define REPORT_BUFFER_SIZE 1000
#define BUFFER_SIZE 1000
#define NODE_DELAYS 3 // delays the node execution for 3 seconds to allow temperatures to be simulated first


// Define MPI communication tags
#define INIT_TAG 0
#define ADDRESSES_TAG 0
#define INFO_EXCHANGE_TAG 1
#define REQUEST_TAG 2
#define TEMPERATURE_TAG 3
#define REPORT_TAG 4
#define TERMINATION_TAG 5
#define CANCEL_TAG 6


// Global variables
MPI_Datatype AlertType;
MPI_Datatype NodeInfoType;
int rows;
int cols;
float nodeInterval;
float baseInterval;
int baseIterationsCount;


// Function definitions for init.c
void printGuide();
void getUserInputs(MPI_Comm commWorld, int rank, int size);
void initAlertType(MPI_Datatype* AlertType);
void initNodeInfoType(MPI_Datatype* NodeInfoType);
int getRandomNumber(int rank, int count);


#endif