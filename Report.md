# CSCE 435 Group project

## 0. Group number: 22

## 1. Group members:
1. Gohyun Kim
2. Simon Sprouse
3. Michael Nix
4. Austin Karimi

## 2. Project topic (e.g., parallel sorting algorithms)

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort - Simon Sprouse
- Sample Sort
- Merge Sort - Gohyun Kim
- Radix Sort - Austin Karimi
  - I plan to implement radix sort using some parallelization strategies on a distributed memory system, utilizing grace for performance evaluation. I plan to utilize multiple nodes where the communication between them will occur via MPI.

How we will communicate: Discord
What versions do you plan to compare: An array that is sorted, unsorted, randomized. How time to calculate differs based on number of processes.

This project will compare the performance of four parallel sorting algorithms: Bitonic Sort, Sample Sort, Merge Sort, and Radix Sort. 
These algorithms will be implemented on different architectures to evaluate their scalability, speedup, and suitability for various data sets 
and computing environments such as testing on different processes, tasks per node.

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes



#### Bitonic Merge Steps:


1. Create MPI distribution
- Determine number of processes
- Receive master data
- MPI_init()
- MPI_comm_size()
- MPI_comm_rank()

2. Divide data to processes
- MPI_scatter()
- Each process also gets told a direction to sort

3. All processes sort their initial array 
- Quick sort on local array
- Must know the direction to sort (asc or desc)

4. Process Bitonic merge with neighbor process
- i vs i+k comparison between both arrays
- Recursive bitonic merge call on local array
- Sort local array
- MPI_sendrecv()

5. Gather data from processes
- Local data should be bitonically sorted
- MPI_gather()

6. Finalize process
- MPI_finalize()




#### Sample Sort:


#### Merge Sort:

1. Create MPI distribution
- Determine number of processes
- Receive master data
- get number of processes with MPI_Comm_size()
- get current process rank using MPI_Comm_rank()
- if rank == 0:
-   initalize unsorted array

2. Divide data to processes
- Broadcast number of elements 'n' to all processes with MPI_Bcast()
- Calculate subarray size, n / num_processes
- Scatter the data using MPI_scatter()

3. All processes sort each subarray 
- call merge_sort(subarray, left, right)
- find mid point pivot
- call merge_sort(subarray, left, mid)
- call merge_sort(subarray, mid + 1, right)
- merge all subarrays
- merge(subarray, left, mid, right)
- merge each subarray into halves into a temp array
- copy remaining elements from left half and right half
- copy the merged elements back to the original array

4. Merge across processes
- Initialize step = 1 for representing the distance between neighboring processes involved in merging their sorted arrays
- while (step < num_processes): 
-   if (the current process should receive data):
-       if (neighboring process exists):
-           Receive sorted array from process using MPI_Recv()
-           Merge recerived array with the subarray
-   else
-       Send subarray to process using MPI_Send()
-       break
-   step *= 2

5. Gather merged arrays at the master process
- Use MPI_Gather()
- if rank == 0:
-   print(sorted_array)

6. Finalize processes
- Finalize MPI environment using MPI_Finalize()



#### Radix Sort:

Radix Sort: 


		
1. Initialize MPI 
- Get the total number of processes with MPI_Comm_size() (num_procs) 
- Get the rank of current process with MPI_Comm_rank() (rank)

2. Divide data
- if rank == 0 then 
    - Generate or read the input data 
    - Split the input data into sections for each process 
    - Scatter the sections to all processes using MPI_scatter()
- Else
    - Receive the section of data 

3. Perform radix sort
- function Local_Radix_Sort(chunk): 
    - Determine the maximum number of digits (max_digits) in the data
    - for digit from least significant to most significant do
        - Initialize empty buckets (0 to 9) for current digit
        - for each number in the section do 
          - Find the current digit of the number 
          - Place the number in corresponding bucket 
        - end for 
        - Reassemble the section by concatenating all the buckets in order (0 to 9) 
    end for 
    - return the sorted section
   
4. Data exchange
- for each digit from least significant to most significant:
    - Count the occurrences of each digit in local data (local_histogram) 

- Perform MPI_Alltoall to share local_histograms with all processes (global_histogram) 

- Compute the send_counts and receive_counts based on global_histogram 

- Prepare the data to send based on current digit and send_counts 
- Use MPI_Alltoallv for interprocess exchange (redistribute data)

- Update the local_data with received data

5. Gather sorted data in master process
- if the rank == 0 then 
    - Gather the sorted data sections from all processes and Combine sections into a final sorted array 
- else 
    - Send the sorted section to the root process 

- Finalize MPI

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes will all be powers of 2 (necessary for Bitonic Sort)
- Input types will all be int
- We plan to have Small, medium and large input sizes where we will measure the total execution time, and send/receive times.
- We will utilize different datatypes, with Sorted, Sorted with 1% perturbed, Random and Reverse sorted where we will also measure sorting time and send/receive time to observe how different input types impact performance.
- We will analyze how close the algorithms come to ideal strong scaling and identify bottlenecks (like communication overhead) as the number of processors increases.
- We will evaluate how well the algorithms maintain performance as the problem size grows, looking at any increases in execution time that could be due to communication overhead or memory usage as more processors are added.
