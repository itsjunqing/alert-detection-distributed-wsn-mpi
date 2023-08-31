#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include <unistd.h>

#define LINES_OF_LOG 1000

void wsn(MPI_Comm master_comm, MPI_Comm new_comm);
void base_server(MPI_Comm master_comm, MPI_Comm new_comm);
void initializing_log(char log[LINES_OF_LOG][256]);
void writing_file(char log[LINES_OF_LOG][256], int rank);

int main(int argc, char **argv)
{
    int rank, size;
    MPI_Comm new_comm;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    if (size % 2 == 0)
    {
        if (rank == 0)
            printf("The number of processes must be an odd number (1 base station + divisible number of nodes).\n");
        return 0;
    }
    MPI_Comm_split(MPI_COMM_WORLD, rank == 0, 0, &new_comm);
    if (rank == 0)
        base_server(MPI_COMM_WORLD, new_comm);
    else
        wsn(MPI_COMM_WORLD, new_comm);
    MPI_Finalize();
    return 0;
}

void base_server(MPI_Comm master_comm, MPI_Comm new_comm)
{
    int dims[2], rank, size;
    MPI_Status status;
    MPI_Comm_rank(master_comm, &rank);
    MPI_Comm_size(master_comm, &size);
    MPI_Recv(dims, 2, MPI_INT, MPI_ANY_SOURCE, 0, master_comm, &status);
    printf("Root Rank: %d. Comm Size: %d: Grid Dimension = [%d x %d] \n", rank, size, dims[0], dims[1]);
    printf("=============== PRINTING 2D GRID INFO ===============\n");
    sleep(8);
    int termination = 1;
    for (int i = 1; i < size; i++) {
        MPI_Send(&termination, 1, MPI_INT, i, 0, master_comm);
//        printf("Sent to node %d\n", i);
    }
}

