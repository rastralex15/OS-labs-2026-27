#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <errno.h>

#define COMMAND_SIZE 4000
#define MESSAGE_SIZE 256

typedef struct
{
    int terminate;
    int status;
    char command[COMMAND_SIZE];
    char message[MESSAGE_SIZE];
} SharedData;

int wait_semaphore(sem_t *sem)
{
    while (sem_wait(sem) == -1)
    {
        if (errno != EINTR)
            return -1;
    }

    return 0;
}

int write_all(int fd, const char *buffer, size_t size)
{
    size_t written = 0;

    while (written < size)
    {
        ssize_t n = write(fd, buffer + written, size - written);

        if (n == -1)
        {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (n == 0)
            return -1;

        written += n;
    }

    return 0;
}

int integer_to_string(long long number, char *buffer)
{
    char temp[32];
    int len = 0;

    if (number == 0)
    {
        buffer[0] = '0';
        return 1;
    }

    while (number > 0)
    {
        temp[len++] = '0' + number % 10;
        number /= 10;
    }

    for (int i = 0; i < len; i++)
        buffer[i] = temp[len - i - 1];

    return len;
}

int format_sum(double sum, char *result)
{
    if (sum > 9000000000000.0 || sum < -9000000000000.0)
        return -1;

    int pos = 0;

    if (sum < 0)
    {
        result[pos++] = '-';
        sum = -sum;
    }

    long long scaled = (long long)(sum * 1000000.0 + 0.5);

    long long integer_part = scaled / 1000000;
    int fraction = scaled % 1000000;

    pos += integer_to_string(integer_part, result + pos);

    result[pos++] = '.';

    int divisor = 100000;

    for (int i = 0; i < 6; i++)
    {
        result[pos++] = '0' + fraction / divisor % 10;
        divisor /= 10;
    }

    result[pos++] = '\n';

    return pos;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
        return 1;

    char *filename = argv[1];

    int shm_fd = shm_open(argv[2], O_RDWR, 0600);

    if (shm_fd == -1)
        return 1;

    SharedData *shared = mmap(
        NULL,
        sizeof(SharedData),
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        shm_fd,
        0);

    if (shared == MAP_FAILED)
    {
        close(shm_fd);
        return 1;
    }

    close(shm_fd);

    sem_t *sem_child = sem_open(argv[3], 0);

    if (sem_child == SEM_FAILED)
    {
        munmap(shared, sizeof(SharedData));
        return 1;
    }

    sem_t *sem_parent = sem_open(argv[4], 0);

    if (sem_parent == SEM_FAILED)
    {
        sem_close(sem_child);
        munmap(shared, sizeof(SharedData));
        return 1;
    }

    int file = open(
        filename,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644);

    if (file == -1)
    {
        shared->status = -1;
        sem_post(sem_parent);

        sem_close(sem_child);
        sem_close(sem_parent);
        munmap(shared, sizeof(SharedData));

        return 1;
    }

    shared->status = 0;

    if (sem_post(sem_parent) == -1)
    {
        close(file);
        sem_close(sem_child);
        sem_close(sem_parent);
        munmap(shared, sizeof(SharedData));

        return 1;
    }

    while (1)
    {
        if (wait_semaphore(sem_child) == -1)
            break;

        if (shared->terminate)
            break;

        char *ptr = shared->command;

        double sum = 0.0;

        while (*ptr != '\0')
        {
            char *next;

            float number = strtof(ptr, &next);

            if (next != ptr)
            {
                sum += number;
                ptr = next;
            }
            else
            {
                ptr++;
            }
        }

        char result[128];

        int len = format_sum(sum, result);

        if (len == -1)
        {
            shared->status = -1;
            sem_post(sem_parent);
            break;
        }

        if (write_all(file, result, len) == -1)
        {
            shared->status = -1;
            sem_post(sem_parent);
            break;
        }

        shared->status = 0;

        if (sem_post(sem_parent) == -1)
            break;
    }

    close(file);

    sem_close(sem_child);
    sem_close(sem_parent);

    munmap(shared, sizeof(SharedData));

    return 0;
}
