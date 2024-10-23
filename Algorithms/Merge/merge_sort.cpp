#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
#include <math.h>
#include <string.h>
#include <float.h>

// Performance measurement variables
double start_time, end_time;
double local_elapsed_time, total_time;
double min_time, max_time, avg_time, variance_time;

// Function declarations
void merge_arrays(int *, int *, int, int, int);
void merge_sort(int *, int *, int, int);
void check(int *array, int sizeOfArray);
void generate_input(int *array, int size, const char *input_type);
void evaluate_performance(double local_time, int rank, int total_processes);

int main(int argc, char** argv) {
    CALI_CXX_MARK_FUNCTION;

    // Metadata variables
    const char *algorithm = "merge";
    const char *programming_model = "mpi";
    const char *data_type = "int";
    const char *input_type = "Random";  // Default is Random
    const char *implementation_source = "online";
    const char *scalability = "weak"; 
    const size_t size_of_data_type = sizeof(int);

    const char* main = "main";
    const char* data_init_runtime = "data_init_runtime";
    const char* comm = "comm";
    const char* comm_large = "comm_large";
    const char* comp = "comp";
    const char* comp_large = "comp_large";
    const char* correctness_check = "correctness_check";
   
    // Declare array
    int array_size = 0;
    if (argc == 3) {
        array_size = atoi(argv[1]);
        input_type = argv[2]; // Accept input type from command line
    } else {
        printf("\nProvide the size of the array and input type (Random/Sorted/Reverse/1%%Perturbed)\n");
        return 0;
    }
    
    int process_rank;
    int total_processes;

    MPI_Status status;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &process_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &total_processes);

    cali::ConfigManager mgr;
    mgr.start();
  
    // Initialize array
    CALI_MARK_BEGIN("data_init_runtime");
    int *global_array = (int*)malloc(array_size * sizeof(int));
    generate_input(global_array, array_size, input_type); // Generate array based on input_type
    CALI_MARK_END("data_init_runtime");
   
    // Start timing
    MPI_Barrier(MPI_COMM_WORLD); // Synchronize processes
    start_time = MPI_Wtime(); // Start timing the entire process
    
    // Divide array
    int local_chunk_size = array_size / total_processes;

    // Send subarrays using MPI scatter
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    int *local_array = (int*)malloc(local_chunk_size * sizeof(int));
    MPI_Scatter(global_array, local_chunk_size, MPI_INT, local_array, local_chunk_size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    
    // Perform local merge sort
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    int *temp_array = (int*)malloc(local_chunk_size * sizeof(int));
    merge_sort(local_array, temp_array, 0, local_chunk_size - 1);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
  
    // Combine subarrays
    int *final_sorted_array = NULL;
    if (process_rank == 0) {
        final_sorted_array = (int*)malloc(array_size * sizeof(int));
    }

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    if (process_rank == 0) {
        MPI_Gather(MPI_IN_PLACE, local_chunk_size, MPI_INT, final_sorted_array, local_chunk_size, MPI_INT, 0, MPI_COMM_WORLD);
    } else {
        MPI_Gather(local_array, local_chunk_size, MPI_INT, NULL, local_chunk_size, MPI_INT, 0, MPI_COMM_WORLD);
    }
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
 
    // Merge sort on the master
    if (process_rank == 0) {
        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");
        int *final_temp_array = (int*)malloc(array_size * sizeof(int));
        merge_sort(final_sorted_array, final_temp_array, 0, array_size - 1);
        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");

        check(final_sorted_array, array_size);
        
        free(final_sorted_array);
        free(final_temp_array);
    }

    // End timing
    end_time = MPI_Wtime();
    local_elapsed_time = end_time - start_time;

    evaluate_performance(local_elapsed_time, process_rank, total_processes);

    free(global_array);
    free(local_array);
    free(temp_array);
 
    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::value("algorithm", algorithm);
    adiak::value("programming_model", programming_model);
    adiak::value("data_type", data_type);
    adiak::value("size_of_data_type", size_of_data_type);
    adiak::value("input_size", array_size);
    adiak::value("input_type", input_type); 
    adiak::value("num_procs", total_processes); 
    adiak::value("scalability", scalability); 
    adiak::value("group_num", 22); 
    adiak::value("implementation_source", implementation_source);
 
    CALI_MARK_BEGIN("comm");
    MPI_Barrier(MPI_COMM_WORLD);
    CALI_MARK_END("comm");
    
    if (process_rank == 0){
      printf("\nProcesses: %d, Array Size: %d, Input Type: %s\n", total_processes, array_size, input_type);
    }
    
    mgr.stop();
    mgr.flush();
    MPI_Finalize();
    return 0;
}

