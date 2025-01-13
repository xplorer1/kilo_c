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
#define ABUF_INIT {NULL, 0}
#define KILO_VERSION "0.0.1"

struct editorConfig {
    int screenrows;
    int screencols;
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

/*** append buffer ***/
struct abuf {
    char *b;
    int len;
};

void ab_append(struct abuf *ab, const char *s, int len) {

    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return; //die("realloc");

    memcpy(&new[ab->len], s, len);

    ab->b = new;
    ab->len += len;
}

void ab_free(struct abuf *ab) {
    free(ab->b);
}

/*** output ***/
void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < e_config.screenrows; y++) {
        if (y == e_config.screenrows / 3) {
            char welcome[80];
            int welcome_len = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);

            if (welcome_len > e_config.screencols) welcome_len = e_config.screencols; 
            int padding = (e_config.screencols - welcome_len) / 2;

            if (padding) {
                ab_append(ab, "~", 1);
                padding--;
            }

            while (padding--) ab_append(ab, " ", 1);
            ab_append(ab, welcome, welcome_len);

        } else {
            ab_append(ab, "~", 1);
        }

        ab_append(ab, "\x1b[K", 3);

        if (y < e_config.screenrows - 1) {
            ab_append(ab, "\r\n", 2);
        }
    }
}

void editorRefreshScreen() {
    struct abuf ab = ABUF_INIT;

    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);

    ab_append(&ab, "\x1b[H", 3);
    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
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

/**
 * Get the cursor position using ANSI escape sequences.
 * Return 0 if successful, otherwise return -1.
 * This function is used to determine the current position of the cursor in the terminal window so that the editor can properly handle input.
 * The ANSI escape sequence "\x1b[6n" is used to retrieve the current cursor position.
 * The write() function is used to send a string to the terminal.
 * The STDOUT_FILENO constant represents the file descriptor for standard output, which is the terminal.
 */
int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }

    buf[i] = '\0';
    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

/**
 * Get the size of the terminal window using ioctl() method.
 * Return 0 if successful, otherwise return -1.
 * This function is used to determine the size of the terminal window so that the editor can properly display text and handle input.
 * The TIOCGWINSZ request is used to retrieve the current width and height of the terminal window.
 * The ioctl() function is used to send a request to the kernel to perform a specific operation.
 * The STDOUT_FILENO constant represents the file descriptor for standard output, which is the terminal.
 */
int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {

        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;

        return getCursorPosition(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void initialize_editor() {
  if (getWindowSize(&e_config.screenrows, &e_config.screencols) == -1) die("getWindowSize");
}

void run_editor() {
    enableRawMode();
    initialize_editor();

    while(1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
}