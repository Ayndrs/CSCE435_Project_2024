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
- Sample Sort - Michael Nix
- Merge Sort - Gohyun Kim
- Radix Sort - Austin Karimi

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
Note:
- The fastest sorting algorithm should be used for the sequential internal sorting. Quick Sort is the most common algorithm for this, but Insertion Sort would be faster for smaller arrays. e.g. When the number of processors is large enough to make it reasonable
- Despite this, I will only include Quick Sort in the pseudo-code due to the testing required to find the cut-off required to make Insertion Sort useful 

parameters: 
- Unsorted array (arr)
- number of processors (p)
1. Initialize MPI environment
- MPI_Init()
- MPI_Comm_rank() to get rank of the process
- MPI_Comm_size() to get the number of processes

2. Split the array among processes
- num subarrays = number of processes
- get each local subarray's size (using MPI_Comm_size)
- allocate memory for each local subarray
- MPI_Scatter(...)

3. Sort the local arrays 
- QuickSort(subarray) //see below

4. Select local samples
- From each subarray, place p-1 values into a sample-array 
    - each value is evenly spaced in the parent sub-array

5. Gather the local samples
- if rank == 0: allocate space for all the samples
- MPI_Gather(local samples)

6. Sort the collected samples (master process)
- sorted samples = QuickSort(gathered samples)

7. Create splitter array (master process)
- splitters = []
- for i in [1..p-1]: splitters += sorted samples[i*p]

8. Broadcast splitters
- MPI_Bcast(splitters)

9. Split local arrays into buckets (based on splitters)
- buckets = 1xp array
- for each item in local array:
    find the bucket for the item
    append the item to buckets array

10. Send and receive the buckets to the corresponding processes
- MPI_Alltoall(...) to share the number of sends and receiving values
- MPI_Alltoallv(...) to share the values

11. Sort received buckets locally
- QuickSort(received buckets)

12. Gather sorted subarrays at master
- MPI_Gather(local sorted arrays)
- MPI_Finalize()

13. Return (master)
- if rank == 0 return sorted

QuickSort(arr): //sequential quicksort
- Choose a pivot
- Split arr into 3 partitions
    - a = values less than the pivot
    - b = the pivot
    - c = values greater than the pivot
- prev = QuickSort(a)
- next = QuickSort(c)
- return a + b + c



#### Merge Sort:

1. Create MPI distribution
- Determine number of processes
- Receive master data
- get number of processes with MPI_Comm_size()
- get current process rank using MPI_Comm_rank()
- if rank == 0:
-   initalize unsorted array

2. Divide data to processes
- Calculate subarray size, n / num_processes
- have each worker compute the subarray using MPI_Scatter()

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

5. Combine merged arrays at the master process
- MPI_Gather() to receive all subarrays
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


### 3a. Caliper instrumentation
Please use the caliper build `/scratch/group/csce435-f24/Caliper/caliper/share/cmake/caliper` 
(same as lab2 build.sh) to collect caliper files for each experiment you run.

Your Caliper annotations should result in the following calltree
(use `Thicket.tree()` to see the calltree):
```
main
|_ data_init_X      # X = runtime OR io
|_ comm
|    |_ comm_small
|    |_ comm_large
|_ comp
|    |_ comp_small
|    |_ comp_large
|_ correctness_check
```