// Helper function to generate input arrays based on type
void generate_input(int *array, int size, const char *input_type) {
    if (strcmp(input_type, "Sorted") == 0) {
        for (int i = 0; i < size; i++) array[i] = i;
    } else if (strcmp(input_type, "Reverse") == 0) {
        for (int i = 0; i < size; i++) array[i] = size - i;
    } else if (strcmp(input_type, "Perturbed") == 0) {
        for (int i = 0; i < size; i++) array[i] = i;
        for (int i = 0; i < size / 100; i++) {
            int idx1 = rand() % size;
            int idx2 = rand() % size;
            int temp = array[idx1];
            array[idx1] = array[idx2];
            array[idx2] = temp;
        }
    } else { // Default: Random
        srand(time(NULL));
        for (int i = 0; i < size; i++) array[i] = rand() % 1000;
    }
}

// Helper function to evaluate performance metrics
void evaluate_performance(double local_time, int rank, int total_processes) {
    MPI_Reduce(&local_time, &min_time, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_time, &max_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_time, &total_time, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    
    if (rank == 0) {
        avg_time = total_time / total_processes;
        variance_time = 0.0;

        for (int i = 0; i < total_processes; i++) {
            variance_time += pow(local_time - avg_time, 2);
        }
        variance_time /= total_processes;

        printf("Performance Metrics:\n");
        printf("Total Time: %f\n", total_time);
        printf("Min Time/Rank: %f\n", min_time);
        printf("Max Time/Rank: %f\n", max_time);
        printf("Avg Time/Rank: %f\n", avg_time);
        printf("Variance Time/Rank: %f\n", variance_time);
    }
}

void merge_arrays(int *original, int *temp, int left, int middle, int right) {
    int left_index = left;        
    int right_index = middle + 1; 
    int merge_index = left;       

    while (left_index <= middle && right_index <= right) {
        if (original[left_index] <= original[right_index]) {
            temp[merge_index] = original[left_index];
            left_index++;
        } else {
            temp[merge_index] = original[right_index];
            right_index++;
        }
        merge_index++;
    }

    while (left_index <= middle) {
        temp[merge_index] = original[left_index];
        left_index++;
        merge_index++;
    }

    while (right_index <= right) {
        temp[merge_index] = original[right_index];
        right_index++;
        merge_index++;
    }

    for (merge_index = left; merge_index <= right; merge_index++) {
        original[merge_index] = temp[merge_index];
    }
}

void merge_sort(int *array, int *temp_array, int left, int right) {
    if (left < right) {
        int middle = left + (right - left) / 2;
        merge_sort(array, temp_array, left, middle);
        merge_sort(array, temp_array, middle + 1, right);
        merge_arrays(array, temp_array, left, middle, right);
    }
}

void check(int *array, int sizeOfArray) {
    CALI_MARK_BEGIN("correctness_check");
    for (int i = 0; i < sizeOfArray - 1; i++) {
        if (array[i] > array[i + 1]) {
            printf("\nIncorrect\n");
            return;
        }
    }
    CALI_MARK_END("correctness_check");
    printf("\nArray is sorted\n");
}
