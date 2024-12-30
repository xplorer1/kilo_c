
#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <stdlib.h>
#include <stdbool.h>

int main() {
    char *input;
    bool time_to_exit = false;

    while (!time_to_exit) {
        input = readline("");
        if (input == NULL) {
            time_to_exit = true;
            goto loop_end;
        }

        if (*input == '\0')   // user just hit enter, no content
            goto loop_end;

        loop_end:

            free(input);
            input = NULL;

    }

    return 0;
}