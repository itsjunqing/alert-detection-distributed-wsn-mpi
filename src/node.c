#include <stdio.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "./init.h"
#include "./node.h"
#include "mac_ip.c"


void node(MPI_Comm commWorld, MPI_Comm comm) {
	/**
	 * Runs the event detection for each node 
	 * 
	 * commWorld: communication for entire program, to enable communication with base station
	 * comm: communication for the sensor nodes
	 */

	/*******************************************************
	 * Setting up cartesian grid topology
	 ********************************************************/

	// Initializes cartesian grid topology variables
	int rank, size, i;
	int coord[N_DIMS];
	MPI_Comm cartComm;
	int* neighbours;
	int neighboursCount;

	// Assign cartesian grid topology 
	MPI_Comm_rank(comm, &rank);
	MPI_Comm_size(comm, &size);
	initCartesianTopology(comm, rows, cols, &cartComm);
	
	// Get the coordinates and the list of neighboring ranks
	MPI_Cart_coords(cartComm, rank, N_DIMS, coord);
	getValidNeighbours(cartComm, &neighbours, &neighboursCount);

	// Opening a log file
	char filename[50];
	sprintf(filename, "log_%d.txt", rank);
	FILE *fptr = fopen(filename, "w");


	/*******************************************************
	 * Set up communication with base station
	 *******************************************************/

	// Send the MAC and IP addresses to base station with the original communicator
	int baseRank = 0;
	sendMACAndIPAddress(fptr, commWorld, baseRank);
	

	/*******************************************************
	 * Set up NodeInfo struct for communication
	 *******************************************************/

	// Initialize the basic information of a node
	NodeInfo nodeInfo;
	nodeInfo.rank = rank;
	memcpy(nodeInfo.coord, coord, sizeof(coord));
	nodeInfo.temperature = 0;

	// Send the content of NodeInfo to all neighbours for future usage
	NodeInfo* neighboursNodeInfo = (NodeInfo*) malloc(neighboursCount * sizeof(NodeInfo));
	for (i = 0; i < neighboursCount; i++) {
		MPI_Sendrecv(&nodeInfo, 1, NodeInfoType, neighbours[i], INFO_EXCHANGE_TAG, &neighboursNodeInfo[i], 1, NodeInfoType, neighbours[i], INFO_EXCHANGE_TAG, cartComm, MPI_STATUS_IGNORE);
	}	

	// Logging neighbours of a node
	for (i = 0; i < neighboursCount; i++) 
		fprintf(fptr, "Neighbour Rank: %d, Coord: (%d, %d)\n", neighboursNodeInfo[i].rank, neighboursNodeInfo[i].coord[0], neighboursNodeInfo[i].coord[1]);


	/*******************************************************
	 * Simulate node sensor readings 
	 *******************************************************/

	// Initialize the variables for simulation
	int terminated, temperature, waiting, allReceived, count; 
	terminated = 0;
	count = 0;

	// asynchronous sent for sending to neighbours
	MPI_Request sendRequests[neighboursCount]; 

	// asynchronous request for receiving from neighbours
	MPI_Request recvRequests[neighboursCount]; 

	// asynchronous sent for sending temperature to ranks that request for it
	MPI_Request tempSentReq; 

	// Output running message
	printf("Node %d started executing\n", rank);
	
	// Sleep for 3 seconds to wait for infrared simulation to be populated
	sleep(NODE_DELAYS);

	// Keep running until it receives a termination signal
	while (!terminated) {
		waiting = 0;
		temperature = getRandomNumber(rank, count);
		nodeInfo.temperature = temperature;

		// Log the temperature
		fprintf(fptr, "Temperature: %d\n", temperature);
			
		// Send a temperature request from all neighbours
		if (temperature > THRESHOLD) {
			sendTemperatureRequests(cartComm, neighbours, neighboursCount, neighboursNodeInfo, sendRequests, recvRequests, &waiting, fptr, rank);
			allReceived = 0;
		}
		
		// Check if any process is requesting for my temperature and send them accordingly 
		checkTemperatureRequest(cartComm, &tempSentReq, temperature, fptr, rank);

		// Check if base station has sent a termination signal and terminate accordingly
		checkTermination(commWorld, &terminated, baseRank, fptr, rank);
		if (terminated) {
			clearPendingCommunications(neighboursCount, sendRequests, recvRequests, &tempSentReq);
			continue;
		}

		// Keep waiting for the neighbours' temperature to be received 
		while (waiting) {

			// Check if any process is requesting for my temperature and send them accordingly 
			checkTemperatureRequest(cartComm, &tempSentReq, temperature, fptr, rank);

			// Check if base station has sent a termination signal and terminate accordingly
			checkTermination(commWorld, &terminated, baseRank, fptr, rank);
			if (terminated) {
				clearPendingCommunications(neighboursCount, sendRequests, recvRequests, &tempSentReq);
				waiting = 0; 
				continue;
			}

			// Tests if all neighbouring ranks have sent their temperature to this node
			MPI_Testall(neighboursCount, recvRequests, &allReceived, MPI_STATUSES_IGNORE);
			if (allReceived) {

				// Log the receive of temperature
				for (i = 0; i < neighboursCount; i++) 
					fprintf(fptr, "Rank %d has received the temperature %d from rank %d\n", rank, neighboursNodeInfo[i].temperature, neighboursNodeInfo[i].rank);
				
				// Check matching count
				int matchCount = getMatchingCount(neighboursNodeInfo, temperature, neighboursCount);

				// Send the report to base station
				if (matchCount >= 2) {
					sendReport(commWorld, baseRank, matchCount, &nodeInfo, neighboursNodeInfo, neighboursCount);
				}
				waiting = 0;
			}
		}

		// Sleep to create delays in microseconds
		usleep(nodeInterval * 1e6);

		// Increase the iteration count (for randomizing number generation)
		count++; 
	}

	// Output terminated message
	printf("Node %d terminated\n", rank);

	// Free cartesian grid communicator
	MPI_Comm_free(&cartComm);

	// Free dynamic arrays
	free(neighboursNodeInfo);
	free(neighbours);

}


