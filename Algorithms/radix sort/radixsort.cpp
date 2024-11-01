#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>

using std::vector;
using std::cout;
using std::endl;
using std::copy;
using std::string;

void generate_sorted_array(std::vector<int>& array, int size) {
    for (int i = 0; i < size; ++i) {
        array[i] = i;
    }
}
void generate_random_array(std::vector<int>& array, int size) {
    std::srand(time(NULL));
    for (int i = 0; i < size; ++i) {
        array[i] = std::rand() % 100000;
    }
}
void generate_reverse_sorted_array(std::vector<int>& array, int size) {
    for (int i = 0; i < size; ++i) {
        array[i] = size - i - 1;
    }
}
void generate_perturbed_array(std::vector<int>& array, int size) {
    generate_sorted_array(array, size); 
    // Randomly perturb 1% of the elements
    int perturb_count = size / 100;
    std::srand(time(NULL));
    for (int i = 0; i < perturb_count; ++i) {
        int index = std::rand() % size;
        int perturb_value = (std::rand() % 200) - 100;
        array[index] += perturb_value;
    }
}
void distribute_data(int *input, int sizeOfMatrix, int numworkers) {
    // Code copied from lab 2 (send_count = rows, averow = base_chunk)
    int base_chunk = sizeOfMatrix / numworkers;
    int extra = sizeOfMatrix % numworkers;
    int offset = 0;
    MPI_Request request;
    for (int worker = 1; worker <= numworkers; ++worker) {
        int send_count;
        if (worker <= extra) {
            send_count = base_chunk + 1;
        } else {
            send_count = base_chunk;
        }
        
        // printf("Master: Sending %d elements to worker %d starting at offset %d\n", send_count, worker, offset);
        CALI_MARK_BEGIN("comm");
        MPI_Isend(&offset, 1, MPI_INT, worker, 1, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        MPI_Isend(&send_count, 1, MPI_INT, worker, 1, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        MPI_Isend(input + offset, send_count, MPI_INT, worker, 1, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        CALI_MARK_END("comm");
        
        offset = offset + send_count;
    }
}
void gather_results(int *input, int numworkers) {
    MPI_Status status;
    MPI_Request request;

    int zero_offset = 0;  // Starting position for zero-bit values
    int one_offset = 0;   // Starting position for one-bit values after all zeros

    // Arrays to cache zero and one counts from each worker
    std::vector<int> zero_counts(numworkers);
    std::vector<int> one_counts(numworkers);

    // Step 1: Receive zero and one counts from each worker and calculate offsets
    for (int worker = 1; worker <= numworkers; ++worker) {
        int zero_count;
        int one_count;
        CALI_MARK_BEGIN("comm");
        // Receive the counts from each worker
        MPI_Irecv(&zero_count, 1, MPI_INT, worker, 1, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        MPI_Irecv(&one_count, 1, MPI_INT, worker, 1, MPI_COMM_WORLD, &request);
        MPI_Wait(&request, MPI_STATUS_IGNORE);
        CALI_MARK_END("comm");
        // Cache the counts
        zero_counts[worker - 1] = zero_count;
        one_counts[worker - 1] = one_count;

        // Accumulate zero counts to determine the starting position for ones
        one_offset += zero_count;
    }

    zero_offset = 0; // Reset zero_offset to start placing zeros from the beginning

    // Step 2: Receive zero and one values directly into `input` at calculated offsets
    for (int worker = 1; worker <= numworkers; ++worker) {
        int zero_count = zero_counts[worker - 1];
        int one_count = one_counts[worker - 1];

        // Step 2a: Receive zero-bit values directly into `input` at zero_offset
        if (zero_count > 0) {
            CALI_MARK_BEGIN("comm");
            MPI_Irecv(input + zero_offset, zero_count, MPI_INT, worker, 1, MPI_COMM_WORLD, &request);
            MPI_Wait(&request, MPI_STATUS_IGNORE);
            CALI_MARK_END("comm");
            zero_offset += zero_count; // Update zero_offset for the next worker’s data
        }

        // Step 2b: Receive one-bit values directly into `input` at one_offset
        if (one_count > 0) {
            CALI_MARK_BEGIN("comm");
            MPI_Irecv(input + one_offset, one_count, MPI_INT, worker, 1, MPI_COMM_WORLD, &request);
            MPI_Wait(&request, MPI_STATUS_IGNORE);
            CALI_MARK_END("comm");
            one_offset += one_count; // Update one_offset for the next worker’s data
        }
    }
}
void binary_radix_sort_master(int size, int *input, int workers){
   CALI_MARK_BEGIN("comp");
   CALI_MARK_BEGIN("comp_small");
   MPI_Request request;
   // Getting the maximum value in the array for figuring out maximum bit representation length
   int max = input[0];
   for (int i = 1; i < size; i++) {
      if (input[i] > max) {
         max = input[i];
      }
   }
   int num_bits = (max == 0) ? 1 : static_cast<int>(std::log2(max)) + 1;
   CALI_MARK_END("comp_small");
   CALI_MARK_END("comp");

   cout << "Master: Starting Radix Sort with: " << workers << " workers and a maximum number of bits: " << num_bits << endl;

   // The Main loop over all bits (from least significant bit to most significant bit)
   for (int bit_index = 0; bit_index < num_bits; bit_index++){
     // printf("Master: Processing bit %d\n", bit_index);

     distribute_data(input, size, workers);

     MPI_Bcast(&bit_index, 1, MPI_INT, 0, MPI_COMM_WORLD);

     gather_results(input, workers);
   }

   // Sending the termination signal to workers
   int terminate_signal = -1;
   for (int dest = 1; dest <= workers; dest++){
      CALI_MARK_BEGIN("comm");
      MPI_Isend(&terminate_signal, 1, MPI_INT, dest, 1, MPI_COMM_WORLD, &request);
      MPI_Wait(&request, MPI_STATUS_IGNORE);
      CALI_MARK_END("comm");
      // printf("Master: Sent termination signal to worker %d\n", dest);
   }
}

void worker_process(int *input){
   int offset;
   int elements;
   int bit;
   MPI_Status status;
   MPI_Request request;
   while (true){
      CALI_MARK_BEGIN("comm");
      MPI_Irecv(&offset, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &request);
      MPI_Wait(&request, MPI_STATUS_IGNORE);
      
      if (offset == -1){
         // printf("Worker %d received termination signal.\n", MPI::COMM_WORLD.Get_rank());
         break; 
      }

      MPI_Irecv(&elements, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &request);
      MPI_Wait(&request, MPI_STATUS_IGNORE);

      // printf("Worker %d is processing %d elements.\n", MPI::COMM_WORLD.Get_rank(), elements);

      vector<int> local_arr(elements);
      
      MPI_Irecv(local_arr.data(), elements, MPI_INT, 0, 1, MPI_COMM_WORLD, &request);
      MPI_Wait(&request, MPI_STATUS_IGNORE);
      
      MPI_Bcast(&bit, 1, MPI_INT, 0, MPI_COMM_WORLD);
      CALI_MARK_END("comm");
      vector<int> zeroBits;
      vector<int> onesBits;

      // Split the local chunk based on the current bit
      CALI_MARK_BEGIN("comp");
      CALI_MARK_BEGIN("comp_large");
      for (const auto& number : local_arr) {
         if ((number >> bit) & 1) {
            onesBits.push_back(number);
         } else {
            zeroBits.push_back(number);
         }
      }
      int zeros_size = zeroBits.size();
      int ones_size = onesBits.size();
      CALI_MARK_END("comp_large");
      CALI_MARK_END("comp");
      //MPI_Isend(&elements, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &request);
      //MPI_Wait(&request, MPI_STATUS_IGNORE);
      CALI_MARK_BEGIN("comm");
      MPI_Isend(&zeros_size, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &request);
      MPI_Wait(&request, MPI_STATUS_IGNORE);
      MPI_Isend(&ones_size, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &request);
      MPI_Wait(&request, MPI_STATUS_IGNORE);

      if (zeros_size > 0) {
         MPI_Isend(zeroBits.data(), zeros_size, MPI_INT, 0, 1, MPI_COMM_WORLD, &request);
         MPI_Wait(&request, MPI_STATUS_IGNORE);
      }
      if (ones_size > 0) {
         MPI_Isend(onesBits.data(), ones_size, MPI_INT, 0, 1, MPI_COMM_WORLD, &request);
         MPI_Wait(&request, MPI_STATUS_IGNORE);
      }
      CALI_MARK_END("comm");
      // printf("Worker %d sent %d zeros and %d ones to master.\n", MPI::COMM_WORLD.Get_rank(), zeros_size, ones_size);
   }
}

int main(int argc, char *argv[]){

   CALI_CXX_MARK_FUNCTION;
   int size;
   int array_type;

   if (argc == 3) {
        size = std::atoi(argv[1]);
        array_type = std::atoi(argv[2]);
   }
   else {
        std::cerr << "Usage: " << argv[0] << " <size> <numProcesses> <arrayType>" << endl; 
        return 1;
   }

   int tasks;
   int taskid;
   int numworkers;
   vector<int> input(size);

   MPI_Init(&argc, &argv);
   MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
   MPI_Comm_size(MPI_COMM_WORLD, &tasks);
   
   cali::ConfigManager mgr;
   mgr.start();
   
   numworkers = tasks - 1;
   adiak::init(NULL);
   adiak::launchdate();    // launch date of the job
   adiak::libraries();     // Libraries used
   adiak::cmdline();       // Command line used to launch the job
   adiak::clustername();   // Name of the cluster
   adiak::value("algorithm", "binary_radix_sort"); // The name of the algorithm
   adiak::value("programming_model", "mpi"); // e.g., "mpi"
   adiak::value("data_type", "int"); // The datatype of input elements
   adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes
   adiak::value("input_size", size); // The number of elements in input dataset
   adiak::value("input_type", array_type); // For sorting
   adiak::value("num_procs", tasks); // The number of processors (MPI ranks)
   adiak::value("scalability", "strong"); // The scalability of your algorithm
   adiak::value("group_num", 22); // The number of your group
   adiak::value("implementation_source", "online/ai/handwritten");
   
   if (taskid == 0){
      // Master process
      CALI_MARK_BEGIN("data_init_runtime");
      
      if (array_type == 0) {
         generate_random_array(input, size);
      } else if (array_type == 1) {
         generate_sorted_array(input, size);
      } else if (array_type == 2) {
         generate_reverse_sorted_array(input, size);
      } else if (array_type == 3) {
         generate_perturbed_array(input, size);
      } else {
         std::cerr << "Invalid array type: " << array_type << endl;
         MPI_Abort(MPI_COMM_WORLD, 1);
      }
		  
      CALI_MARK_END("data_init_runtime");

      cout << "Master: Starting sorting with array size " << size << endl;

      // Whole program computation timing
      double whole_program_start = MPI_Wtime();
      // Perform radix sort with help from worker processes
      binary_radix_sort_master(size, input.data(), numworkers);
      // Whole program computation end
      double whole_program_end = MPI_Wtime();
      cout << "Master: Total time for sorting array type " << array_type << ":" << (whole_program_end - whole_program_start) << " seconds." << endl;

      CALI_MARK_BEGIN("correctness_check");
      // Check if array is sorted
      bool is_sorted = true;
      for (int i = 1; i < size; ++i){
         if (input[i - 1] > input[i]){
            is_sorted = false;
            break;
         }
      }
      CALI_MARK_END("correctness_check");

      if (is_sorted){
         cout << "Master: Array is sorted." << endl;
         //for (int i = 0; i < size; ++i) {
           //cout << input[i] << " ";
         //}
      } else {
         cout << "Master: Array is NOT sorted." << endl;
      }
   } else {
      // Worker processes
      worker_process(input.data());
   }

   // Barrier and log to ensure processes reach the barrier
   //printf("Process %d reached the final barrier.\n", taskid);
   MPI_Barrier(MPI_COMM_WORLD);
   //printf("Process %d passed the final barrier.\n", taskid);

   mgr.stop();
   mgr.flush();
   
   MPI_Finalize();
}