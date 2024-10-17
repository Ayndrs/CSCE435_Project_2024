#include <mpi.h>
#include <vector>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>


using std::vector;
using std::cout;
using std::endl;
using std::copy;
using std::string;

void generate_sorted_array(std::vector<int>& array, int size) {
    CALI_MARK_BEGIN("data_init_runtime");
    for (int i = 0; i < size; ++i) {
        array[i] = i;
    }
    CALI_MARK_END("data_init_runtime");
}
void generate_random_array(std::vector<int>& array, int size) {
    CALI_MARK_BEGIN("data_init_runtime");
    std::srand(time(NULL));
    for (int i = 0; i < size; ++i) {
        array[i] = std::rand() % 100000;
    }
    CALI_MARK_END("data_init_runtime");
}
void generate_reverse_sorted_array(std::vector<int>& array, int size) {
    CALI_MARK_BEGIN("data_init_runtime");
    for (int i = 0; i < size; ++i) {
        array[i] = size - i - 1;
    }
    CALI_MARK_END("data_init_runtime");
}
void generate_perturbed_array(std::vector<int>& array, int size) {
    CALI_MARK_BEGIN("data_init_runtime");
    generate_sorted_array(array, size); 
    // Randomly perturb 1% of the elements
    int perturb_count = size / 100;
    std::srand(time(NULL));
    for (int i = 0; i < perturb_count; ++i) {
        int index = std::rand() % size;
        int perturb_value = (std::rand() % 200) - 100;
        array[index] += perturb_value;
    }
    CALI_MARK_END("data_init_runtime");
}
void distribute_data(int *input, int sizeOfMatrix, int numworkers) {
    // Code copied from lab 2 (send_count = rows, averow = base_chunk)
    CALI_MARK_BEGIN("comm");
    int base_chunk = sizeOfMatrix / numworkers;
    int extra = sizeOfMatrix % numworkers;
    int offset = 0;
    for (int worker = 1; worker <= numworkers; ++worker) {
        int send_count;
        if (worker <= extra) {
            send_count = base_chunk + 1;
        } else {
            send_count = base_chunk;
        }
        if (send_count <= 10) {
            CALI_MARK_BEGIN("comm_small");
        } else {
            CALI_MARK_BEGIN("comm_large");
        }
        
        MPI_Send(&offset, 1, MPI_INT, worker, 1, MPI_COMM_WORLD);
        MPI_Send(&send_count, 1, MPI_INT, worker, 1, MPI_COMM_WORLD);
        MPI_Send(input + offset, send_count, MPI_INT, worker, 1, MPI_COMM_WORLD);
        
        if (send_count <= 10) {
            CALI_MARK_END("comm_small");
        } else {
            CALI_MARK_END("comm_large");
        }
        offset = offset + send_count;
    }
    CALI_MARK_END("comm");
}
void gather_results(int *input, int numworkers) {
    CALI_MARK_BEGIN("comm");
    MPI_Status status;
    vector<int> zero_values;
    vector<int> one_values;

    for (int worker = 1; worker <= numworkers; ++worker) {
        int received_size;
        int zero_count;
        int one_count;
        // receive number of values with nth bit value of 0 or 1, then fill in the zeros and ones vectors with the actual values corresponding to those bits to edit the orriginal array
        MPI_Recv(&received_size, 1, MPI_INT, worker, 1, MPI_COMM_WORLD, &status);
        MPI_Recv(&zero_count, 1, MPI_INT, worker, 1, MPI_COMM_WORLD, &status);
        MPI_Recv(&one_count, 1, MPI_INT, worker, 1, MPI_COMM_WORLD, &status);

        vector<int> zeros(zero_count);
        vector<int> ones(one_count);

        MPI_Recv(zeros.data(), zero_count, MPI_INT, worker, 1, MPI_COMM_WORLD, &status);
        MPI_Recv(ones.data(), one_count, MPI_INT, worker, 1, MPI_COMM_WORLD, &status);

        zero_values.insert(zero_values.end(), zeros.begin(), zeros.end());
        one_values.insert(one_values.end(), ones.begin(), ones.end());
    }
    copy(zero_values.begin(), zero_values.end(), input);
    copy(one_values.begin(), one_values.end(), input + zero_values.size());
    CALI_MARK_END("comm");
}
void binary_radix_sort_master(int size, int *input, int workers){
   CALI_MARK_BEGIN("comp");
   // Getting the maximum value in the array for figuring out maximum bit representation length
   int max = input[0];
   for (int i = 1; i < size; i++) {
      if (input[i] > max) {
         max = input[i];
      }
   }
   int num_bits = (max == 0) ? 1 : static_cast<int>(std::log2(max)) + 1;

   cout << "Master: Starting Radix Sort with: " << workers << " workers and a maximum number of bits: " << num_bits << endl;
   // The Main loop over all bits (from least significant bit to most significant bit)
   for (int bit_index = 0; bit_index < num_bits; bit_index++){
     distribute_data(input, size, workers);
     MPI_Bcast(&bit_index, 1, MPI_INT, 0, MPI_COMM_WORLD);
     gather_results(input, workers);
   }
   // Sending the termination signal to workers
   int terminate_signal = -1;
   for (int dest = 1; dest <= workers; dest++){
      MPI_Send(&terminate_signal, 1, MPI_INT, dest, 1, MPI_COMM_WORLD);
   }
   CALI_MARK_END("comp");
}

