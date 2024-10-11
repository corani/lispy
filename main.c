#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

// Fake readline function
char *readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);

    char *result = malloc(strlen(buffer)+1);
    strcpy(result, buffer);
    cpy[strlen(result)-1] = '\0';

    return result
}

// Fake add_history function
void add_history(char* unused) { (void)unused; }

#else 

// libedit-dev
#include <editline/readline.h>
#include <editline/history.h>

#endif

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Print version and exit information
    puts("Lispy Version 0.0.1");
    puts("Press Ctrl+c to Exit\n");

    // Infinite loop
    for (;;) {
        // Output prompt and get input
        char* input = readline("lispy> ");

        // Add input to history
        add_history(input);

        // Echo input back to user
        printf("No you're a %s\n", input);

        free(input);
    }

    return 0;
}