void sendMACAndIPAddress(FILE* fptr, MPI_Comm commWorld, int baseRank) {	
	/**	
	 * Sends the MAC address and IP address of the rank to the base station for record purpose	
	 */	
		/// CHANGE BUFFER SIZE!
	// Get the MAC and IP addresses	
	char MACAddress[50];	
	char IPAddress[16];	
	getMACAddress(MACAddress);	
	getIPAddress(IPAddress);	
	
	// Pack and send the addresses to the base station	
	int position = 0;	
	int addressBufferSize = 100;	
	char addressBuffer[addressBufferSize];	
	MPI_Pack(MACAddress, 18, MPI_CHAR, addressBuffer, addressBufferSize, &position, commWorld);	
	MPI_Pack(IPAddress, 16, MPI_CHAR, addressBuffer, addressBufferSize, &position, commWorld);	
	MPI_Send(addressBuffer, addressBufferSize, MPI_PACKED, baseRank, ADDRESSES_TAG, commWorld);	

	// Log the MAC and IP addresses
	fprintf(fptr, "MAC Address: %s\n", MACAddress);
	fprintf(fptr, "IP Address: %s\n", IPAddress);

}


void initCartesianTopology(MPI_Comm comm, int rows, int cols, MPI_Comm* cartComm) {
	/**
	 * Takes in the number of rows and columns and initializes a 2D cartesian topology whose MPI communication of this topology is set to the given cartComm pointer. 
	 */
	
	// initializes variables
	int size, reorder;
	int wrapAround[N_DIMS], dims[N_DIMS];

	// assigning variables
	MPI_Comm_size(comm, &size);
	dims[0] = rows; 
	dims[1] = cols;
	wrapAround[0] = wrapAround[1] = 0; 
	reorder = 1;

	// create cartesian mapping
	MPI_Dims_create(size, N_DIMS, dims);
	MPI_Cart_create(comm, N_DIMS, dims, wrapAround, reorder, cartComm);

}


void getValidNeighbours(MPI_Comm cartComm, int** neighbours, int* neighboursCount) {
	/**
	 * Gets the valid neighbours (i.e., filtered neighours whose rank >= 0) rank and the total number of neighbours
	 */
	
	int i, leftRank, rightRank, topRank, bottomRank;

	// getting all neighbours' rank
	MPI_Cart_shift(cartComm, SHIFT_ROW, DISP, &topRank, &bottomRank);
	MPI_Cart_shift(cartComm, SHIFT_COL, DISP, &leftRank, &rightRank);

	// getting the total number of valid neighbours
	int allNeighbours[4] = {leftRank, rightRank, topRank, bottomRank};
	int count = 0;
	for (i = 0; i < 4; i++) {
		if (allNeighbours[i] >= 0) count++;
	}

	// filtering valid neighbours
	int* validNeighbours = (int*) malloc(count * sizeof(int));
	int pointer = 0;
	for (i = 0; i < 4; i++) {
		if (allNeighbours[i] >= 0) {
			validNeighbours[pointer] = allNeighbours[i];
			pointer++;
		}
	}	

	// copy the neighbours infomation to user-defined parameters
	*neighboursCount = count;
	*neighbours = validNeighbours;

}


int getMatchingCount(NodeInfo* neighboursNodeInfo, int temperature, int count) {	
	/**
	 * Returns the number of matching count of nodes' temperature within tolerance range
	 */
	int i, matchCount = 0;
	for (i = 0; i < count; i++) {
		if (abs(neighboursNodeInfo[i].temperature - temperature) <= TOLERANCE) 
			matchCount++;
	}
	return matchCount;
}


