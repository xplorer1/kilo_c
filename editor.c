#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <termios.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "editor.h"

#define CTRL_KEY(k) ((k) & 0x1f) //to hold our control key definitions.

struct editorConfig {
    struct termios orig_termios;
};

struct editorConfig e_config;

/*** terminal ***/
void die(const char *str) {

    //To clear the screen and reposition the cursor when our program exits.
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(str);
    exit(1);
}

void disableRawMode() {
    //disable raw mode and if any error, die.
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &e_config.orig_termios) == -1)
        die("tcsetattr");
}

void enableRawMode() {
    //get current terminal attributes and save them in the orig_termios struct.
    if (tcgetattr(STDIN_FILENO, &e_config.orig_termios) == -1) die("tcgetattr");

    atexit(disableRawMode);
    struct termios raw = e_config.orig_termios;

    //a::IGNBRK Ignore break condition.
    //b::ICRNL Turn off translating any carriage returns (13, '\r') inputted by the user into newlines (10, '\n'). 
    //c::INPCK Enable parity checking.
    //d::ISTRIP Strip character bits, which is not needed for our simple text editor.
    //e::IXON Turn off software flow control, which allows the program to read from and write to the terminal without 
        //waiting for the user to press Ctrl-Q or Ctrl-S.
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

    //a::OPOST Turn off output processing, which causes the program to send each character to the terminal using local flags - c_oflag.
    raw.c_oflag &= ~(OPOST);

    //c::CS8 Set character size to 8 bits, which is the most common size for modern keyboards.
    raw.c_cflag |= (CS8);

    //a::ECHO Turn off echoing, which causes each key you type to be printed to the terminal using local flags - c_lflag.
    //b::ICANON Turn off canonical mode, which causes the program to read one character at a time and wait 
        //for the user to press Enter before processing the input.
    //c::IEXTEN Turn off input processing of special characters, such as Ctrl-C and Ctrl-D, which are not needed for our simple text editor.
    //d::ISIG Turn off signal handling, which causes the program to exit gracefully when you press Ctrl-C.
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

    //Set the minimum number of characters to read (VMIN) to 0, which means that read() will block until a character is available.
    //Set the time to wait for characters (VTIME) to 1, which means that read() will wait for 1 second for a character to be available.
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    //Apply the modified terminal attributes to the stdin file descriptor. If any error occurs, die.
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

/*** output ***/

void editorDrawRows() {

    //To draw a tilde at the beginning of any lines that come after the end of the file being edited.
    int y;
    for (y = 0; y < 10; y++) {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

void editorRefreshScreen() {

    //To clear the screen and reposition the cursor when our program exits.
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    //Draw tilde after refresh.
    editorDrawRows();
    write(STDOUT_FILENO, "\x1b[H", 3);
}

char editorReadKey() {
    int nread;
    char input;

    //Read input from the keyboard and return the character.
    while ((nread = read(STDIN_FILENO, &input, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    return input;
}

/*** input ***/
void editorProcessKeypress() {

    //Recieve the input from keyboard and map it to editor operations.
    char ch = editorReadKey();
    switch (ch) {
        case CTRL_KEY('q'):

            //To clear the screen and reposition the cursor when our program exits.
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
        break;
    }
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void initialize_editor() {
    enableRawMode();

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
}