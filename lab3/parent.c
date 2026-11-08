#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <signal.h>
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

void int_to_string(int number, char *str)
{
    char temp[20];
    int len = 0;

    if (number == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    while (number > 0)
    {
        temp[len++] = '0' + number % 10;
        number /= 10;
    }

    for (int i = 0; i < len; i++)
        str[i] = temp[len - i - 1];

    str[len] = '\0';
}

void make_name(char *name, const char *prefix, int pid)
{
    char number[20];

    int_to_string(pid, number);

    int i = 0;
    int j = 0;

    while (prefix[i] != '\0')
        name[j++] = prefix[i++];

    i = 0;

    while (number[i] != '\0')
        name[j++] = number[i++];

    name[j] = '\0';
}

int read_line(char *buffer, int size)
{
    int pos = 0;
    char c;

    while (1)
    {
        ssize_t n = read(STDIN_FILENO, &c, 1);

        if (n == -1)
            return -1;

        if (n == 0)
        {
            if (pos == 0)
                return 0;

            break;
        }

        if (c == '\n')
            break;

        if (pos >= size - 1)
        {
            while (c != '\n')
            {
                n = read(STDIN_FILENO, &c, 1);

                if (n <= 0 || c == '\n')
                    break;
            }

            return -2;
        }

        buffer[pos++] = c;
    }

    buffer[pos] = '\0';

    return 1;
}

int wait_semaphore(sem_t *sem)
{
    while (sem_wait(sem) == -1)
    {
        if (errno != EINTR)
            return -1;
    }

    return 0;
}

void cleanup(
    SharedData *shared,
    sem_t *sem_parent,
    sem_t *sem_child,
    const char *shm_name,
    const char *sem_parent_name,
    const char *sem_child_name)
{
    sem_close(sem_parent);
    sem_close(sem_child);

    sem_unlink(sem_parent_name);
    sem_unlink(sem_child_name);

    munmap(shared, sizeof(SharedData));

    shm_unlink(shm_name);
}

int main()
{
    char filename[256];
    char command[COMMAND_SIZE];

    const char filename_prompt[] = "Введите имя выходного файла: ";
    const char command_prompt[] = "Введите числа (пустая строка для завершения): ";
    const char success_message[] = "Сумма записана в файл\n";
    const char input_error[] = "Слишком длинная команда\n";

    write(STDOUT_FILENO, filename_prompt, sizeof(filename_prompt) - 1);

    if (read_line(filename, sizeof(filename)) != 1)
        return 1;

    if (filename[0] == '\0')
        return 1;

    char shm_name[64];
    char sem_parent_name[64];
    char sem_child_name[64];

    int pid_number = getpid();

    make_name(shm_name, "/my_shm_", pid_number);
    make_name(sem_parent_name, "/sem_parent_", pid_number);
    make_name(sem_child_name, "/sem_child_", pid_number);

    int shm_fd = shm_open(
        shm_name,
        O_CREAT | O_EXCL | O_RDWR,
        0600);

    if (shm_fd == -1)
        return 1;

    if (ftruncate(shm_fd, sizeof(SharedData)) == -1)
    {
        close(shm_fd);
        shm_unlink(shm_name);
        return 1;
    }

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
        shm_unlink(shm_name);
        return 1;
    }

    close(shm_fd);

    memset(shared, 0, sizeof(SharedData));

    sem_t *sem_parent = sem_open(
        sem_parent_name,
        O_CREAT | O_EXCL,
        0600,
        0);

    if (sem_parent == SEM_FAILED)
    {
        munmap(shared, sizeof(SharedData));
        shm_unlink(shm_name);
        return 1;
    }

    sem_t *sem_child = sem_open(
        sem_child_name,
        O_CREAT | O_EXCL,
        0600,
        0);

    if (sem_child == SEM_FAILED)
    {
        sem_close(sem_parent);
        sem_unlink(sem_parent_name);
        munmap(shared, sizeof(SharedData));
        shm_unlink(shm_name);
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1)
    {
        cleanup(
            shared,
            sem_parent,
            sem_child,
            shm_name,
            sem_parent_name,
            sem_child_name);

        return 1;
    }

    if (pid == 0)
    {
        execl(
            "./child.o",
            "child",
            filename,
            shm_name,
            sem_child_name,
            sem_parent_name,
            NULL);

        shared->status = -1;
        sem_post(sem_parent);

        _exit(1);
    }

    if (wait_semaphore(sem_parent) == -1)
    {
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);

        cleanup(
            shared,
            sem_parent,
            sem_child,
            shm_name,
            sem_parent_name,
            sem_child_name);

        return 1;
    }

    if (shared->status == -1)
    {
        waitpid(pid, NULL, 0);

        cleanup(
            shared,
            sem_parent,
            sem_child,
            shm_name,
            sem_parent_name,
            sem_child_name);

        return 1;
    }

    while (1)
    {
        write(STDOUT_FILENO, command_prompt, sizeof(command_prompt) - 1);

        int result = read_line(command, sizeof(command));

        if (result == -1)
        {
            shared->terminate = 1;
            sem_post(sem_child);
            break;
        }

        if (result == -2)
        {
            write(STDOUT_FILENO, input_error, sizeof(input_error) - 1);
            continue;
        }

        if (result == 0 || command[0] == '\0')
        {
            shared->terminate = 1;
            sem_post(sem_child);
            break;
        }

        strcpy(shared->command, command);
        shared->terminate = 0;
        shared->status = 0;

        if (sem_post(sem_child) == -1)
        {
            kill(pid, SIGTERM);
            break;
        }

        if (wait_semaphore(sem_parent) == -1)
        {
            kill(pid, SIGTERM);
            break;
        }

        if (shared->status == -1)
            break;

        write(
            STDOUT_FILENO,
            success_message,
            sizeof(success_message) - 1);
    }

    while (waitpid(pid, NULL, 0) == -1)
    {
        if (errno != EINTR)
            break;
    }

    cleanup(
        shared,
        sem_parent,
        sem_child,
        shm_name,
        sem_parent_name,
        sem_child_name);

    return 0;
}
