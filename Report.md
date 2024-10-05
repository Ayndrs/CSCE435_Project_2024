# CSCE 435 Group project

## 0. Group number: 22

## 1. Group members:
1. Gohyun Kim
2. Simon Sprouse
3. Michael Nix
4. Austin Karimi

## 2. Project topic (e.g., parallel sorting algorithms)

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)



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
- MPI_sendrecv

5. Gather data from processes
- Local data should be bitonically sorted
- MPI_gather()

6. Finalize process
- MPI_finalize()




#### Sample Sort:
#### Merge Sort:
#### Radix Sort:

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes

### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
