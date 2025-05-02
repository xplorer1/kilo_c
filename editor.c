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

/*** defines ***/
#define CTRL_KEY(k) ((k) & 0x1f) //to hold our control key definitions.
#define ABUF_INIT {NULL, 0}
#define KILO_VERSION "0.0.1"

struct EditorConfig {
    int cx, cy;
    int screen_rows;
    int screen_cols;
    struct termios orig_termios;
};

struct EditorConfig e_config;

enum editor_key {
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

/*** terminal ***/
void die(const char *str) {

    //To clear the screen and reposition the cursor when our program exits.
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(str);
    exit(1);
}

void disable_raw_mode() {
    //disable raw mode and if any error, die.
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &e_config.orig_termios) == -1)
        die("tcsetattr");
}

void enable_raw_mode() {
    //get current terminal attributes and save them in the orig_termios struct.
    if (tcgetattr(STDIN_FILENO, &e_config.orig_termios) == -1) die("tcgetattr");

    atexit(disable_raw_mode);
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
void editor_draw_rows(struct abuf *ab) {
    int y;
    for (y = 0; y < e_config.screen_rows; y++) {
        if (y == e_config.screen_rows / 3) {
            char welcome[80];
            int welcome_len = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);

            if (welcome_len > e_config.screen_cols) welcome_len = e_config.screen_cols; 
            int padding = (e_config.screen_cols - welcome_len) / 2;

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

        if (y < e_config.screen_rows - 1) {
            ab_append(ab, "\r\n", 2);
        }
    }
}

void editor_refresh_screen() {
    struct abuf ab = ABUF_INIT;

    ab_append(&ab, "\x1b[?25l", 6);
    ab_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", e_config.cy + 1, e_config.cx + 1);
    ab_append(&ab, buf, strlen(buf));

    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

int editor_read_key() {
    int nread;
    char input;

    //Read input from the keyboard and return the character.
    while ((nread = read(STDIN_FILENO, &input, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    if (input == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }

        return '\x1b';

    } else {
        return input;
    }
}

/*** input ***/
void editor_move_cursor(int key) {
    switch (key) {
        case ARROW_LEFT:
            if (e_config.cx != 0) e_config.cx--;
            break;
        case ARROW_RIGHT:
            if (e_config.cx != e_config.screen_cols - 1) e_config.cx++;
            break;
        case ARROW_UP:
            if (e_config.cy != 0) e_config.cy--;
            break;
        case ARROW_DOWN:
            if (e_config.cy != e_config.screen_rows - 1) e_config.cy++;
            break;
    }
}

void editor_process_keypress() {

    //Recieve the input from keyboard and map it to editor operations.
    int ch = editor_read_key();
    switch (ch) {
        case CTRL_KEY('q'):

            //To clear the screen and reposition the cursor when our program exits.
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);

            exit(0);
        break;

        case HOME_KEY:
            e_config.cx = 0;
        break;

        case END_KEY:
            e_config.cx = e_config.screen_cols - 1;
        break;

        case PAGE_UP:
        case PAGE_DOWN: {
            int times = e_config.screen_rows;
            while (times--) {
                editor_move_cursor(ch == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
        }
        break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(ch);
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
int get_cursor_position(int *rows, int *cols) {
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
int get_windowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {

        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;

        return get_cursor_position(rows, cols);
    } else {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

void initialize_editor() {
    e_config.cx = 0;
    e_config.cy = 0;
  if (get_windowSize(&e_config.screen_rows, &e_config.screen_cols) == -1) die("get_windowSize");
}

void run_editor() {
    enable_raw_mode();
    initialize_editor();

    while(1) {
        editor_refresh_screen();
        editor_process_keypress();
    }
}