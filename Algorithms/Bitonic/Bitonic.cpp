#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <mpi.h>
#include <string>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>


void bitonic_sort(std::vector<int>& arr, int low, int count, bool ascending);
void bitonic_merge(std::vector<int>& arr, int low, int count, bool ascending);


void bitonic_sort(std::vector<int>& arr, int low, int count, bool ascending) {
    if (count > 1) {
        int k = count / 2;
        bitonic_sort(arr, low, k, true);  
        bitonic_sort(arr, low + k, k, false); 
        bitonic_merge(arr, low, count, ascending);
    }
}

void bitonic_merge(std::vector<int>& arr, int low, int count, bool ascending) {
    if (count > 1) {
        int k = count / 2;
        for (int i = low; i < low + k; i++) {
            if ((arr[i] > arr[i + k]) == ascending) {
                std::swap(arr[i], arr[i + k]);
            }
        }
        bitonic_merge(arr, low, k, ascending);
        bitonic_merge(arr, low + k, k, ascending);
    }
}




std::vector<int> generate_sorted_array(int size) { 
    std::vector<int> arr(size);
    for (int i = 0; i < size; i++) { 
        arr[i] = i;
    }
    return arr;
}

std::vector<int> generate_random_array(int size) {

    srand(static_cast<unsigned>(time(0)));
    std::vector<int> arr(size);
    for (int i = 0; i < size; i++) {
        arr[i] = rand() % 1000; 
    }
    return arr;
}

std::vector<int> generate_reverse_sorted_array(int size) { 
    std::vector<int> arr(size);
    for (int i = 0; i < size; i++) { 
        arr[i] = size - i;
    }
    return arr;
}

std::vector<int> generate_perturbed_array(int size) {
    std::vector<int> arr = generate_sorted_array(size);
    int number_of_swaps = static_cast<int>(arr.size() * 0.01);

    srand(static_cast<unsigned>(time(0)));


    for (int i = 0; i < number_of_swaps; i++) { 
        int loc_1 = rand() % size;
        int loc_2 = rand() % size;
        std::swap(arr[loc_1], arr[loc_2]);
    }
    return arr;
}



bool check_sorted(const std::vector<int>& sorted_arr) {
    for (size_t i = 1; i < sorted_arr.size(); i++) {
        if (sorted_arr[i - 1] > sorted_arr[i]) {
            return false;
        }
    }
    return true;
}






int main(int argc, char** argv) {

    MPI_Init(&argc, &argv);
    int rank, comm_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_size);


    int power = std::atoi(argv[1]);
    const int N = pow(2, power); 
    int local_size = N / comm_size;

    std::vector<int> input_arr(N);              // input (each thread gets a copy)
    std::vector<int> local_arr(local_size);     // thread will sort this local version
    std::vector<int> sorted_arr(N);             // buffer for root thred MPI_Gather
    std::string method;

    // 0 - sorted
    // 1 - reverse sorted
    // 2 - 1% perturbed
    // 3 - random


    int sort_method = std::atoi(argv[2]);
    if (rank == 0) {
        CALI_MARK_BEGIN("data_init");
        if (sort_method == 0) { 
            input_arr = generate_sorted_array(N);
            method = "sorted";
        }
        else if (sort_method == 1) { 
            input_arr = generate_reverse_sorted_array(N);
            method = "ReverseSorted";
        }
        else if (sort_method == 2) { 
            input_arr = generate_perturbed_array(N);
            method = "Random";
        }
        else { 
            input_arr = generate_random_array(N);
            method = "1_perc_perturbed";
        }
        CALI_MARK_END("data_init");
        
    }

    adiak::init(NULL);
    adiak::launchdate();    // launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "bitonic"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g. "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", "4"); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", N); // The number of elements in input dataset (1000)
    adiak::value("input_type", method); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
    adiak::value("num_procs", comm_size); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", 22); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "ai"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").


    cali::ConfigManager mgr;
    // mgr.add("runtime-report");
    mgr.start();

    double start = MPI_Wtime();
    CALI_MARK_BEGIN("comp");


    // Give all threads input array / assign local regions
    MPI_Bcast(input_arr.data(), N, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatter(input_arr.data(), local_size, MPI_INT, local_arr.data(), local_size, MPI_INT, 0, MPI_COMM_WORLD);

    // Run the sort
    CALI_MARK_BEGIN("comp_large");
    bitonic_sort(local_arr, 0, local_size, true);
    CALI_MARK_END("comp_large");

    CALI_MARK_BEGIN("comm");
    // Collect local arrays, root thread gets the whole array sorted (bitonically)
    MPI_Gather(local_arr.data(), local_arr.size(), MPI_INT, sorted_arr.data(), local_arr.size(), MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm");
    
    if (rank == 0) {

        // Turn final bitonic sequence into sorted sequence
        bitonic_sort(sorted_arr, 0, N, true);
        
        double end = MPI_Wtime();
        double elapsed = end - start;
        std::cout << "Total time: " << elapsed << std::endl;
        


        CALI_MARK_BEGIN("correctness_check");
        bool isSorted = check_sorted(sorted_arr);
        CALI_MARK_END("correctness_check");


        std::cout << "Sorted? " << isSorted << std::endl;

    }

    CALI_MARK_END("comp");

    mgr.flush();

    MPI_Finalize();
    return 0;
}
