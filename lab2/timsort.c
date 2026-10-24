#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "timsort.h"

#define RUN 32

typedef struct
{
    int left;
    int mid;
    int right;
    int type;
} Task;

typedef struct
{
    int *arr;
    Task *tasks;
    int start;
    int end;
} WorkerArgs;

static void insertion_sort(int arr[], int left, int right)
{
    for (int i = left + 1; i <= right; i++)
    {
        int key = arr[i];
        int j = i - 1;

        while (j >= left && arr[j] > key)
        {
            arr[j + 1] = arr[j];
            j--;
        }

        arr[j + 1] = key;
    }
}

static void merge(int arr[], int left, int mid, int right)
{
    int n1 = mid - left + 1;
    int n2 = right - mid;

    int *temp = malloc(sizeof(int) * (n1 + n2));

    if (temp == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    int i = left;
    int j = mid + 1;
    int k = 0;

    while (i <= mid && j <= right)
    {
        if (arr[i] <= arr[j])
            temp[k++] = arr[i++];
        else
            temp[k++] = arr[j++];
    }

    while (i <= mid)
        temp[k++] = arr[i++];

    while (j <= right)
        temp[k++] = arr[j++];

    for (i = 0; i < k; i++)
        arr[left + i] = temp[i];

    free(temp);
}

// Sequential TimSort
void tim_sort(int arr[], int n)
{
    if (n <= 1)
        return;

    for (int left = 0; left < n; left += RUN)
    {
        int right = left + RUN - 1;

        if (right >= n)
            right = n - 1;

        insertion_sort(arr, left, right);
    }

    for (int width = RUN; width < n; width *= 2)
    {
        for (int left = 0; left < n; left += 2 * width)
        {
            int mid = left + width - 1;
            int right = left + 2 * width - 1;

            if (mid >= n)
                mid = n - 1;

            if (right >= n)
                right = n - 1;

            if (mid < right)
                merge(arr, left, mid, right);
        }

        if (width > n / 2)
            break;
    }
}

static void *worker(void *arg)
{
    WorkerArgs *args = (WorkerArgs *)arg;

    for (int i = args->start; i < args->end; i++)
    {
        Task *task = &args->tasks[i];

        if (task->type == 0)
        {
            insertion_sort(
                args->arr,
                task->left,
                task->right);
        }
        else
        {
            merge(
                args->arr,
                task->left,
                task->mid,
                task->right);
        }
    }

    return NULL;
}

static void execute_tasks(
    int arr[],
    Task tasks[],
    int task_count,
    int thread_count)
{
    if (task_count == 0)
        return;

    pthread_t *threads = malloc(sizeof(pthread_t) * thread_count);

    WorkerArgs *args = malloc(sizeof(WorkerArgs) * thread_count);

    if (threads == NULL || args == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        free(threads);
        free(args);
        exit(1);
    }

    int created = 0;

    for (int i = 0; i < thread_count; i++)
    {
        args[i].arr = arr;
        args[i].tasks = tasks;
        args[i].start = task_count * i / thread_count;
        args[i].end = task_count * (i + 1) / thread_count;

        if (pthread_create(&threads[i], NULL, worker, &args[i]) != 0)
        {
            fprintf(stderr, "Failed to create thread\n");

            for (int j = 0; j < created; j++)
                pthread_join(threads[j], NULL);

            free(threads);
            free(args);
            exit(1);
        }

        created++;
    }

    for (int i = 0; i < created; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            fprintf(stderr, "Failed to join thread\n");
            free(threads);
            free(args);
            exit(1);
        }
    }

    free(threads);
    free(args);
}

// Parallel TimSort
void parallel_tim_sort(int arr[], int n, int thread_count)
{
    if (n <= 1)
        return;

    if (thread_count < 1)
        thread_count = 1;

    int max_tasks = (n + RUN - 1) / RUN;

    Task *tasks = malloc(sizeof(Task) * max_tasks);

    if (tasks == NULL)
    {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    // sort initial runs in parallel
    int task_count = 0;

    for (int left = 0; left < n; left += RUN)
    {
        int right = left + RUN - 1;

        if (right >= n)
            right = n - 1;

        tasks[task_count].left = left;
        tasks[task_count].mid = 0;
        tasks[task_count].right = right;
        tasks[task_count].type = 0;

        task_count++;
    }

    execute_tasks(arr, tasks, task_count, thread_count);

    // merge sorted runs in parallel
    for (int width = RUN; width < n; width *= 2)
    {
        task_count = 0;

        for (int left = 0; left < n; left += 2 * width)
        {
            int mid = left + width - 1;
            int right = left + 2 * width - 1;

            if (mid >= n)
                mid = n - 1;

            if (right >= n)
                right = n - 1;

            if (mid < right)
            {
                tasks[task_count].left = left;
                tasks[task_count].mid = mid;
                tasks[task_count].right = right;
                tasks[task_count].type = 1;

                task_count++;
            }
        }

        execute_tasks(arr, tasks, task_count, thread_count);

        if (width > n / 2)
            break;
    }

    free(tasks);
}
