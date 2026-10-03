#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char filename[256];
    char command[4096];

    int pipe1[2]; // parent -> child
    int pipe2[2]; // child -> parent

    int pos = 0;
    char c;

    while (pos < 255)
    {
        if (read(STDIN_FILENO, &c, 1) <= 0)
            return 1;

        if (c == '\n')
            break;

        filename[pos++] = c;
    }

    filename[pos] = '\0';

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1)
        return 1;

    pid_t pid = fork();

    if (pid == -1)
        return 1;

    if (pid == 0) // child process
    {
        close(pipe1[1]);
        close(pipe2[0]);

        if (dup2(pipe1[0], STDIN_FILENO) == -1)
            _exit(1);

        close(pipe1[0]);

        char pipe2_fd[20];

        int len = 0;
        int fd = pipe2[1];

        char temp[20];

        if (fd == 0)
        {
            temp[len++] = '0';
        }
        else
        {
            while (fd > 0)
            {
                temp[len++] = '0' + fd % 10;
                fd /= 10;
            }
        }

        for (int i = 0; i < len; i++)
            pipe2_fd[i] = temp[len - i - 1];

        pipe2_fd[len] = '\0';

        execl("./child", "child", filename, pipe2_fd, NULL);

        _exit(1);
    }
    else // parent process
    {
        close(pipe1[0]);
        close(pipe2[1]);

        while (1)
        {
            pos = 0;

            while (pos < 4095)
            {
                if (read(STDIN_FILENO, &c, 1) <= 0)
                {
                    close(pipe1[1]);
                    close(pipe2[0]);
                    waitpid(pid, NULL, 0);
                    return 0;
                }

                command[pos++] = c;

                if (c == '\n')
                    break;
            }

            int sent = 0;

            while (sent < pos)
            {
                int n = write(pipe1[1], command + sent, pos - sent);

                if (n <= 0)
                {
                    close(pipe1[1]);
                    close(pipe2[0]);
                    waitpid(pid, NULL, 0);
                    return 0;
                }
                sent += n;
            }

            char response[256];

            int n = read(pipe2[0], response, sizeof(response) - 1);

            if (n > 0)
            {
                response[n] = '\0';

                write(STDOUT_FILENO, response, n);
            }

            if (pos == 1 && command[0] == '\n')
                break;
        }

        close(pipe1[1]);
        close(pipe2[0]);
        waitpid(pid, NULL, 0);

        return 0;
    }
}
