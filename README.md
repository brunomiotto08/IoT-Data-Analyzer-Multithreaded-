# IoT Analyzer Multithread

A multithreaded C program to process CSV files containing sensor data from multiple IoT devices. The program efficiently computes monthly statistics (minimum, maximum, average) for each sensor type per device and outputs structured results.

## Requirements

### Windows Development
- **Compiler**: MINGW64
- **Command**: 
  ```bash
  gcc -o programaT main.c -lpthread

./programaT.exe
