#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
#include <math.h>

#define MASTER 0
#define FROM_MASTER 1
#define FROM_WORKER 2

//functions
void merge(int *array, int l, int m, int r);
void mergeSort(int *array, int l, int r);
void check(int *array, int sizeOfArray);

//metadata
const char *algorithm = "merge";
const char *programming_model = "mpi";
const char *data_type = "int";
const char *input_type = "Random";
const char *implementation_source = "online";
const char *scalability = "weak"; 
const size_t size_of_data_type = sizeof(int);

int main (int argc, char *argv[]) {
    CALI_CXX_MARK_FUNCTION;

    int sizeOfArray;
    if (argc == 2) {
        sizeOfArray = atoi(argv[1]);
    } 
    else {
        printf("\nPlease provide the size of the array\n");
        return 0;
    }
  

    int numtasks, taskid, numworkers, source, dest, mtype, averow, extra, offset, i;
    int *array = NULL, *local_array = NULL;
    const char* main = "main";
    const char* data_init_runtime = "data_init_runtime";
    const char* comm = "comm";
    const char* comm_small = "comm_small";
    const char* comm_large = "comm_large";
    const char* comp = "comp";
    const char* comp_small = "comp_small";
    const char* comp_large = "comp_large";
    const char* correctness_check = "correctness_check";

    MPI_Status status;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);

    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::value("algorithm", algorithm);
    adiak::value("programming_model", programming_model);
    adiak::value("data_type", data_type);
    adiak::value("size_of_data_type", size_of_data_type);
    adiak::value("input_size", sizeOfArray);
    adiak::value("input_type", input_type); 
    adiak::value("num_procs", numtasks); 
    adiak::value("scalability", scalability); 
    adiak::value("group_num", 22); 
    adiak::value("implementation_source", implementation_source);


    if (numtasks < 2) {
        printf("Need at least two MPI tasks. Quitting...\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        exit(1);
    }
    numworkers = numtasks - 1;

    double start_time, end_time, local_time, total_local_time, min_time, max_time, avg_time, sum_squared_diffs, variance_time;

    start_time = MPI_Wtime(); 
    CALI_MARK_BEGIN("main");

    cali::ConfigManager mgr;
    mgr.start();

    //master
    if (taskid == MASTER) {
        CALI_MARK_BEGIN("data_init_runtime");

        array = (int*)malloc(sizeOfArray * sizeof(int));
        for (i = 0; i < sizeOfArray; i++) {
            array[i] = rand() % 1000; 
        }
        CALI_MARK_END("data_init_runtime");

        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_large");
        averow = sizeOfArray / numworkers;
        extra = sizeOfArray % numworkers;
        offset = 0;
        mtype = FROM_MASTER;
        for (dest = 1; dest <= numworkers; dest++) {
            int chunk_size = (dest <= extra) ? averow + 1 : averow;
            MPI_Send(&offset, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);
            MPI_Send(&chunk_size, 1, MPI_INT, dest, mtype, MPI_COMM_WORLD);
            MPI_Send(&array[offset], chunk_size, MPI_INT, dest, mtype, MPI_COMM_WORLD);
            offset += chunk_size;
        }

        mtype = FROM_WORKER;
        for (i = 1; i <= numworkers; i++) {
            source = i;
            int chunk_size;
            MPI_Recv(&offset, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);
            MPI_Recv(&chunk_size, 1, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);
            MPI_Recv(&array[offset], chunk_size, MPI_INT, source, mtype, MPI_COMM_WORLD, &status);
        }
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");

        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");
        mergeSort(array, 0, sizeOfArray - 1);
        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");

        check(array, sizeOfArray);
        free(array);
    }

    //worker
    if (taskid > MASTER) {
        CALI_MARK_BEGIN("comm");
        CALI_MARK_BEGIN("comm_large");
        mtype = FROM_MASTER;
        int chunk_size;
        MPI_Recv(&offset, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);
        MPI_Recv(&chunk_size, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);

        local_array = (int *)malloc(chunk_size * sizeof(int));
        MPI_Recv(local_array, chunk_size, MPI_INT, MASTER, mtype, MPI_COMM_WORLD, &status);
        CALI_MARK_END("comm_large");
        CALI_MARK_END("comm");


        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");
        mergeSort(local_array, 0, chunk_size - 1);
        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");

        mtype = FROM_WORKER;
        MPI_Send(&offset, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);
        MPI_Send(&chunk_size, 1, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);
        MPI_Send(local_array, chunk_size, MPI_INT, MASTER, mtype, MPI_COMM_WORLD);

        free(local_array);
    }
    end_time = MPI_Wtime();
    local_time = end_time - start_time;

    CALI_MARK_END("main");

    MPI_Reduce(&local_time, &min_time, 1, MPI_DOUBLE, MPI_MIN, MASTER, MPI_COMM_WORLD);
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, MASTER, MPI_COMM_WORLD);
    MPI_Reduce(&local_time, &total_local_time, 1, MPI_DOUBLE, MPI_SUM, MASTER, MPI_COMM_WORLD);
    avg_time = total_local_time / numtasks;

    double local_squared_diff = (local_time - avg_time) * (local_time - avg_time);
    MPI_Reduce(&local_squared_diff, &sum_squared_diffs, 1, MPI_DOUBLE, MPI_SUM, MASTER, MPI_COMM_WORLD);
    variance_time = sum_squared_diffs / numtasks;

    if (taskid == MASTER) {
        printf("Total Time: %f\n", total_local_time);
        printf("Min Time/Rank: %f\n", min_time);
        printf("Max Time/Rank: %f\n", max_time);
        printf("Avg Time/Rank: %f\n", avg_time);
        printf("Variance Time/Rank: %f\n", variance_time);
    }

    mgr.stop();
    mgr.flush();
    MPI_Finalize();
    return 0;
}

void merge(int *array, int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;
    int *L = (int *)malloc(n1 * sizeof(int));
    int *R = (int *)malloc(n2 * sizeof(int));

    for (int i = 0; i < n1; i++) L[i] = array[l + i];
    for (int j = 0; j < n2; j++) R[j] = array[m + 1 + j];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (L[i] <= R[j]) {
            array[k] = L[i++];
        } else {
            array[k] = R[j++];
        }
        k++;
    }

    while (i < n1){
      array[k++] = L[i++];
    }
    while (j < n2) {
      array[k++] = R[j++];
    }

    free(L);
    free(R);
}

void mergeSort(int *array, int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;
        mergeSort(array, l, m);
        mergeSort(array, m + 1, r);
        merge(array, l, m, r);
    }
}

void check(int *array, int sizeOfArray) {
    CALI_MARK_BEGIN("correctness_check");
    for (int i = 1; i < sizeOfArray; i++) {
        if (array[i - 1] > array[i]) {
            printf("Array is not sorted at index %d\n", i);
            return;
        }
    }
    CALI_MARK_END("correctness_check");
    printf("Array is correctly sorted.\n");
}
