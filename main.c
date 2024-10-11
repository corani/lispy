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
    LVAL_ERR,
    LVAL_NUM,
    LVAL_SYM,
    LVAL_SEXPR,
    LVAL_QEXPR,
} lval_type;

typedef struct lval {
    lval_type       Type;
    long            Num;
    char*           Err;
    char*           Sym;

    int             Count;
    struct lval**   Cell;
} lval;

lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));

    *v = (lval) { 
        .Type = LVAL_NUM, 
        .Num = x,
    };

    return v;
}

lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));

    char* msg = malloc(strlen(m) + 1);
    strcpy(msg, m);

    *v = (lval) {
        .Type = LVAL_ERR,
        .Err = msg,
    };

    return v;
}

lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));

    char* sym = malloc(strlen(s) + 1);
    strcpy(sym, s);

    *v = (lval) {
        .Type = LVAL_SYM,
        .Sym = sym,
    };

    return v;
}

lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));

    *v = (lval) {
        .Type = LVAL_SEXPR,
        .Count = 0,
        .Cell = NULL,
    };

    return v;
}

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));

    *v = (lval) {
        .Type = LVAL_QEXPR,
        .Count = 0,
        .Cell = NULL,
    };

    return v;
}

void lval_free(lval* v) {
    switch (v->Type) {
        case LVAL_NUM: 
            break;
        case LVAL_ERR: {
            free(v->Err); 
        } break;
        case LVAL_SYM: {
            free(v->Sym); 
        } break;
        case LVAL_SEXPR: case LVAL_QEXPR: {
            for (int i = 0; i < v->Count; i++) {
                lval_free(v->Cell[i]);
            }

            free(v->Cell);
        } break;
    }

    free(v);
}

lval* lval_add(lval* v, lval* x) {
    v->Count++;
    v->Cell = realloc(v->Cell, sizeof(lval*) * v->Count);
    v->Cell[v->Count - 1] = x;

    return v;
}

lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);

    return errno != ERANGE 
        ? lval_num(x) 
        : lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) return lval_read_num(t);
    if (strstr(t->tag, "symbol")) return lval_sym(t->contents);

    // If root (>) or sexpr create an empty list 
    lval* result = NULL;
    if (strcmp(t->tag, ">") == 0) result = lval_sexpr();
    if (strstr(t->tag, "sexpr"))  result = lval_sexpr();
    if (strstr(t->tag, "qexpr"))  result = lval_qexpr();

    // Fill this list with any valid expression contained within
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) continue;
        if (strcmp(t->children[i]->contents, ")") == 0) continue;
        if (strcmp(t->children[i]->contents, "{") == 0) continue;
        if (strcmp(t->children[i]->contents, "}") == 0) continue;
        if (strcmp(t->children[i]->tag,  "regex") == 0) continue;

        result = lval_add(result, lval_read(t->children[i]));
    }

    return result;
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);

    for (int i = 0; i < v->Count; i++) {
        lval_print(v->Cell[i]);

        if (i != (v->Count - 1)) {
            putchar(' ');
        }
    }

    putchar(close);
}

void lval_print(lval* v) {
    switch (v->Type) {
        case LVAL_NUM: {
            printf("%li", v->Num); 
        } break;
        case LVAL_ERR: {
            printf("Error: %s", v->Err);
        } break;
        case LVAL_SYM: {
            printf("%s", v->Sym);
        } break;
        case LVAL_SEXPR: {
            lval_expr_print(v, '(', ')');
        } break;
        case LVAL_QEXPR: {
            lval_expr_print(v, '{', '}');
        } break;
    }
}

void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

lval* lval_eval_sexpr(lval* v);

lval* lval_eval(lval* v) {
    if (v->Type == LVAL_SEXPR) return lval_eval_sexpr(v);

    return v;
}

lval* lval_pop(lval* v, int i) {
    lval* result = v->Cell[i];

    // Shift the memory after the item at "i" over the top
    memmove(&v->Cell[i], &v->Cell[i+1], sizeof(lval*) * (v->Count-i-1));

    v->Count--;
    v->Cell = realloc(v->Cell, sizeof(lval*) * v->Count);

    return result;
}

lval* lval_take(lval* v, int i) {
    lval* result = lval_pop(v, i);
    lval_free(v);

    return result;
}

