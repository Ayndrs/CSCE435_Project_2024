#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <algorithm>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#include <iostream>
#include <vector>
#include <random>

// Helper function to generate random data
std::vector<int> generate_random_data(int size, int lower = 0, int upper = 1000000) {
    std::vector<int> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(lower, upper);
    for (int i = 0; i < size; ++i) {
        data[i] = dis(gen);
    }
    return data;
}

std::vector<int> generate_sorted_data(int size, int min, int max) {
    std::vector<int> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    data[0] = dis(gen);
    for (int i = 1; i < size; ++i) {
        data[i] = data[i - 1] + dis(gen);
    }
    return data;
}

std::vector<int> generate_reverse_sorted_data(int size, int min, int max) {
    std::vector<int> data(size);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    data[size - 1] = dis(gen);
    for (int i = size - 2; i >= 0; --i) {
        data[i] = data[i + 1] + dis(gen);
    }
    return data;
}

std::vector<int> generate_one_percent_perturbed_data(int size, int min, int max) {
    std::vector<int> data = generate_sorted_data(size, min, max);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, size / 100);
    for (int i = 0; i < size / 100; ++i) {
        int idx1 = dis(gen);
        int idx2 = dis(gen);
        std::swap(data[idx1], data[idx2]);
    }
    return data;
}

bool check_correctness(std::vector<int> &local_data, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // Step 1: Check if local data is sorted
    bool is_local_sorted = true;
    for (size_t i = 1; i < local_data.size(); ++i) {
        if (local_data[i] < local_data[i - 1]) {
            is_local_sorted = false;
            break;
        }
    }

    // Step 2: Exchange boundary data between neighbors
    int local_min = local_data.empty() ? INT_MAX : local_data.front();
    int local_max = local_data.empty() ? INT_MIN : local_data.back();
    int recv_min = 0;

    // Check with the next processor (rank + 1)
    if (rank < size - 1) {
        // Send local max to rank + 1 and receive min from rank + 1
        MPI_Send(&local_max, 1, MPI_INT, rank + 1, 0, comm);
        MPI_Recv(&recv_min, 1, MPI_INT, rank + 1, 0, comm, MPI_STATUS_IGNORE);
        if (recv_min < local_max) {
            std::cout << "Rank " << rank << " is not sorted with rank " << rank + 1 << std::endl;
            is_local_sorted = false; // Local max should be <= next processor's min
        }
    }

    // Check with the previous processor (rank - 1)
    if (rank > 0) {
        int prev_max;
        // Receive max from rank - 1 and send local min to rank - 1
        MPI_Recv(&prev_max, 1, MPI_INT, rank - 1, 0, comm, MPI_STATUS_IGNORE);
        MPI_Send(&local_min, 1, MPI_INT, rank - 1, 0, comm);
        if (prev_max > local_min) {
            std::cout << "Rank " << rank << " is not sorted with rank " << rank - 1 << std::endl;
            is_local_sorted = false; // Previous max should be <= local min
        }
    }

    // Step 3: Perform global AND to check if all processors are sorted
    bool is_globally_sorted;
    MPI_Allreduce(&is_local_sorted, &is_globally_sorted, 1, MPI_C_BOOL, MPI_LAND, comm);

    return is_globally_sorted;
}


// SampleSort implementation
void sample_sort(std::vector<int> &local_data, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // Step 1: Locally sort the data on each processor
    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    std::sort(local_data.begin(), local_data.end());
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
    
    // Step 2: Choose samples from each processor (e.g., uniformly select 'size-1' samples)
    int samples_per_proc = size - 1;
    std::vector<int> local_samples(samples_per_proc);
    for (int i = 0; i < samples_per_proc; ++i) {
        local_samples[i] = local_data[(i + 1) * (local_data.size() / size)];
    }
    // Step 3: Gather all samples at root processor
    std::vector<int> all_samples;
    if (rank == 0) {
        all_samples.resize(size * samples_per_proc);
    }
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small");
    CALI_MARK_BEGIN("MPI_Gather");
    MPI_Gather(local_samples.data(), samples_per_proc, MPI_INT,
               all_samples.data(), samples_per_proc, MPI_INT, 0, comm);
    CALI_MARK_END("MPI_Gather");
    CALI_MARK_END("comm_small");
    CALI_MARK_END("comm");

    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_small");
    // Step 4: Root processor sorts all the gathered samples and selects pivots
    std::vector<int> pivots(size - 1);
    if (rank == 0) {
        std::sort(all_samples.begin(), all_samples.end());
        for (int i = 0; i < size - 1; ++i) {
            pivots[i] = all_samples[(i + 1) * samples_per_proc];
        }
    }
    CALI_MARK_END("comp_small");
    CALI_MARK_END("comp");

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_small");
    CALI_MARK_BEGIN("MPI_Bcast");
    // Step 5: Broadcast pivots to all processors
    MPI_Bcast(pivots.data(), size - 1, MPI_INT, 0, comm);
    CALI_MARK_END("MPI_Bcast");
    CALI_MARK_END("comm_small");
    CALI_MARK_END("comm");
    
    // Step 6: Partition local data based on the pivots
    std::vector<std::vector<int>> partitions(size);
    auto it = local_data.begin();
    for (int i = 0; i < size - 1; ++i) {
        auto bound = std::lower_bound(it, local_data.end(), pivots[i]);
        partitions[i].insert(partitions[i].end(), it, bound);
        it = bound;
    }
    partitions[size - 1].insert(partitions[size - 1].end(), it, local_data.end());

    // Step 7: Send partitioned data to respective processors
    std::vector<int> send_counts(size), recv_counts(size);
    for (int i = 0; i < size; ++i) {
        send_counts[i] = partitions[i].size();
    }
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    CALI_MARK_BEGIN("MPI_Alltoall");
    MPI_Alltoall(send_counts.data(), 1, MPI_INT, recv_counts.data(), 1, MPI_INT, comm);
    CALI_MARK_END("MPI_Alltoall");
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");
    // Step 8: Exchange data
    std::vector<int> send_buffer, recv_buffer;
    for (int i = 0; i < size; ++i) {
        send_buffer.insert(send_buffer.end(), partitions[i].begin(), partitions[i].end());
    }
    int total_recv = std::accumulate(recv_counts.begin(), recv_counts.end(), 0);
    recv_buffer.resize(total_recv);
    std::vector<int> send_displs(size), recv_displs(size);
    send_displs[0] = recv_displs[0] = 0;
    for (int i = 1; i < size; ++i) {
        send_displs[i] = send_displs[i - 1] + send_counts[i - 1];
        recv_displs[i] = recv_displs[i - 1] + recv_counts[i - 1];
    }
    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    CALI_MARK_BEGIN("MPI_Alltoallv");
    MPI_Alltoallv(send_buffer.data(), send_counts.data(), send_displs.data(), MPI_INT,
                  recv_buffer.data(), recv_counts.data(), recv_displs.data(), MPI_INT, comm);
    CALI_MARK_END("MPI_Alltoallv");
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    // Step 9: Locally sort the received data
    local_data = recv_buffer;
    std::sort(local_data.begin(), local_data.end());
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");
}

