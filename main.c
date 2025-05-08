#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

// Cross-platform way to get CPU count
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#else
#include <unistd.h>
#endif

#define MAX_LINE_LENGTH 1024
#define MAX_DEVICES 100
#define MAX_MONTHS 12
#define NUM_SENSORS 6

typedef struct {
    char device[50];
    int year;
    int month;
    double max[NUM_SENSORS];
    double min[NUM_SENSORS];
    double sum[NUM_SENSORS];
    int count[NUM_SENSORS];
} MonthlyStats;

typedef struct {
    char device[50];
    char date[20];
    double values[NUM_SENSORS];
} SensorRecord;

typedef struct {
    SensorRecord *records;
    int start;
    int end;
    MonthlyStats *results;
    int *result_count;
    pthread_mutex_t *mutex;
} ThreadData;

const char *sensor_names[NUM_SENSORS] = {
    "temperatura",
    "umidade",
    "luminosidade",
    "ruido",
    "eco2",
    "etvoc"
};

int get_cpu_count() {
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
#elif defined(__APPLE__)
    int nm[2];
    size_t len = 4;
    uint32_t count;

    nm[0] = CTL_HW; nm[1] = HW_AVAILCPU;
    sysctl(nm, 2, &count, &len, NULL, 0);

    if (count < 1) {
        nm[1] = HW_NCPU;
        sysctl(nm, 2, &count, &len, NULL, 0);
        if (count < 1) { count = 1; }
    }
    return count;
#else
    return sysconf(_SC_NPROCESSORS_ONLN);
#endif
}

void parse_date(const char *date_str, int *year, int *month) {
    sscanf(date_str, "%d-%d", year, month);
}

void initialize_stats(MonthlyStats *stats, const char *device, int year, int month) {
    strcpy(stats->device, device);
    stats->year = year;
    stats->month = month;
    
    for (int i = 0; i < NUM_SENSORS; i++) {
        stats->max[i] = -INFINITY;
        stats->min[i] = INFINITY;
        stats->sum[i] = 0.0;
        stats->count[i] = 0;
    }
}

int find_or_add_stats(MonthlyStats *results, int *count, const char *device, int year, int month) {
    for (int i = 0; i < *count; i++) {
        if (strcmp(results[i].device, device) == 0 && 
            results[i].year == year && 
            results[i].month == month) {
            return i;
        }
    }
    
    initialize_stats(&results[*count], device, year, month);
    (*count)++;
    return *count - 1;
}

void process_record(MonthlyStats *stats, const SensorRecord *record) {
    for (int i = 0; i < NUM_SENSORS; i++) {
        if (record->values[i] > stats->max[i]) {
            stats->max[i] = record->values[i];
        }
        if (record->values[i] < stats->min[i]) {
            stats->min[i] = record->values[i];
        }
        stats->sum[i] += record->values[i];
        stats->count[i]++;
    }
}

void *process_records(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    
    for (int i = data->start; i < data->end; i++) {
        SensorRecord *record = &data->records[i];
        int year, month;
        parse_date(record->date, &year, &month);
        
        pthread_mutex_lock(data->mutex);
        int stats_index = find_or_add_stats(data->results, data->result_count, 
                                           record->device, year, month);
        pthread_mutex_unlock(data->mutex);
        
        process_record(&data->results[stats_index], record);
    }
    
    return NULL;
}

void write_results_to_csv(const MonthlyStats *results, int count, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Failed to open output file");
        return;
    }
    
    fprintf(file, "device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo\n");
    
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < NUM_SENSORS; j++) {
            if (results[i].count[j] > 0) {
                double avg = results[i].sum[j] / results[i].count[j];
                fprintf(file, "%s;%04d-%02d;%s;%.2f;%.2f;%.2f\n",
                        results[i].device,
                        results[i].year,
                        results[i].month,
                        sensor_names[j],
                        results[i].max[j],
                        avg,
                        results[i].min[j]);
            }
        }
    }
    
    fclose(file);
}

int read_csv(const char *filename, SensorRecord **records, int *record_count) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open input file");
        return 0;
    }
    
    char line[MAX_LINE_LENGTH];
    *record_count = 0;
    
    // Count lines (excluding header)
    while (fgets(line, sizeof(line), file)) {
        (*record_count)++;
    }
    (*record_count)--; // Subtract header
    
    // Allocate memory
    *records = (SensorRecord *)malloc(*record_count * sizeof(SensorRecord));
    if (!*records) {
        perror("Memory allocation failed");
        fclose(file);
        return 0;
    }
    
    // Rewind and read data
    rewind(file);
    fgets(line, sizeof(line), file); // Skip header
    
    int index = 0;
    while (fgets(line, sizeof(line), file) && index < *record_count) {
        char *token = strtok(line, "|");
        int field = 0;
        
        while (token != NULL && field < 12) {
            switch (field) {
                case 1: // device
                    strncpy((*records)[index].device, token, sizeof((*records)[index].device) - 1);
                    break;
                case 3: // date
                    strncpy((*records)[index].date, token, 10); // Only get YYYY-MM-DD part
                    (*records)[index].date[10] = '\0';
                    break;
                case 4: // temperatura
                    (*records)[index].values[0] = atof(token);
                    break;
                case 5: // umidade
                    (*records)[index].values[1] = atof(token);
                    break;
                case 6: // luminosidade
                    (*records)[index].values[2] = atof(token);
                    break;
                case 7: // ruido
                    (*records)[index].values[3] = atof(token);
                    break;
                case 8: // eco2
                    (*records)[index].values[4] = atof(token);
                    break;
                case 9: // etvoc
                    (*records)[index].values[5] = atof(token);
                    break;
            }
            
            token = strtok(NULL, "|");
            field++;
        }
        
        // Only process records from March 2024 onwards
        int year, month;
        parse_date((*records)[index].date, &year, &month);
        
        if (year > 2024 || (year == 2024 && month >= 3)) {
            index++;
        } else {
            (*record_count)--;
        }
    }
    
    fclose(file);
    return 1;
}

int main() {
    const char *input_filename = "devices.csv";
    const char *output_filename = "sensor_stats.csv";
    
    SensorRecord *records = NULL;
    int record_count = 0;
    
    if (!read_csv(input_filename, &records, &record_count)) {
        return 1;
    }
    
    if (record_count == 0) {
        printf("No records found after March 2024.\n");
        free(records);
        return 0;
    }
    
    // Determine number of threads based on available processors
    int num_threads = get_cpu_count();
    if (num_threads > record_count) {
        num_threads = record_count;
    }
    
    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    ThreadData *thread_data = (ThreadData *)malloc(num_threads * sizeof(ThreadData));
    MonthlyStats *results = (MonthlyStats *)malloc(record_count * sizeof(MonthlyStats));
    int result_count = 0;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    
    // Calculate records per thread
    int records_per_thread = record_count / num_threads;
    int remaining_records = record_count % num_threads;
    int start = 0;
    
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].records = records;
        thread_data[i].start = start;
        thread_data[i].end = start + records_per_thread + (i < remaining_records ? 1 : 0);
        thread_data[i].results = results;
        thread_data[i].result_count = &result_count;
        thread_data[i].mutex = &mutex;
        
        start = thread_data[i].end;
        
        pthread_create(&threads[i], NULL, process_records, &thread_data[i]);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Write results to CSV
    write_results_to_csv(results, result_count, output_filename);
    printf("Results written to %s\n", output_filename);
    
    // Clean up
    free(records);
    free(threads);
    free(thread_data);
    free(results);
    pthread_mutex_destroy(&mutex);
    
    return 0;
}