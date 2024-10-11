#include <stdio.h>
#include <stdlib.h>

#include "mpc/mpc.h"

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

typedef enum {
    LVAL_NUM,
    LVAL_ERR,
} lval_type;

typedef enum {
    ERR_DIV_ZERO,
    ERR_BAD_OP,
    ERR_BAD_NUM,
} err_type;

typedef struct {
    lval_type   Type;
    long        Num;
    err_type    Err;
} lval;

lval lval_num(long x) {
    return (lval) { 
        .Type = LVAL_NUM, 
        .Num = x,
    };
}

lval lval_err(err_type e) {
    return (lval) {
        .Type = LVAL_ERR,
        .Err = e,
    };
}

void lval_print(lval v) {
    switch (v.Type) {
        case LVAL_NUM: {
            printf("%li", v.Num); 
        } break;
        case LVAL_ERR: {
            switch (v.Err) {
                case ERR_DIV_ZERO: {
                    printf("Error: Division by zero"); 
                } break;
                case ERR_BAD_OP: {
                    printf("Error: Invalid operator"); 
                } break;
                case ERR_BAD_NUM: {
                    printf("Error: Invalid number"); 
                } break;
            }
        } break;
    }
}

void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}

lval eval_op(lval x, char* op, lval y) {
    if (x.Type == LVAL_ERR) return x;
    if (y.Type == LVAL_ERR) return y;

    if (strcmp(op, "+") == 0) return lval_num(x.Num + y.Num); 
    if (strcmp(op, "-") == 0) return lval_num(x.Num - y.Num);
    if (strcmp(op, "*") == 0) return lval_num(x.Num * y.Num);

    if (strcmp(op, "/") == 0) {
        return y.Num == 0 
            ? lval_err(ERR_DIV_ZERO) 
            : lval_num(x.Num / y.Num);
    }

    return lval_err(ERR_BAD_OP);
}

lval eval(mpc_ast_t* t) {
    // Base case: If tagged as number return it directly
    if (strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);

        return errno != ERANGE ? lval_num(x) : lval_err(ERR_BAD_NUM);
    }

    // Skip the '('
    int i = 1;

    // The operator is always the second child
    char* op = t->children[i++]->contents;

    // Accumulator x, starting with the first operand (third child)
    lval result = eval(t->children[i++]);

    if (strcmp(op, "-") == 0 && !strstr(t->children[i]->tag, "expr")) {
        return lval_num(-result.Num);
    }

    // Iterate the remaining children and combine
    while (strstr(t->children[i]->tag, "expr")) {
        result = eval_op(result, op, eval(t->children[i++]));
    }

    return result;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Create some parsers
    mpc_parser_t* Number   = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr     = mpc_new("expr");
    mpc_parser_t* Lispy    = mpc_new("lispy");

    // Define the language
    mpca_lang(MPCA_LANG_DEFAULT, "                              \
            number   : /-?[0-9]+/ ;                             \
            operator : '+' | '-' | '*' | '/' ;                  \
            expr     : <number> | '(' <operator> <expr>+ ')' ;  \
            lispy    : /^/ <operator> <expr>+ /$/ ;             \
        ", Number, Operator, Expr, Lispy);

    // Print version and exit information
    puts("Lispy Version 0.0.1");
    puts("Press Ctrl+c to Exit\n");

    // Infinite loop
    for (;;) {
        // Output prompt and get input
        char* input = readline("lispy> ");

        // Add input to history
        add_history(input);

        // Attempt to parse the user input
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            // On success print the AST 
            mpc_ast_print(r.output);

            // Evaluate the AST
            lval result = eval(r.output);
            printf("result: ");
            lval_println(result);

            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}
