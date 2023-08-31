#include <stdio.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "./init.h"
#include "./base.h"


// Define global variables
pthread_mutex_t infraredTimeMutex;
pthread_mutex_t infraredValueMutex;
SatelliteData* simulatedValues = NULL;
char** macAddresses = NULL;
char** ipAddresses = NULL;
int userStop;
double simStartTime;


void base(MPI_Comm commWorld, MPI_Comm comm) {
	/**
	 * Base station function
	 */

	// Get the size of the entire communication and the the grid size
	int size;
	MPI_Comm_size(commWorld, &size);
	int cartSize = size-1;	

	// Starts the simulation time
	simStartTime = MPI_Wtime();

	// Constructs infrared simulation
	constructInfrared(cartSize);
	
	// Creates a thread to simulate the temperatures
	pthread_t tid_satellite;
	pthread_create(&tid_satellite, 0, threadSimulation, &cartSize);
		
	// Creates a thread to check for user stopping
	pthread_t tid_userStop;
	userStop = 0;
	pthread_create(&tid_userStop, 0, checkStop, NULL);

	// Receives the MAC and IP address from all nodes
	receiveMACAndIPAddress(commWorld, cartSize);
	
	// Start listening to events from nodes
	listenForReports(commWorld);

	// Sends termination signal to all nodes in the grid
	int i, terminated = 1;
	for (i = 1; i < size; i++) {
		MPI_Send(&terminated, 1, MPI_INT, i, TERMINATION_TAG, commWorld); 
	}

	// Stops the thread from running
	pthread_cancel(tid_satellite);
	pthread_cancel(tid_userStop);

	// Destructs infrared simulation
	destructInfrared();

	printf("Base terminated!\n");
}


void receiveMACAndIPAddress(MPI_Comm commWorld, int cartSize) {
	/**
	 * Receives the MAC and IP Addresses from nodes and store them in arrays
	 */
	
	int position, i; 
	char addressBuffer[ADDRESS_BUFFER_SIZE];
	MPI_Status status;

	// Allocates size for MAC address
	macAddresses = (char**) malloc(cartSize * sizeof(char*));
	for (i = 0; i < cartSize; i++) 
		macAddresses[i] = (char*) malloc(18 * sizeof(char));
	
	// Allocates size for IP address
	ipAddresses = (char**) malloc(cartSize * sizeof(char*));
	for (i = 0; i < cartSize; i++) 
		ipAddresses[i] = (char*) malloc(16 * sizeof(char));

	// Receives the address buffer from all rank 1 to last rank 
	for (i = 1; i <= cartSize; i++) {
		position = 0;
		MPI_Recv(addressBuffer, ADDRESS_BUFFER_SIZE, MPI_PACKED, i, ADDRESSES_TAG, commWorld, &status);
		MPI_Unpack(addressBuffer, ADDRESS_BUFFER_SIZE, &position, macAddresses[i-1], 18, MPI_CHAR, commWorld);
		MPI_Unpack(addressBuffer, ADDRESS_BUFFER_SIZE, &position, ipAddresses[i-1], 16, MPI_CHAR, commWorld);
	}
}



