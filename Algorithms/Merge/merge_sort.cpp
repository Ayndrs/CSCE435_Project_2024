#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
#include <math.h>

void merge_arrays(int *, int *, int, int, int);
void merge_sort(int *, int *, int, int);
void check(int *array, int sizeOfArray);

int main(int argc, char** argv) {
    CALI_CXX_MARK_FUNCTION;

    //metadata variables
    const char *algorithm = "merge";
    const char *programming_model = "mpi";
    const char *data_type = "int";
    const char *input_type = "Random";
    const char *implementation_source = "online";
    const char *scalability = "weak"; 
    const size_t size_of_data_type = sizeof(int);

    const char* main = "main";
    const char* data_init_runtime = "data_init_runtime";
    const char* comm = "comm";
    const char* comm_large = "comm_large";
    const char* comp = "comp";
    const char* comp_large = "comp_large";
    const char* comp_small = "comp_small";
    const char* correctness_check = "correctness_check";
   
    //declare array
    int array_size = 0;
    if (argc == 2) {
        array_size = atoi(argv[1]);
    } else {
        printf("\nProvide the size of the array\n");
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
  
    //initialize array
    CALI_MARK_BEGIN("data_init_runtime");
    int *global_array = (int*)malloc(array_size * sizeof(int));
    int index;
    srand(time(NULL));
    for(index = 0; index < array_size; index++) {
        global_array[index] = rand() % 1000;    
    }
    CALI_MARK_END("data_init_runtime");
   
    //divide array
    int local_chunk_size = array_size / total_processes;

    //send subarrays using mpi scatter
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    int *local_array = (int*)malloc(local_chunk_size * sizeof(int));
    MPI_Scatter(global_array, local_chunk_size, MPI_INT, local_array, local_chunk_size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    int *temp_array = (int*)malloc(local_chunk_size * sizeof(int));
    merge_sort(local_array, temp_array, 0, local_chunk_size - 1);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
  
    //combine subarrays
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
 
    //merge sort on the master
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
    CALI_MARK_BEGIN("comm_large");
    MPI_Barrier(MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    
    mgr.stop();
    mgr.flush();
    MPI_Finalize();
    return 0;
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

    for (int i = left; i <= right; i++) {
        original[i] = temp[i];  
    }
}

void merge_sort(int *original, int *temp, int left, int right) {
    if (left < right) {
        int middle = (left + right) / 2;
        merge_sort(original, temp, left, middle);
        merge_sort(original, temp, middle + 1, right);
        merge_arrays(original, temp, left, middle, right);
    }
}

void check(int *array, int size_of_array) {
    CALI_MARK_BEGIN("correctness_check");
    for (int i = 1; i < size_of_array; i++) {
        if (array[i - 1] > array[i]) {
            printf("Array is not sorted at index %d\n", i);
            return;
        }
    }
    CALI_MARK_END("correctness_check");
    printf("Array is correctly sorted.\n");
}