Required region annotations:
- `main` - top-level main function.
    - `data_init_X` - the function where input data is generated or read in from file. Use *data_init_runtime* if you are generating the data during the program, and *data_init_io* if you are reading the data from a file.
    - `correctness_check` - function for checking the correctness of the algorithm output (e.g., checking if the resulting data is sorted).
    - `comm` - All communication-related functions in your algorithm should be nested under the `comm` region.
      - Inside the `comm` region, you should create regions to indicate how much data you are communicating (i.e., `comm_small` if you are sending or broadcasting a few values, `comm_large` if you are sending all of your local values).
      - Notice that auxillary functions like MPI_init are not under here.
    - `comp` - All computation functions within your algorithm should be nested under the `comp` region.
      - Inside the `comp` region, you should create regions to indicate how much data you are computing on (i.e., `comp_small` if you are sorting a few values like the splitters, `comp_large` if you are sorting values in the array).
      - Notice that auxillary functions like data_init are not under here.
    - `MPI_X` - You will also see MPI regions in the calltree if using the appropriate MPI profiling configuration (see **Builds/**). Examples shown below.

All functions will be called from `main` and most will be grouped under either `comm` or `comp` regions, representing communication and computation, respectively. You should be timing as many significant functions in your code as possible. **Do not** time print statements or other insignificant operations that may skew the performance measurements.

### **Nesting Code Regions Example** - all computation code regions should be nested in the "comp" parent code region as following:
```
CALI_MARK_BEGIN("comp");
CALI_MARK_BEGIN("comp_small");
sort_pivots(pivot_arr);
CALI_MARK_END("comp_small");
CALI_MARK_END("comp");

# Other non-computation code
...

CALI_MARK_BEGIN("comp");
CALI_MARK_BEGIN("comp_large");
sort_values(arr);
CALI_MARK_END("comp_large");
CALI_MARK_END("comp");
```

### **Calltree Example**:
```
# MPI Mergesort
4.695 main
├─ 0.001 MPI_Comm_dup
├─ 0.000 MPI_Finalize
├─ 0.000 MPI_Finalized
├─ 0.000 MPI_Init
├─ 0.000 MPI_Initialized
├─ 2.599 comm
│  ├─ 2.572 MPI_Barrier
│  └─ 0.027 comm_large
│     ├─ 0.011 MPI_Gather
│     └─ 0.016 MPI_Scatter
├─ 0.910 comp
│  └─ 0.909 comp_large
├─ 0.201 data_init_runtime
└─ 0.440 correctness_check
```

### 3b. Collect Metadata

Have the following code in your programs to collect metadata:
```
adiak::init(NULL);
adiak::launchdate();    // launch date of the job
adiak::libraries();     // Libraries used
adiak::cmdline();       // Command line used to launch the job
adiak::clustername();   // Name of the cluster
adiak::value("algorithm", algorithm); // The name of the algorithm you are using (e.g., "merge", "bitonic")
adiak::value("programming_model", programming_model); // e.g. "mpi"
adiak::value("data_type", data_type); // The datatype of input elements (e.g., double, int, float)
adiak::value("size_of_data_type", size_of_data_type); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
adiak::value("input_size", input_size); // The number of elements in input dataset (1000)
adiak::value("input_type", input_type); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "1_perc_perturbed")
adiak::value("num_procs", num_procs); // The number of processors (MPI ranks)
adiak::value("scalability", scalability); // The scalability of your algorithm. choices: ("strong", "weak")
adiak::value("group_num", group_number); // The number of your group (integer, e.g., 1, 10)
adiak::value("implementation_source", implementation_source); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").
```

They will show up in the `Thicket.metadata` if the caliper file is read into Thicket.

### **See the `Builds/` directory to find the correct Caliper configurations to get the performance metrics.** They will show up in the `Thicket.dataframe` when the Caliper file is read into Thicket.
## 4. Performance evaluation

Include detailed analysis of computation performance, communication performance. 
Include figures and explanation of your analysis.

### 4a. Vary the following parameters
For input_size's:
- 2^16, 2^18, 2^20, 2^22, 2^24, 2^26, 2^28

For input_type's:
- Sorted, Random, Reverse sorted, 1%perturbed

MPI: num_procs:
- 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024

This should result in 4x7x10=280 Caliper files for your MPI experiments.

### 4b. Hints for performance analysis

To automate running a set of experiments, parameterize your program.

- input_type: "Sorted" could generate a sorted input to pass into your algorithms
- algorithm: You can have a switch statement that calls the different algorithms and sets the Adiak variables accordingly
- num_procs: How many MPI ranks you are using

When your program works with these parameters, you can write a shell script 
that will run a for loop over the parameters above (e.g., on 64 processors, 
perform runs that invoke algorithm2 for Sorted, ReverseSorted, and Random data).  

### 4c. You should measure the following performance metrics
- `Time`
    - Min time/rank
    - Max time/rank
    - Avg time/rank
    - Total time
    - Variance time/rank


## 5. Presentation
Plots for the presentation should be as follows:
- For each implementation:
    - For each of comp_large, comm, and main:
        - Strong scaling plots for each input_size with lines for input_type (7 plots - 4 lines each)
        - Strong scaling speedup plot for each input_type (4 plots)
        - Weak scaling plots for each input_type (4 plots)

Analyze these plots and choose a subset to present and explain in your presentation.

## 6. Final Report
Submit a zip named `TeamX.zip` where `X` is your team number. The zip should contain the following files:
- Algorithms: Directory of source code of your algorithms.
- Data: All `.cali` files used to generate the plots seperated by algorithm/implementation.
- Jupyter notebook: The Jupyter notebook(s) used to generate the plots for the report.
- Report.md