lval* lval_join(lval* x, lval* y) {
    while (y->Count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    lval_free(y);

    return x; 
}

#define LASSERT(args, cond, err) \
    if (!(cond)) { lval_free(args); return lval_err(err); }

lval* builtin_head(lval* a) {
    LASSERT(a, a->Count == 1,  
        "Function 'head' passed too many arguments");
    LASSERT(a, a->Cell[0]->Type == LVAL_QEXPR,
        "Function 'head' passed incorrect type");
    LASSERT(a, a->Cell[0]->Count != 0, 
        "Function 'head' passed {}");

    lval* v = lval_take(a, 0);

    while (v->Count > 1) lval_free(lval_pop(v, 1));

    return v;
}

lval* builtin_tail(lval* a) {
    LASSERT(a, a->Count == 1,  
        "Function 'head' passed too many arguments");
    LASSERT(a, a->Cell[0]->Type == LVAL_QEXPR,
        "Function 'head' passed incorrect type");
    LASSERT(a, a->Cell[0]->Count != 0, 
        "Function 'head' passed {}");

    lval* v = lval_take(a, 0);

    lval_free(lval_pop(v, 0));

    return v;
}

lval* builtin_list(lval* a) {
    a->Type = LVAL_QEXPR;

    return a;
}

lval* builtin_eval(lval* v) {
    LASSERT(v, v->Count == 1, 
        "Function 'eval' passed too many arguments");
    LASSERT(v, v->Cell[0]->Type == LVAL_QEXPR,
        "Function 'eval' passed incorrect type");

    lval* x = lval_take(v, 0);
    x->Type = LVAL_SEXPR;

    return lval_eval(x);
}

lval* builtin_join(lval* a) {
    for (int i = 0; i < a->Count; i++) {
        LASSERT(a, a->Cell[i]->Type == LVAL_QEXPR,
            "Function 'join' passed incorrect type");
    }

    lval* result = lval_pop(a, 0);

    while (a->Count) {
        result = lval_join(result, lval_pop(a, 0));
    }

    lval_free(a);

    return result;
}

#undef LASSERT

lval* builtin_op(lval* a, char* op) {
    // Ensure all arguments are numbers
    for (int i = 0; i < a->Count; i++) {
        if (a->Cell[i]->Type != LVAL_NUM) {
            lval_free(a);

            return lval_err("Cannot operate on non-number");
        }
    }

    lval* x = lval_pop(a, 0);

    if ((strcmp(op, "-") == 0) && a->Count == 0) {
        x->Num = -x->Num;
    }

    while (a->Count > 0) {
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) x->Num += y->Num;
        if (strcmp(op, "-") == 0) x->Num -= y->Num;
        if (strcmp(op, "*") == 0) x->Num *= y->Num;
        if (strcmp(op, "/") == 0) {
            if (y->Num == 0) {
                lval_free(x);
                lval_free(y);

                x = lval_err("Division by zero");
                break;
            }

            x->Num /= y->Num;
        }

        lval_free(y);
    }

    lval_free(a);
    return x;
}

lval* builtin(lval* a, char* func) {
    if (strcmp("list", func) == 0) return builtin_list(a);
    if (strcmp("head", func) == 0) return builtin_head(a);
    if (strcmp("tail", func) == 0) return builtin_tail(a);
    if (strcmp("join", func) == 0) return builtin_join(a);
    if (strcmp("eval", func) == 0) return builtin_eval(a);
    if (strstr("+-/*", func)) return builtin_op(a, func);

    lval_free(a);

    return lval_err("Unknown function");
}

lval* lval_eval_sexpr(lval* v) {
    // Recursively evaluate children
    for (int i = 0; i < v->Count; i++) {
        v->Cell[i] = lval_eval(v->Cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->Count; i++) {
        if (v->Cell[i]->Type == LVAL_ERR) return lval_take(v, i);
    }

    // Empty expression
    if (v->Count == 0) return v;

    // Single expression
    if (v->Count == 1) return lval_take(v, 0);

    // Ensure first element is a symbol
    lval* f = lval_pop(v, 0);
    if (f->Type != LVAL_SYM) {
        lval_free(f);
        lval_free(v);

        return lval_err("S-expression does not start with symbol");
    }

    // Call builtin with operator
    lval* result = builtin(v, f->Sym);
    lval_free(f);

    return result;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Create some parsers
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr  = mpc_new("sexpr");
    mpc_parser_t* Qexpr  = mpc_new("qexpr");
    mpc_parser_t* Expr   = mpc_new("expr");
    mpc_parser_t* Lispy  = mpc_new("lispy");

    // Define the language
    mpca_lang(MPCA_LANG_DEFAULT, "                              \
            number : /-?[0-9]+/ ;                               \
            symbol : \"list\" | \"head\" | \"tail\" | \"join\"  \
                   | \"eval\" | '+' | '-' | '*' | '/' ;         \
            sexpr  : '(' <expr>* ')' ;                          \
            qexpr  : '{' <expr>* '}' ;                          \
            expr   : <number> | <symbol> | <sexpr> | <qexpr> ;  \
            lispy    : /^/ <expr>* /$/ ;                        \
        ", Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

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
            // mpc_ast_print(r.output);

            // Convert to s-expression
            lval* result = lval_read(r.output);
            lval_println(result);

            // Evaluate the s-expression
            result = lval_eval(result);
            lval_println(result);

            lval_free(result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}
