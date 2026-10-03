#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    if (argc < 3)
        return 1;

    // argv[1] — file name.
    // argv[2] — descriptor pipe2.

    char *filename = argv[1];

    int pipe2_fd = atoi(argv[2]);

    int file = open(
        filename,
        O_WRONLY | O_CREAT | O_TRUNC,
        0644);

    if (file == -1)
        return 1;

    char buffer[4096];

    while (1)
    {
        int size = read(STDIN_FILENO, buffer, sizeof(buffer));

        if (size <= 0)
            break;

        int end = -1;

        for (int i = 0; i < size; i++)
        {
            if (buffer[i] == '\n')
            {
                end = i;
                break;
            }
        }

        if (end == 0)
            break;

        char line[4096];

        int line_size = end;

        if (line_size >= sizeof(line))
            line_size = sizeof(line) - 1;

        for (int i = 0; i < line_size; i++)
            line[i] = buffer[i];

        line[line_size] = '\0';

        char *ptr = line;

        double sum = 0.0f;

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

        int negative = 0;

        if (sum < 0)
        {
            negative = 1;
            sum = -sum;
        }

        int integer_part = (int)sum;
        int fraction = (int)((sum - integer_part) * 1000000.0f);

        char result[128];
        int r = 0;

        if (negative)
            result[r++] = '-';

        char digits[32];
        int d = 0;

        if (integer_part == 0)
        {
            digits[d++] = '0';
        }
        else
        {
            while (integer_part > 0)
            {
                digits[d++] = '0' + integer_part % 10;
                integer_part /= 10;
            }
        }

        for (int i = d - 1; i >= 0; i--)
            result[r++] = digits[i];

        result[r++] = '.';

        int divisor = 100000;

        for (int i = 0; i < 6; i++)
        {
            result[r++] = '0' + fraction / divisor % 10;
            divisor /= 10;

            if (divisor == 0)
                divisor = 1;
        }

        result[r++] = '\n';

        write(file, result, r);

        char message[] = "Сумма записана в файл\n";

        write(pipe2_fd, message, strlen(message));
    }

    close(file);
    close(pipe2_fd);

    return 0;
}