int main(int argc, char *argv[]) {
    CALI_CXX_MARK_FUNCTION;
    // CALI_MARK_BEGIN("main");
    CALI_MARK_BEGIN("MPI_Init");
    MPI_Init(&argc, &argv);
    CALI_MARK_END("MPI_Init");

    // cali_mpi_init(); // Initialize Caliper MPI support

    int rank, size;
    CALI_MARK_BEGIN("MPI_Comm_rank");
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    CALI_MARK_END("MPI_Comm_rank");
    CALI_MARK_BEGIN("MPI_Comm_size");
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    CALI_MARK_END("MPI_Comm_size");

    cali::ConfigManager mgr;
    mgr.start();
    // Mark the main function in Caliper
    double start_time = MPI_Wtime();

    double init_time = MPI_Wtime();
    // Determine the size of the local data
    if(argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <total_data_size> <input_type>" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
    int total_data_size = atoi(argv[1]);
    int local_data_size = total_data_size / size;
    int input_type = atoi(argv[2]);
    std::vector<int> local_data;
    CALI_MARK_BEGIN("data_init_runtime");
    switch (input_type) {
        case 0:
            local_data = generate_random_data(local_data_size);
            break;
        case 1:
            local_data = generate_sorted_data(local_data_size, 0, 1000000);
            break;
        case 2:
            local_data = generate_reverse_sorted_data(local_data_size, 0, 1000000);
            break;
        case 3:
            local_data = generate_one_percent_perturbed_data(local_data_size, 0, 1000000);
            break;
        default:
            std::cerr << "Invalid input type: " << input_type << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
    }
    CALI_MARK_END("data_init_runtime");
    init_time = MPI_Wtime() - init_time;

    double start_sort_time = MPI_Wtime();

    // Perform sample sort
    sample_sort(local_data, MPI_COMM_WORLD);
    start_sort_time = MPI_Wtime() - start_sort_time;

    // Check correctness
    double correctness_time = MPI_Wtime();
    CALI_MARK_BEGIN("correctness_check");
    bool is_sorted = check_correctness(local_data, MPI_COMM_WORLD);
    if (rank == 0) {
        std::cout << "Data is sorted: " << (is_sorted ? "True" : "False") << std::endl;
    }
    CALI_MARK_END("correctness_check");
    correctness_time = MPI_Wtime() - correctness_time;

    // End timing
    double end_time = MPI_Wtime();
    

    // Print result on rank 0
    if (rank == 0) {
        std::cout << "Sample sort completed in " << (end_time - start_time) << " seconds." << std::endl;
    }

    // Finalize Caliper and MPI
    // cali_mpi_finalize();
    //CALI_MARK_END("main");
    mgr.stop();
    mgr.flush();

    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::value("algorithm", "sample_sort");
    adiak::value("programming_model", "mpi");
    adiak::value("data_type", "int");
    adiak::value("size_of_data_type", sizeof(int));
    adiak::value("input_size", total_data_size);
    switch(input_type) {
        case 0:
            adiak::value("input_type", "random");
            break;
        case 1:
            adiak::value("input_type", "sorted");
            break;
        case 2:
            adiak::value("input_type", "reverse_sorted");
            break;
        case 3:
            adiak::value("input_type", "one_percent_perturbed");
            break;
    }
    adiak::value("num_procs", size); 
    adiak::value("scalability", "strong"); 
    adiak::value("group_num", 22); 
    adiak::value("implementation_source", "online");

    MPI_Finalize();
    return 0;
}
