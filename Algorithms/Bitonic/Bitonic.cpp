#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <mpi.h>

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




std::vector<int> generate_random_array(int size) {

    srand(static_cast<unsigned>(time(0)));
    std::vector<int> arr(size);
    for (int i = 0; i < size; i++) {
        arr[i] = rand() % 1000; 
    }
    return arr;
}

bool check_sorted(std::vector<int>& sorted_arr) { 
    int j = 0;
    for (int i = 0; i < sorted_arr.size(); i++) { 
        if (i > j) { 
            j = i;
        }
        else if (i < j) { 
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


    const int N = pow(2, 22); 
    int local_size = N / comm_size;

    std::vector<int> input_arr(N);              // input (each thread gets a copy)
    std::vector<int> local_arr(local_size);     // thread will sort this local version
    std::vector<int> sorted_arr(N);             // buffer for root thred MPI_Gather
    
    if (rank == 0) {
        input_arr = generate_random_array(N);
    }

    cali::ConfigManager mgr;
    // mgr.add("runtime-report");
    mgr.start();

    double start = MPI_Wtime();


    // Give all threads input array / assign local regions
    MPI_Bcast(input_arr.data(), N, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatter(input_arr.data(), local_size, MPI_INT, local_arr.data(), local_size, MPI_INT, 0, MPI_COMM_WORLD);

    // Run the sort
    CALI_MARK_BEGIN("bitonic_sort");
    bitonic_sort(local_arr, 0, local_size, true);
    CALI_MARK_END("bitonic_sort");

    // Collect local arrays, root thread gets the whole array sorted (bitonically)
    MPI_Gather(local_arr.data(), local_arr.size(), MPI_INT, sorted_arr.data(), local_arr.size(), MPI_INT, 0, MPI_COMM_WORLD);

    
    if (rank == 0) {

        // Turn final bitonic sequence into sorted sequence
        bitonic_sort(sorted_arr, 0, N, true);
        
        double end = MPI_Wtime();
        double elapsed = end - start;
        std::cout << "Total time: " << elapsed << std::endl;
        std::cout << "Sorted? " << check_sorted(sorted_arr) << std::endl;

    }

    mgr.flush();

    MPI_Finalize();
    return 0;
}
