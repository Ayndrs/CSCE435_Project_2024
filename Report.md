# CSCE 435 Group project

## 0. Group number: 22

## 1. Group members:
1. Gohyun Kim
2. Simon Sprouse
3. Michael Nix
4. Austin Karimi

## 2. Project topic (e.g., parallel sorting algorithms)

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort
- Sample Sort
- Merge Sort
- Radix Sort

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

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes will all be powers of 2 (necessary for Bitonic Sort)
- Input types will all be int
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
