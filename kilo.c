
/*** includes ***/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

/*** data ***/
struct termios orig_termios;

/*** terminal ***/
void die(const char *str) {
    perror(str);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disableRawMode);
    struct termios raw = orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

/*** init ***/
int main() {
    enableRawMode();

    char input = '\0';
    bool time_to_exit = false;

    while (!time_to_exit) {
        size_t sz = read(STDIN_FILENO, &input, 1);
        if (sz == -1 && errno != EAGAIN) die("read");
        
        if (input == 'q') {
            time_to_exit = true;
        }

        if (iscntrl(input)) {
            printf("%d \r\n", input);
        } else {
            printf("%d ('%c')\r\n", input, input);
        }
    }

    return 0;
}