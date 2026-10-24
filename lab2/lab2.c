#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "timsort.h"

double get_time()
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        perror("clock_gettime");
        exit(1);
    }

    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

int *read_array(const char *filename, int *size)
{
    FILE *file = fopen(filename, "r");

    if (file == NULL)
    {
        perror("Cannot open input file");
        return NULL;
    }

    int capacity = 1024;
    int count = 0;

    int *arr = malloc(sizeof(int) * capacity);

    if (arr == NULL)
    {
        perror("malloc");
        fclose(file);
        return NULL;
    }

    int num;
    int result;

    while ((result = fscanf(file, "%d", &num)) == 1)
    {
        if (count == capacity)
        {
            if (capacity > 2147483647 / 2)
            {
                fprintf(stderr, "Input file is too large\n");
                free(arr);
                fclose(file);
                return NULL;
            }

            capacity *= 2;

            int *temp = realloc(arr, sizeof(int) * (size_t)capacity);

            if (temp == NULL)
            {
                perror("realloc");
                free(arr);
                fclose(file);
                return NULL;
            }

            arr = temp;
        }

        arr[count++] = num;
    }

    if (result != EOF)
    {
        fprintf(stderr, "Invalid data in input file\n");
        free(arr);
        fclose(file);
        return NULL;
    }

    if (ferror(file))
    {
        perror("Error reading input file");
        free(arr);
        fclose(file);
        return NULL;
    }

    fclose(file);

    *size = count;
    return arr;
}

int is_sorted(const int arr[], int n)
{
    for (int i = 1; i < n; i++)
    {
        if (arr[i - 1] > arr[i])
            return 0;
    }

    return 1;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    int n = 0;
    int *original = read_array(argv[1], &n);

    if (original == NULL)
        return 1;

    int *arr = malloc(sizeof(int) * (n > 0 ? (size_t)n : 1));

    if (arr == NULL)
    {
        perror("malloc");
        free(original);
        return 1;
    }

    printf("Input file: %s\n", argv[1]);
    printf("Array size: %d\n\n", n);

    /* Sequential version. */
    memcpy(arr, original, sizeof(int) * (size_t)n);

    double start = get_time();
    tim_sort(arr, n);
    double sequential_time = get_time() - start;

    if (!is_sorted(arr, n))
    {
        fprintf(stderr, "Sequential sort failed!\n");
        free(arr);
        free(original);
        return 1;
    }

    printf("Sequential TimSort: %.3f ms\n\n", sequential_time * 1000.0);

    /* Thread counts for the experiment. */
    const int thread_counts[] = {
        1, 2, 4, 8, 16, 22, 32, 128, 1024};

    const int count = (int)(sizeof(thread_counts) / sizeof(thread_counts[0]));

    printf(
        "%-10s %-18s %-15s %-15s\n",
        "Threads",
        "Time (ms)",
        "Speedup",
        "Efficiency");

    printf("------------------------------------------------------------\n");

    for (int i = 0; i < count; i++)
    {
        int threads = thread_counts[i];

        /* Restore the original unsorted input. */
        memcpy(arr, original, sizeof(int) * (size_t)n);

        start = get_time();
        parallel_tim_sort(arr, n, threads);
        double parallel_time = get_time() - start;

        if (!is_sorted(arr, n))
        {
            fprintf(
                stderr,
                "Parallel sort failed with %d threads!\n",
                threads);

            free(arr);
            free(original);
            return 1;
        }

        double speedup = 0.0;
        double efficiency = 0.0;

        if (parallel_time > 0.0)
            speedup = sequential_time / parallel_time;

        efficiency = speedup / threads;

        printf(
            "%-10d %-18.3f %-15.4f %-15.4f\n",
            threads,
            parallel_time * 1000.0,
            speedup,
            efficiency);
    }

    free(arr);
    free(original);

    return 0;
}