void listenForReports(MPI_Comm commWorld) {
	/**
	 * Listens for incoming reports from nodes
	 */
	
	int flag, position, i, count = 0;

	// Initialize the buffer to receive from node
	char reportBuffer[REPORT_BUFFER_SIZE];

	// Initialize the structures to unpack from the buffer
	Alert alert;
	NodeInfo* neighboursNodeInfo;
	NodeInfo reportingNode;
	int neighboursCount;

	// Initialize local variables
	time_t now;
	MPI_Status status;
		
	double commTime;
	double longestCommTime = 0;
	double shortestCommTime = 0;
	double totalCommTime = 0;
	int trueAlertsCount = 0;
	int falseAlertsCount = 0;

	FILE* baseFilePtr = fopen("base_log.txt", "w");;

	// Start running
	while (count < baseIterationsCount) { 
		position = 0;
			
		// Stops listening if user enters stop
		if (userStop) break;

		MPI_Recv(reportBuffer, REPORT_BUFFER_SIZE, MPI_PACKED, MPI_ANY_SOURCE, REPORT_TAG, commWorld, &status);
		printf("Base received report from rank %d\n", status.MPI_SOURCE-1);
	
		MPI_Unpack(reportBuffer, REPORT_BUFFER_SIZE, &position, &alert, 1, AlertType, commWorld);

		MPI_Unpack(reportBuffer, REPORT_BUFFER_SIZE, &position, &reportingNode, 1, NodeInfoType, commWorld);

		MPI_Unpack(reportBuffer, REPORT_BUFFER_SIZE, &position, &neighboursCount, 1, MPI_INT, commWorld);
		neighboursNodeInfo = (NodeInfo*) malloc(neighboursCount * sizeof(NodeInfo));

		// Unpack each neighbour
		for (i = 0; i < neighboursCount; i++) 
			MPI_Unpack(reportBuffer, REPORT_BUFFER_SIZE, &position, &neighboursNodeInfo[i], 1, NodeInfoType, commWorld);
		
		time(&now);

		commTime = MPI_Wtime() - alert.commStartTime - NODE_DELAYS;
		commTime = commTime < 0? 0: commTime;
		totalCommTime += commTime;
	 	longestCommTime = (longestCommTime > commTime)? longestCommTime: commTime;
	 	shortestCommTime = (shortestCommTime < commTime)? shortestCommTime: commTime;
		
		// Initialize a SatelliteAlert to obtain the satellite information matched
		SatelliteAlert satelliteAlert;
		satelliteAlert.satelliteTime = 0;
		satelliteAlert.satelliteTemperature = 0;

		int trueAlert = isWithinThreshold(&reportingNode, &alert, &satelliteAlert);
		trueAlert? trueAlertsCount++: falseAlertsCount++;
		
		fprintf(baseFilePtr, "============================================================\n");
		fprintf(baseFilePtr, "Iteration: %d\n", count);
		fprintf(baseFilePtr, "Logged Time: %s", asctime(localtime(&now)));

		fprintf(baseFilePtr, "\n");
		fprintf(baseFilePtr, "Alert Reported Time: %s", asctime(localtime(&alert.timestamp)));
		fprintf(baseFilePtr, "Alert Type: %s\n", trueAlert? "True": "False");
		fprintf(baseFilePtr, "Number of Adjacent Matches to Reporting Node: %d\n", alert.matchCount);
		fprintf(baseFilePtr, "Communication Time (seconds): %f\n", commTime);

		fprintf(baseFilePtr, "\n");
		fprintf(baseFilePtr, "Reporting Node Information:\n");
		fprintf(baseFilePtr, "\t\tRank: %d\n", reportingNode.rank);
		fprintf(baseFilePtr, "\t\tCoordinate: (%d, %d)\n", reportingNode.coord[0], reportingNode.coord[1]);
		fprintf(baseFilePtr, "\t\tTemperature: %d\n", reportingNode.temperature);
		fprintf(baseFilePtr, "\t\tMAC Address: %s\n", macAddresses[reportingNode.rank]);
		fprintf(baseFilePtr, "\t\tIP Address: %s\n", ipAddresses[reportingNode.rank]);

		fprintf(baseFilePtr, "\n");
		fprintf(baseFilePtr, "Adjacent Nodes Information:\n");
		for (i = 0; i < neighboursCount; i++) {
			fprintf(baseFilePtr, "\t\tRank: %d\n", neighboursNodeInfo[i].rank);
			fprintf(baseFilePtr, "\t\tCoordinate: (%d, %d)\n", neighboursNodeInfo[i].coord[0], neighboursNodeInfo[i].coord[1]);
			fprintf(baseFilePtr, "\t\tTemperature: %d\n", neighboursNodeInfo[i].temperature);
			fprintf(baseFilePtr, "\t\tMAC Address: %s\n", macAddresses[neighboursNodeInfo[i].rank]);
			fprintf(baseFilePtr, "\t\tIP Address: %s\n", ipAddresses[neighboursNodeInfo[i].rank]);
			fprintf(baseFilePtr, "\t\t-------------------------\n");
		}

		fprintf(baseFilePtr, "\n");
		fprintf(baseFilePtr, "Infrared Satellite Information:\n");
		fprintf(baseFilePtr, "\t\tReporting Time: %s", asctime(localtime(&satelliteAlert.satelliteTime)));
		fprintf(baseFilePtr, "\t\tReporting Temperature: %d\n", satelliteAlert.satelliteTemperature);
		
		// Sleep in microseconds
		usleep(baseInterval * 1e6);
		count++;
	}

	fprintf(baseFilePtr, "==================================================\n");
	fprintf(baseFilePtr, "\t\tSummary Report\n");
	fprintf(baseFilePtr, "==================================================\n");
	
	fprintf(baseFilePtr, "Total Simulation Time (seconds): %f\n", MPI_Wtime() - simStartTime);
	fprintf(baseFilePtr, "Shortest Communication Time (seconds): %f\n", shortestCommTime);
	fprintf(baseFilePtr, "Longest Communication Time (seconds): %f\n", longestCommTime);

	fprintf(baseFilePtr, "\n");
	fprintf(baseFilePtr, "Total Communication Time (seconds): %f\n", totalCommTime);
	fprintf(baseFilePtr, "Total Messages Received: %d\n", count);
	fprintf(baseFilePtr, "Average Communication Time (seconds): %f\n", totalCommTime / count);

	fprintf(baseFilePtr, "\n");
	fprintf(baseFilePtr, "Total True Alerts Count: %d\n", trueAlertsCount);
	fprintf(baseFilePtr, "Total False Alerts Count: %d\n", falseAlertsCount);

	fclose(baseFilePtr);

	// free dynamic array
	free(neighboursNodeInfo);

}