void worker_process(int *input){
   CALI_MARK_BEGIN("comp");
   int offset;
   int elements;
   int bit;
   MPI_Status status;

   while (true){
      MPI_Recv(&offset, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
      MPI_Recv(&elements, 1, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);

      if (elements == -1){
         break; 
      }
      // Receive the local array chunk data, and broadcast the current bit to all workers
      vector<int> local_arr(elements);
      
      MPI_Recv(local_arr.data(), elements, MPI_INT, 0, 1, MPI_COMM_WORLD, &status);
      MPI_Bcast(&bit, 1, MPI_INT, 0, MPI_COMM_WORLD);

      vector<int> zeroBits;
      vector<int> onesBits;
      // Split the local chunk based on the current bit
      for (const auto& number : local_arr) {
         if ((number >> bit) & 1) {
            onesBits.push_back(number);
         } else {
            zeroBits.push_back(number);
         }
      }
      int zeros_size = zeroBits.size();
      int ones_size = onesBits.size();

      MPI_Send(&elements, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
      MPI_Send(&zeros_size, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
      MPI_Send(&ones_size, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
      // send the data of the vectors containing the values with nth bits being 0 or 1 to master process
      if (zeros_size > 0) {
         MPI_Send(zeroBits.data(), zeros_size, MPI_INT, 0, 1, MPI_COMM_WORLD);
      }
      if (ones_size > 0) {
         MPI_Send(onesBits.data(), ones_size, MPI_INT, 0, 1, MPI_COMM_WORLD);
      }
   }
   CALI_MARK_END("comp");
}

int main(int argc, char *argv[]){

   CALI_CXX_MARK_FUNCTION;
   cali::ConfigManager mgr;
   mgr.start();
   int size;
   string array_type;
   const char* main = "main";
   CALI_MARK_BEGIN(main);

   
   if (argc == 3) {
        size = std::atoi(argv[1]);
        array_type = argv[2];
   }
   else {
        std::cerr << "Usage: " << argv[0] << " <size> <numProcesses> <arrayType>" << endl; 
        cout << array_type;
        return 1;
    }
   int tasks;
   int taskid;
   int numworkers;
   const char* correctness_check = "correctness_check";
   vector<int> input(size);

   MPI_Init(&argc, &argv);
   MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
   MPI_Comm_size(MPI_COMM_WORLD, &tasks);

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
      if (array_type == "random") {
         generate_random_array(input, size);
      } else if (array_type == "sorted") {
         generate_sorted_array(input, size);
      } else if (array_type == "reverse") {
         generate_reverse_sorted_array(input, size);
      } else if (array_type == "perturbed") {
         generate_perturbed_array(input, size);
      } else {
         std::cerr << "Invalid array type: " << array_type << endl;
         MPI_Abort(MPI_COMM_WORLD, 1);
      }

      cout << "Master: Starting sorting with array size " << size << endl;

      // Whole program computation timing
      double whole_program_start = MPI_Wtime();
      // Perform radix sort with help from worker processes
      binary_radix_sort_master(size, input.data(), numworkers);
      // Whole program computation end
      double whole_program_end = MPI_Wtime();
      cout << "Master: Total time for sorting: " << (whole_program_end - whole_program_start) << " seconds." << endl;
      CALI_MARK_BEGIN(correctness_check);
      // Check if array is sorted
      bool is_sorted = true;
      for (int i = 1; i < size; ++i){
         if (input[i - 1] > input[i]){
            is_sorted = false;
            break;
         }
      }
      CALI_MARK_END(correctness_check);

      if (is_sorted)
         cout << "Master: Array is sorted." << endl;
      else
         cout << "Master: Array is NOT sorted." << endl;
   }
   else{
      // Worker processes
      worker_process(input.data());
   }
   
   CALI_MARK_END(main);
   mgr.stop();
   mgr.flush();
   MPI_Finalize();
}