void sendTemperatureRequests(MPI_Comm cartComm, int* neighbours, int neighboursCount, NodeInfo* neighboursNodeInfo, MPI_Request* sendRequests, MPI_Request* recvRequests, int* waiting, FILE *fptr, int rank) {
	/**
	 * Request temperature from neighbours 
	 */
	
	int i, requested = 0;

	// Go through all neighbours
	for (i = 0; i < neighboursCount; i++) {

		// Send temperature request to neighbours
		MPI_Isend(&requested, 1, MPI_INT, neighbours[i], REQUEST_TAG, cartComm, &sendRequests[i]);
		MPI_Irecv(&neighboursNodeInfo[i].temperature, 1, MPI_INT, neighbours[i], TEMPERATURE_TAG, cartComm, &recvRequests[i]);

		// Log the request message
		fprintf(fptr, "Rank %d requesting temperature from neighbour rank %d\n", rank, neighbours[i]);
		fprintf(fptr, "Rank %d awaiting for temperature from neighbour rank %d\n", rank, neighbours[i]);
	}

	// Sets waiting to be true
	*waiting = 1;

}


void checkTemperatureRequest(MPI_Comm cartComm, MPI_Request *tempSentReq, int temperature, FILE *fptr, int rank) {
	/**
	 * Checks for any incoming requests for temperature and send it accordingly
	 */
	
	int granted, requestFlag = 0;
	MPI_Status status;

	// sending the temperature to the requested neighbour
	MPI_Iprobe(MPI_ANY_SOURCE, REQUEST_TAG, cartComm, &requestFlag, &status);
	if (requestFlag) {
		MPI_Recv(&granted, 1, MPI_INT, status.MPI_SOURCE, REQUEST_TAG, cartComm, &status);
		MPI_Isend(&temperature, 1, MPI_INT, status.MPI_SOURCE, TEMPERATURE_TAG, cartComm, tempSentReq);

		// Log the sending of temperature 
		fprintf(fptr, "Rank %d has received request from %d and sent the temperature %d to rank %d\n", rank, status.MPI_SOURCE, temperature, status.MPI_SOURCE);
	}
}


void checkTermination(MPI_Comm commWorld, int* terminated, int baseRank, FILE* fptr, int rank) {
	/**
	 * Checks for any termination signal sent from the base station 
	 */
	
	int terminationFlag = 0;
	MPI_Status status;

	// receiving the termination signal from base station 
	MPI_Iprobe(baseRank, TERMINATION_TAG, commWorld, &terminationFlag, &status);
	if (terminationFlag) { 
		MPI_Recv(terminated, 1, MPI_INT, status.MPI_SOURCE, TERMINATION_TAG, commWorld, &status);

		// Log the termination
		fprintf(fptr, "Rank %d has received the termination signal\n", rank);
	}

}


void clearPendingCommunications(int neighboursCount, MPI_Request* sendRequests, MPI_Request* recvRequests, MPI_Request* tempSentReq) {
	/**
	 * Cleans up the process upon returning by clearing all pending communications
	 */
	
	int i, sendCompleted, recvCompleted, tempSentCompleted;

	for (i = 0; i < neighboursCount; i++) {
		sendCompleted = recvCompleted = 0;
	
		// cancel the temperature request from neighbour if it has not completed
		MPI_Test(&sendRequests[i], &sendCompleted, MPI_STATUS_IGNORE);
		if (!sendCompleted) {
			MPI_Cancel(&sendRequests[i]);
			MPI_Request_free(&sendRequests[i]);
		}

		// cancel the temperature receive from neighbour if it has not completed
		MPI_Test(&recvRequests[i], &recvCompleted, MPI_STATUS_IGNORE);
		if (!recvCompleted) {
			MPI_Cancel(&recvRequests[i]);
			MPI_Request_free(&recvRequests[i]);
		}
	}

	// cancel the sending of temperature to neighbour if it has not completed
	MPI_Test(tempSentReq, &tempSentCompleted, MPI_STATUS_IGNORE);
	if (!tempSentCompleted) {
		MPI_Cancel(tempSentReq);
		MPI_Request_free(tempSentReq);
	}
}


void sendReport(MPI_Comm commWorld, int baseRank, int matchCount, NodeInfo* nodeInfo, NodeInfo* neighboursNodeInfo, int neighboursCount) {
	/**
	 * Sends the report to base station
	 */
	
	// Obtain the current reporting time
	time_t now;	
	time(&now);

	// Obtain the alert information
	Alert alert;
	alert.timestamp = now;
	alert.matchCount = matchCount;
	alert.commStartTime = MPI_Wtime();

	// Initialize buffer for sending report
	int reportBufferSize = 200; 
	char reportBuffer[reportBufferSize];
	int i, position = 0;

	// Packing alert, number of neighbours and all node's information
	MPI_Pack(&alert, 1, AlertType, reportBuffer, reportBufferSize, &position, commWorld);
	MPI_Pack(nodeInfo, 1, NodeInfoType, reportBuffer, reportBufferSize, &position, commWorld);
	MPI_Pack(&neighboursCount, 1, MPI_INT, reportBuffer, reportBufferSize, &position, commWorld);
	for (i = 0; i < neighboursCount; i++) 
		MPI_Pack(&neighboursNodeInfo[i], 1, NodeInfoType, reportBuffer, reportBufferSize, &position, commWorld);
	MPI_Send(reportBuffer, position, MPI_PACKED, baseRank, REPORT_TAG, commWorld);
	
}