int isWithinThreshold(NodeInfo* reportingNode, Alert* alert, SatelliteAlert* satelliteAlert) {
	/**
	 * Returns true if the reporting node's temperature matches with the simulated temperature by a threshold value and false otherwise
	 */
	
	int rank = reportingNode->rank;
	int i, j, infraredTemperature;
	time_t now;

	// Go through all time units
	for (i = 0; i < TIME_UNITS; i++) {
		pthread_mutex_lock(&infraredTimeMutex); // lock with mutex
		now = simulatedValues[i].timestamp; // read from infrared simulation
		pthread_mutex_unlock(&infraredTimeMutex);

		// Continue if timestamp is not yet set (thread still running, data not generated)
		if (now == 0) continue;
		satelliteAlert->satelliteTime = now;
		
		// Checks if alert's time and simulated time is within a fixed time window
		if (labs(now - alert->timestamp) <= TIME_WINDOW) {
			pthread_mutex_lock(&infraredValueMutex); // lock with mutex
			infraredTemperature = simulatedValues[i].values[rank]; // read the temperature
			pthread_mutex_unlock(&infraredValueMutex);

			satelliteAlert->satelliteTemperature = infraredTemperature;
			
			// Checks if the node's temperature matches the simulated temperature by a threshold
			if (abs(infraredTemperature - reportingNode->temperature) <= TOLERANCE) {
				return 1;
			}
		}
	}
	return 0;
}



void printSimulatedValues(FILE* fptr, int size) {
	/**
	 * Logs the simulated values 
	 */
	
	int value, i, j;
	time_t now;

	// Go through all time units
	for (i = 0; i < TIME_UNITS; i++) {
		pthread_mutex_lock(&infraredTimeMutex); // lock with mutex
		now = simulatedValues[i].timestamp; // read
		pthread_mutex_unlock(&infraredTimeMutex);

		// Log the timestamp for generating the temperatures 
		fprintf(fptr,"==============\n");
		if (now != 0)
			fprintf(fptr, "TIME[%d] = %ld = %s\n", i, now, asctime(localtime(&now)));
		else 
			fprintf(fptr, "TIME[%d] = 0\n", i);

		// Log the temperatures generated of each node for this time unit
		for (j = 0; j < size; j++) {
			pthread_mutex_lock(&infraredValueMutex); // lock with mutex
			value = simulatedValues[i].values[j];
			pthread_mutex_unlock(&infraredValueMutex);
			fprintf(fptr, "values[%d] = %d\n", j, value);
		}
	}
}


void* threadSimulation(void* arg) {
	/**
	 * Simulates the temperature values until it is stopped by the base process
	 */
	
	int size = *((int*) arg);
	int value, i, j, count = 0;
	time_t rawTime; 

	FILE *fptr = fopen("thread_log.txt", "w");

	// Keep running infinitely
	while (1) {
		
		// Go through all time units 
		for (i = 0; i < TIME_UNITS; i++) {
			time(&rawTime); 
			pthread_mutex_lock(&infraredTimeMutex); // lock with mutex
			simulatedValues[i].timestamp = rawTime;
			pthread_mutex_unlock(&infraredTimeMutex);

			// Simulates a temperature for this time unit
			for (j = 0; j < size; j++) {
				value = getRandomNumber(j, count);
				pthread_mutex_lock(&infraredValueMutex); // lock with mutex
				simulatedValues[i].values[j] = value;
				pthread_mutex_unlock(&infraredValueMutex);
			}

			// Sleep for 500 milliseconds 
			usleep(500000); 
			
			// Log the simulated values
			printSimulatedValues(fptr, size);
		}

		// Increase the iteration count (for seeding random value)
		count++;
	}
	return NULL;
}

void* checkStop(void* arg) {
	/**
	 * Waits for the user to stop the program manually 
	 */

	char temp[4];

	// Display the message
	while (strcmp(temp, "stop") != 0) {
		printf("MANUAL: Enter \"stop\" at anytime to stop the program\n");
		fflush(stdout);
		fflush(stdin);
		scanf("%s", temp);
	}

	printf("DRIVER: Stopping the program..\n");
	userStop = 1;

	return NULL;
}


void constructInfrared(int size) {
	/**
	 * Initializes and construct the infrared simulation 
	 */
	
	int i;

	// Initializes mutex to access time and temperature
	pthread_mutex_init(&infraredTimeMutex, NULL);
	pthread_mutex_init(&infraredValueMutex, NULL);
	
	// Initializes global array to store simulated values
	simulatedValues = (SatelliteData*) malloc(TIME_UNITS * sizeof(SatelliteData)); 
		
	// Preset the simulation values 
	for (i = 0; i < TIME_UNITS; i++) {
		simulatedValues[i].timestamp = 0;
		simulatedValues[i].values = (int*) calloc(size, sizeof(int));
	}
}


void destructInfrared() {
	/**
	 * Destructs all dynamically allocated memory for infrared simulation
	 */
	
	int i;
	
	for (i = 0; i < TIME_UNITS; i++) {
		free(simulatedValues[i].values);
	}
	free(simulatedValues);
}