void wsn(MPI_Comm master_comm, MPI_Comm new_comm)
{
    int temperature, buff_temp, rank, size, notf, flag, i, j, k, iter, match_count, global_rank;
    int nbr_i_lo, nbr_i_hi;
    int nbr_j_lo, nbr_j_hi;
    int termination = 0;
    int threshold = 80;
    int tolerance = 5;
    int ndims = 2;
    int z = 0;
    int dims[ndims], coord[ndims];
    int wrap_around[ndims];

    int recvValues[4] = {-1, -1, -1, -1};
    char log[LINES_OF_LOG][256];

    time_t now;
    MPI_Comm comm2d;
    MPI_Request request;
    MPI_Comm_rank(master_comm, &global_rank);
    MPI_Comm_rank(new_comm, &rank);
    MPI_Comm_size(new_comm, &size);

    dims[0] = 4;
    dims[1] = 3;
//    printf("Size: %d\n", size);
//    MPI_Dims_create(size, ndims, dims);
    wrap_around[0] = 0;
    wrap_around[1] = 0;
    MPI_Cart_create(new_comm, ndims, dims, wrap_around, 1, &comm2d);
    MPI_Cart_coords(comm2d, rank, ndims, coord);
    MPI_Cart_shift(comm2d, 0, 1, &nbr_i_lo, &nbr_i_hi);
    MPI_Cart_shift(comm2d, 1, 1, &nbr_j_lo, &nbr_j_hi);

    printf("Global rank: %d. Coord: (%d, %d). Left: %d. Right: %d. Top: %d. Bottom: %d.\n", global_rank, coord[0],
           coord[1], nbr_j_lo, nbr_j_hi, nbr_i_lo, nbr_i_hi);
    int neighbors[4] = {nbr_j_lo, nbr_j_hi, nbr_i_lo, nbr_i_hi};

    int nneighbours = 0;
    for (int i = 0; i < 4; i ++) {
        if (neighbors[i] >= 0)
            nneighbours++;
    }
    printf("Number of neighbours of node %d: %d\n", rank, nneighbours);

    int recv_notf[nneighbours];

    MPI_Status *exchange_stat = (MPI_Status *) malloc(sizeof(MPI_Status) * nneighbours * 4);
    MPI_Request *exchange = (MPI_Request *) malloc(sizeof(MPI_Request) * nneighbours * 4);

    if (rank == 0)
    {
        MPI_Send(dims, 2, MPI_INT, 0, 0, master_comm);
    }

    initializing_log(log);

    iter = 0;
    while (termination == 0) {
        sprintf(log[z], "\n\n==============================\nRank %d. Iter %d\n", rank, iter);
        z++;

        srand(time(0) ^ rank);
        temperature = rand() % 20 + 70;
        sprintf(log[z], "Rank %d. Temp: %d\n", rank, temperature);
        z++;

        notf = -rank;
        if (rank == 0) {
            notf = -size;
        }
        if (temperature > threshold) {
            notf = rank;
        }

        recvValues[0] = 0;
        recvValues[1] = 0;
        recvValues[2] = 0;
        recvValues[3] = 0;

        j = k = 0;
        for (i = 0; i < 4; i++) {
            if (neighbors[i] >= 0) {
                MPI_Isend(&notf, 1, MPI_INT, neighbors[i], 1, comm2d, &exchange[j]);
                j++;
                sprintf(log[z], "Sending notification %d to rank %d\n", notf, neighbors[i]);
                z++;
                MPI_Irecv(&recv_notf[k], 1, MPI_INT, neighbors[i], 1, comm2d, &exchange[j]);
                k++;
                j++;
                sprintf(log[z], "Trying to receive from node %d\n", neighbors[i]);
                z++;
            }
        }

        sprintf(log[z], "Number of notification exchange: %d\n", j);
        z++;

        MPI_Waitall(j, exchange, exchange_stat);

        for (i = 0; i < nneighbours; i++) {
            sprintf(log[z], "Received notification %d\n", recv_notf[i]);
            z++;
        }

        j = 0;
        buff_temp = -1;
        for (i = 0; i < nneighbours; i++) {
            if (recv_notf[i] < 0) {
                buff_temp = -1;
                recv_notf[i] = abs(recv_notf[i]);
                if (recv_notf[i] == size) {
                    recv_notf[i] = 0;
                }
            } else {
                buff_temp = temperature;
            }
            MPI_Isend(&buff_temp, 1, MPI_INT, recv_notf[i], 0, comm2d, &exchange[j]);
            j++;
            sprintf(log[z], "Sending temperature %d to rank %d\n", buff_temp, recv_notf[i]);
            z++;
            MPI_Irecv(&recvValues[i], 1, MPI_INT, recv_notf[i], 0, comm2d, &exchange[j]);
            j++;
            sprintf(log[z], "Receiving temperature from rank %d\n", recv_notf[i]);
            z++;
        }

        sprintf(log[z], "Number of temperature exchange: %d\n", j);
        z++;
        MPI_Waitall(j, exchange, exchange_stat);

        sprintf(log[z], "Node %d has done exchanging temperatures.\n", rank);
        z++;

        match_count = 0;
        for (i = 0; i < nneighbours; i++) {
            sprintf(log[z], "Temperature received: %d\n", recvValues[i]);
            z++;
            if (abs(temperature - recvValues[i]) < tolerance) {
                match_count++;
            }
        }
        if (match_count >= 2) {
            sprintf(log[z], "High temperature detected: %d\nSending alert to base station.\n", temperature);
            z++;
        }

        MPI_Irecv(&termination, 1, MPI_INT, 0, 0, master_comm, &request);
        flag = 0;
        now = time(NULL);
        while(!flag && time(NULL) < now + 1){
            MPI_Test(&request, &flag, MPI_STATUS_IGNORE);
        }

        // Cancel the receive request if didn't receive any message
        if (!flag) {
            // we must have timed out
            MPI_Cancel(&request);
            MPI_Request_free(&request);
            // throw an error
        }
        printf("Rank %d. Recv flag: %d. Termination: %d\n", rank, flag, termination);

        sprintf(log[z], "Rank %d. Recv flag: %d. Termination: %d\n", rank, flag, termination);
        z++;

        iter++;
    }

    writing_file(log, rank);

    printf("Node %d end.\n", rank);

    MPI_Comm_free(&comm2d);
    free(exchange_stat);
    free(exchange);
}

void initializing_log(char log[LINES_OF_LOG][256]) {
    for (int i = 0; i < LINES_OF_LOG; i++) {
        sprintf(log[i], '\0');
    }
}

void writing_file(char log[LINES_OF_LOG][256], int rank)
{
    FILE *fptr;
    char filename[50];
    sprintf(filename, "log_%d.txt", rank);
    fptr = fopen(filename,"w");
    for (int j = 0; j < LINES_OF_LOG; j++) {
        fprintf(fptr, log[j]);
    }
    fclose(fptr);
}