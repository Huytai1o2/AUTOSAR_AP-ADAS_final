#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

#define UART_DEV  "/dev/ttyAMA0" // UART0
#define BAUD_RATE B115200
#define BUF_SIZE  256

int main(void)
{
    int fd = open(UART_DEV, O_RDONLY | O_NOCTTY);
    if (fd < 0) {
        perror("open " UART_DEV);
        return EXIT_FAILURE;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        close(fd);
        return EXIT_FAILURE;
    }

    cfmakeraw(&tty);
    cfsetispeed(&tty, BAUD_RATE);
    tty.c_cc[VMIN]  = 1;   /* block until at least 1 byte arrives */
    tty.c_cc[VTIME] = 0;   /* no timeout */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Reading GPS NMEA data from %s at 115200 baud...\n\n", UART_DEV);

    char buf[BUF_SIZE];
    char line[BUF_SIZE];
    int  line_pos = 0;

    while (1) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("read");
            break;
        }

        /* Buffer incoming bytes and print complete NMEA lines */
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') {
                line[line_pos] = '\0';
                printf("%s\n", line);
                fflush(stdout);
                line_pos = 0;
            } else if (c != '\r' && line_pos < BUF_SIZE - 1) {
                line[line_pos++] = c;
            }
        }
    }

    close(fd);
    return EXIT_SUCCESS;
}
