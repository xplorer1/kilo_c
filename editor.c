#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include "editor.h"

#define CTRL_KEY(k) ((k) & 0x1f)

struct termios orig_termios;

/*** terminal ***/
void die(const char *str) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

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

/*** output ***/
void editorRefreshScreen() {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

char editorReadKey() {
    int nread;
    char input;

    while ((nread = read(STDIN_FILENO, &input, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    return input;
}

/*** input ***/
void editorProcessKeypress() {
    char ch = editorReadKey();
    switch (ch) {
        case CTRL_KEY('q'):

            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
        break;
    }
}

void initialize_editor() {
    enableRawMode();

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
}