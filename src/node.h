#ifndef NODE_H
#define NODE_H

// Function definitions for node.c
void node(MPI_Comm commWorld, MPI_Comm comm);

void sendMACAndIPAddress(FILE* fptr, MPI_Comm commWorld, int baseRank);

void initCartesianTopology(MPI_Comm comm, int rows, int cols, MPI_Comm* cartComm);

void getValidNeighbours(MPI_Comm cartComm, int** neighbours, int* neighboursCount);

int getMatchingCount(NodeInfo* neighboursNodeInfo, int temperature, int count);

void sendTemperatureRequests(MPI_Comm cartComm, int* neighbours, int neighboursCount, NodeInfo* neighboursNodeInfo, MPI_Request* sendRequests, MPI_Request* recvRequests, int* waiting, FILE *fptr, int rank);

void checkTemperatureRequest(MPI_Comm cartComm, MPI_Request *tempSentReq, int temperature, FILE *fptr, int rank);

void checkTermination(MPI_Comm commWorld, int* terminated, int baseRank, FILE* fptr, int rank);

void clearPendingCommunications(int neighboursCount, MPI_Request* sendRequests, MPI_Request* recvRequests, MPI_Request* tempSentReq);

void sendReport(MPI_Comm commWorld, int baseRank, int matchCount, NodeInfo* nodeInfo, NodeInfo* neighboursNodeInfo, int neighboursCount);

#endif