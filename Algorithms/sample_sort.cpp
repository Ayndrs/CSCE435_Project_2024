#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

void generate_random_data(std::vector<unsigned int>& local_data, int rank) {
    srand(time(NULL) + rank); // Use rank to differentiate seeds
    for (unsigned int& elem : local_data) {
        elem = rand();
    }
}

std::vector<unsigned int> select_local_samples(const std::vector<unsigned int>& local_data, unsigned int num_samples) {
    int local_size = local_data.size();
    std::vector<int> local_samples(num_samples);
    
    for (unsigned int i = 0; i < num_samples; i++) {
        local_samples[i] = local_data[i * local_size / num_samples];
    }
    
    return local_samples;
}


int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    // Step 1: Generate random data on each process
    unsigned int num_elements;
    if(argc < 2) {
        printf("Usage: %s <num elements>\n", argv[0]);
        return 1;
    }
    num_elements = atoi(argv[1]);
    unsigned int local_size = num_elements / num_procs;
    std::vector<unsigned int> local_data(local_size);
    generate_random_data(local_data, rank);

    // Step 2: Local sorting
    std::sort(local_data.begin(), local_data.end());

    // Step 3: Sample selection
    int num_samples = num_procs - 1;
    std::vector<int> local_samples = select_local_samples(local_data, num_samples);

    // Step 4: Gathering samples and choosing pivots
    std::vector<int> all_samples(num_samples * num_procs);
    MPI_Allgather(local_samples.data(), num_samples, MPI_INT, 
                  all_samples.data(), num_samples, MPI_INT, MPI_COMM_WORLD);

    // Sort all samples and select pivots
    std::sort(all_samples.begin(), all_samples.end());
    std::vector<int> pivots(num_procs - 1);
    for (int i = 0; i < num_procs - 1; i++) {
        pivots[i] = all_samples[(i + 1) * num_samples];
    }

    // Step 5: Partition local data based on pivots
    std::vector<std::vector<int>> partitions = partition_data(local_data, pivots);

    // Step 6: All-to-All communication
    std::vector<int> send_counts(num_procs), recv_counts(num_procs);
    for (int i = 0; i < num_procs; i++) {
        send_counts[i] = partitions[i].size();
    }

    MPI_Alltoall(send_counts.data(), 1, MPI_INT, recv_counts.data(), 1, MPI_INT, MPI_COMM_WORLD);

    std::vector<int> send_displs(num_procs, 0), recv_displs(num_procs, 0);
    for (int i = 1; i < num_procs; i++) {
        send_displs[i] = send_displs[i - 1] + send_counts[i - 1];
        recv_displs[i] = recv_displs[i - 1] + recv_counts[i - 1];
    }

    std::vector<int> send_buffer;
    for (const auto& partition : partitions) {
        send_buffer.insert(send_buffer.end(), partition.begin(), partition.end());
    }

    int total_recv_count = std::accumulate(recv_counts.begin(), recv_counts.end(), 0);
    std::vector<int> recv_buffer(total_recv_count);

    MPI_Alltoallv(send_buffer.data(), send_counts.data(), send_displs.data(), MPI_INT,
                  recv_buffer.data(), recv_counts.data(), recv_displs.data(), MPI_INT, MPI_COMM_WORLD);

    // Step 7: Final local sorting
    local_sort(recv_buffer);

    // At this point, each process has its sorted partition of the global data
    std::cout << "Process " << rank << " sorted " << recv_buffer.size() << " elements." << std::endl;

    MPI_Finalize();
    return 0;
}