# IoT Analyzer Multithread

A multithreaded C program to process CSV files containing sensor data from multiple IoT devices. The program efficiently computes monthly statistics (minimum, maximum, average) for each sensor type per device and outputs structured results.

## Requirements

### Windows Development
- **Compiler**: MINGW64
- **Command**: 
  ```bash
  gcc -o programaT main.c -lpthread

- **Execute**: 
  ```bash
  ./programaT.exe

**Note:** In VSCode on Windows, the editor may flag an error regarding pthreads inclusion, but this doesn't affect compilation when using MINGW64 as we did.

### For Linux Systems
- **Compiler**: GCC
- **Command**: 
  ```bash
  gcc -o programa main.c -lpthread -lm
  ```

- **Execute**: 
  ```bash
  ./programa
  ```


## Installation
- Install MINGW64 (Windows) or GCC (Linux)
- Compile with the appropriate command above
- Ensure devices.csv is in the same directory

## Usage
### Windows
```bash
  gcc -o programaT main.c -lpthread
  ./programaT.exe
```

### Linux
```bash
gcc -o programa main.c -lpthread -lm
./programa
```
- The output file sensor_stats.csv will be generated in the same directory.

## Input CSV Format
- The CSV must contain 12 pipe-separated columns:
```bash
id|device|contagem|data|temperatura|umidade|luminosidade|ruido|eco2|etvoc|latitude|longitude
```

## Architecture

### Thread Implementation
```bash
#include <pthread.h> 
```

### CSV Parsing
The program reads the entire CSV file into memory, then processes records from March 2024 onwards. Each record is parsed into a SensorRecord structure:

```bash
typedef struct {
    char device[50];          // Device identifier
    char date[20];            // Date in YYYY-MM-DD format
    double values[NUM_SENSORS]; // Sensor readings [temp, humidity, etc.]
} SensorRecord;
```

## Thread Distribution

**Parallel Processing Strategy**
- Static Partitioning:
  - Automatically detects available CPU cores using system calls

  - Number of threads = CPU core count

  - Records divided into contiguous blocks

  - Creates one worker thread per available core
   
  - Evenly distributes records among threads (with remainder records distributed to first threads)
   
  - Example with 4 cores and 1000 records:
    - Thread 1: 0-250
    - Thread 2: 251-500
    - Thread 3: 501-750
    - Thread 4: 751-999
  
- Load Balancing:
  - Excess records distributed to initial threads
  - Each thread exclusively processes its assigned block
 

## Thread Data Processing
**Per-Thread Execution Flow**
Each thread performs these operations on its records:

### **Metadata Extraction:**

```bash
parse_date(record->date, &year, &month);
```

### **Statistical Analysis:**

```bash

// Update maximum
if (val > stats->max[sensor_idx]) 
    stats->max[sensor_idx] = val;

// Update minimum  
if (val < stats->min[sensor_idx])
    stats->min[sensor_idx] = val;

// Accumulate for average
stats->sum[sensor_idx] += val;
stats->count[sensor_idx]++;
```

### **Synchronization:**

```bash    
pthread_mutex_lock(&mutex);
// Critical section: shared stats update
pthread_mutex_unlock(&mutex);
```
#### Thread Synchronization
- Mutex-protected operations:
  - Finding/adding monthly stats entries
  - Updating result counters

- Lock-free operations:
  - Processing individual records
  - Calculating min/max/sum for assigned records



## Concurrency Considerations
#### Potential Issues

- Race Conditions:
  - Risk during shared stats updates
  - Mitigated by mutex-protected critical sections

- Memory Contention:
  - Cache thrashing possible with many threads
  - Minimized through block partitioning

- Load Imbalance:
  - Possible if device records aren't uniformly distributed
  - Addressed via static workload division

- I/O Bottleneck:
  - Initial CSV loading is single-threaded
  - Output generation is serialized
  

## Data Analysis
Each thread processes its assigned records and updates shared statistics stored in MonthlyStats structures:

```bash
typedef struct {
    char device[50];         // Device name
    int year, month;         // Time period
    double max[NUM_SENSORS]; // Maximum values
    double min[NUM_SENSORS]; // Minimum values
    double sum[NUM_SENSORS]; // Sums for average calculation
    int count[NUM_SENSORS];  // Reading counts
} MonthlyStats;
```


## Output Generation

- The program generates a CSV file with the following format:

```bash
device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo
```

- Example output line:
  
```bash
sirrosteste_UCS_AMV-10;2024-07;temperatura;28.50;24.75;21.00
device_A;2024-03;temperature;28.50;24.75;21.00
device_B;2024-03;humidity;98.20;85.30;75.40
```

## Thread Execution Mode
#### Kernel Interaction
  - User-Level Threads: Managed by pthread library
  - Kernel Awareness: Scheduled by OS kernel
  - Concurrency: True parallel execution on multi-core systems


## Technical Details
- Cross-Platform Development
### Originally developed on Windows with:
- Source file: main.c
- Input file: devices.csv
- Output file: sensor_stats.csv
- Used MINGW64 as solution for pthreads compatibility on Windows.


## Performance Considerations
- Concurrency Model
  
  - **Thread Management:** POSIX threads (pthreads) running in user space
  
  - **Work Distribution:** Static partitioning of input records
  
  - **Memory Access:** Each thread primarily works on its own data segment

## Potential Issues
- **Memory Contention:**
  - High thread counts may contend for access to shared statistics
  - Mitigated by minimizing critical sections

- **Load Imbalance:**
  - Some devices/months may have more records than others
  - Static partitioning assumes uniform distribution

- **Scalability Limits:**
  - Memory bandwidth may become bottleneck with many cores
  - Disk I/O is single-threaded during initial load

## Autors
  - [@brunomiotto08](https://github.com/brunomiotto08/)
