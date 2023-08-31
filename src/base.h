#ifndef BASE_H
#define BASE_H

#include <pthread.h>

// Define SatelliteData structure, to store the information for simulating temperature values
typedef struct {
	long timestamp;
	int* values;
} SatelliteData; 

// Define SatelliteAlert structure, to store the satellite information matched
typedef struct {
	long satelliteTime;
	int satelliteTemperature;
} SatelliteAlert;

// Function definitions for base.c
void base(MPI_Comm commWorld, MPI_Comm comm); 
void receiveMACAndIPAddress(MPI_Comm commWorld, int cartSize);
void listenForReports(MPI_Comm commWorld);
int isWithinThreshold(NodeInfo* reportingNode, Alert* alert, SatelliteAlert* satelliteAlert);
void printSimulatedValues(FILE* fptr, int size);
void* threadSimulation(void* arg);
void* checkStop(void* arg);
void constructInfrared(int size);
void destructInfrared();

#endif
