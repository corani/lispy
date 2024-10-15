#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>

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
    LVAL_STR,
    LVAL_FUN,
    LVAL_SEXPR,
    LVAL_QEXPR,
} lval_type;

char *lval_type_name(lval_type t) {
    switch (t) {
        case LVAL_ERR: return "Error";
        case LVAL_NUM: return "Number";
        case LVAL_SYM: return "Symbol";
        case LVAL_STR: return "String";
        case LVAL_FUN: return "Function";
        case LVAL_SEXPR: return "S-Expression";
        case LVAL_QEXPR: return "Q-Expression";
        default: return "Unknown";
    }
}

typedef struct lval lval;
typedef struct lenv lenv;
typedef lval* (*lbuiltin)(lenv*, lval*);

void lval_print(lval* v);
lval* lval_eval_sexpr(lenv* e, lval* v);
void lval_free(lval* v);
lval* lval_copy(lval *v);
lval* lval_err(char* fmt, ...);
lval* lval_sym(char* s);
lval* lval_fun(char* s, lbuiltin fun);
lval* lval_read_expr(char* s, int* i, char end);

// NOTE(daniel): Syms and Vals are parallel arrays.
struct lenv {
    lenv*   Parent;

    size_t  Count;
    char**  Syms;
    lval**  Vals;
};

struct lval {
    lval_type       Type;

    // Basic values
    long            Num;
    char*           Err;
    char*           Sym;
    char*           Str;

    // Functions
    lbuiltin        Builtin;
    lenv*           Env;
    lval*           Formals;
    lval*           Body;

    // Expressions
    size_t          Count;
    struct lval**   Cell;
};

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));

    *e = (lenv) {
        .Parent = NULL,
        .Count  = 0,
        .Syms   = NULL,
        .Vals   = NULL,
    };

    return e;
}

void lenv_free(lenv* e) {
    for (size_t i = 0; i < e->Count; ++i) {
        free(e->Syms[i]);
        lval_free(e->Vals[i]);
    }

    // NOTE(daniel): don't free the Parent, it's not owned by this environment.
    free(e->Syms);
    free(e->Vals);
    free(e);
}

lenv* lenv_copy(lenv* e) {
    lenv* n = malloc(sizeof(lenv));

    *n = (lenv) {
        .Parent = e->Parent,
        .Count  = e->Count,
        .Syms   = malloc(sizeof(char*) * e->Count),
        .Vals   = malloc(sizeof(lval*) * e->Count),
    };

    for (size_t i = 0; i < e->Count; ++i) {
        n->Syms[i] = malloc(strlen(e->Syms[i]) + 1);
        strcpy(n->Syms[i], e->Syms[i]);
        n->Vals[i] = lval_copy(e->Vals[i]);
    }

    return n;
}

lval* lenv_get(lenv* e, lval* k) {
    for (size_t i = 0; i < e->Count; ++i) {
        if (strcmp(e->Syms[i], k->Sym) == 0) {
            return lval_copy(e->Vals[i]);
        }
    }

    if (e->Parent) {
        return lenv_get(e->Parent, k);
    } else {
        return lval_err("Unbound symbol '%s'", k->Sym);
    }
}

void lenv_put(lenv* e, lval* k, lval* v) {
    // NOTE(daniel): if the symbol already exists, free the old value and replace it.
    for (size_t i = 0; i < e->Count; ++i) {
        if (strcmp(e->Syms[i], k->Sym) == 0) {
            lval_free(e->Vals[i]);
            e->Vals[i] = lval_copy(v);

            return;
        }
    }

    // NOTE(daniel): otherwise, create a new entry
    ++e->Count;
    e->Syms = realloc(e->Syms, sizeof(char*) * e->Count);
    e->Vals = realloc(e->Vals, sizeof(lval*) * e->Count);

    e->Syms[e->Count - 1] = malloc(strlen(k->Sym) + 1);
    strcpy(e->Syms[e->Count - 1], k->Sym);

    e->Vals[e->Count - 1] = lval_copy(v);
}

void lenv_def(lenv* e, lval* k, lval* v) {
    // NOTE(daniel): go up the parent chain to the root environment.
    while (e->Parent) e = e->Parent;

    lenv_put(e, k, v);
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin fun) {
    lval* k = lval_sym(name);
    lval* v = lval_fun(name, fun);

    lenv_put(e, k, v);
    lval_free(k);
    lval_free(v);
}

lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));

    *v = (lval) { 
        .Type = LVAL_NUM, 
        .Num = x,
    };

    return v;
}

lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));

    va_list va;
    va_start(va, fmt);

    // should be enough for anybody ;-)
    char *msg = malloc(512);

    vsnprintf(msg, 511, fmt, va);

    *v = (lval) {
        .Type = LVAL_ERR,
        .Err = realloc(msg, strlen(msg) + 1),
    };

    va_end(va);

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

lval* lval_str(char* s) {
    lval* v = malloc(sizeof(lval));

    char* str = malloc(strlen(s) + 1);
    strcpy(str, s);

    *v = (lval) {
        .Type = LVAL_STR,
        .Str = str,
    };

    return v;
}

lval* lval_fun(char* s, lbuiltin fun) {
    lval* v = malloc(sizeof(lval));

    char* sym = malloc(strlen(s) + 1);
    strcpy(sym, s);

    *v = (lval) {
        .Type = LVAL_FUN,
        .Sym = sym,
        .Builtin = fun,
    };

    return v;
}

lval* lval_lambda(lval* formals, lval* body) {
    lval* v = malloc(sizeof(lval));

    *v = (lval) {
        .Type = LVAL_FUN,
        .Builtin = NULL,
        .Env = lenv_new(),
        .Formals = formals,
        .Body = body,
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
        case LVAL_NUM: {
            // nothing to do
        } break;
        case LVAL_ERR: {
            free(v->Err); 
        } break;
        case LVAL_FUN: {
            if (v->Builtin) {
                free(v->Sym); 
            } else {
                lenv_free(v->Env);
                lval_free(v->Formals);
                lval_free(v->Body);
            }
        } break;
        case LVAL_SYM: {
            free(v->Sym); 
        } break;
        case LVAL_STR: {
            free(v->Str);
        } break;
        case LVAL_SEXPR: case LVAL_QEXPR: {
            for (size_t i = 0; i < v->Count; ++i) {
                lval_free(v->Cell[i]);
            }

            free(v->Cell);
        } break;
    }

    free(v);
}

lval* lval_copy(lval *v) {
    lval* x = malloc(sizeof(lval));
    x->Type = v->Type;

    switch (v->Type) {
        case LVAL_NUM: {
            x->Num = v->Num; 
        } break;
        case LVAL_FUN: {
            if (v->Builtin) {
                x->Sym = malloc(strlen(v->Sym) + 1);
                strcpy(x->Sym, v->Sym);

                x->Builtin = v->Builtin;
            } else {
                x->Builtin = NULL;
                x->Env = lenv_copy(v->Env);
                x->Formals = lval_copy(v->Formals);
                x->Body = lval_copy(v->Body);
            }
        } break;
        case LVAL_ERR: {
            x->Err = malloc(strlen(v->Err) + 1);
            strcpy(x->Err, v->Err);
        } break;
        case LVAL_SYM: {
            x->Sym = malloc(strlen(v->Sym) + 1);
            strcpy(x->Sym, v->Sym);
        } break;
        case LVAL_STR: {
            x->Str = malloc(strlen(v->Str) + 1);
            strcpy(x->Str, v->Str);
        }break;
        case LVAL_SEXPR: case LVAL_QEXPR: {
            x->Count = v->Count;
            x->Cell = malloc(sizeof(lval*) * x->Count);

            for (size_t i = 0; i < x->Count; ++i) {
                x->Cell[i] = lval_copy(v->Cell[i]);
            }
        } break;
    }
    
    return x;
}

int lval_eq(lval* x, lval* y) {
    if (x->Type != y->Type) return 0;

    switch (x->Type) {
        case LVAL_NUM: 
            return x->Num == y->Num;

        case LVAL_ERR: 
            return strcmp(x->Err, y->Err) == 0;
        case LVAL_SYM: 
            return strcmp(x->Sym, y->Sym) == 0;
        case LVAL_STR:
            return strcmp(x->Str, y->Str) == 0;

        case LVAL_FUN: {
            if (x->Builtin || y->Builtin) {
                return x->Builtin == y->Builtin;
            } else {
                return lval_eq(x->Formals, y->Formals) && lval_eq(x->Body, y->Body);
            }
        }

        case LVAL_SEXPR: case LVAL_QEXPR: {
            if (x->Count != y->Count) return 0;

            for (size_t i = 0; i < x->Count; ++i) {
                if (!lval_eq(x->Cell[i], y->Cell[i])) return 0;
            }

            return 1;
        }
    }

    return 0;
}

lval* lval_add(lval* v, lval* x) {
    ++v->Count;
    v->Cell = realloc(v->Cell, sizeof(lval*) * v->Count);
    v->Cell[v->Count - 1] = x;

    return v;
}

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);

    for (size_t i = 0; i < v->Count; ++i) {
        lval_print(v->Cell[i]);

        if (i != (v->Count - 1)) {
            putchar(' ');
        }
    }

    putchar(close);
}

// Possible unescapable characters 
char* lval_str_unescapable = "abfnrtv\\\'\"";

char lval_str_unescape(char x) {
    switch (x) {
        case 'a' : return '\a';
        case 'b' : return '\b';
        case 'f' : return '\f';
        case 'n' : return '\n';
        case 'r' : return '\r';
        case 't' : return '\t';
        case 'v' : return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '\"': return '\"';
        default  : return '\0';
    }
}

// Possible escapable characters
char* lval_str_escapable = "\a\b\f\n\r\t\v\\\'\"";

char* lval_str_escape(char x) {
    switch(x) {
        case '\a': return "\\a";
        case '\b': return "\\b";
        case '\f': return "\\f";
        case '\n': return "\\n";
        case '\r': return "\\r";
        case '\t': return "\\t";
        case '\v': return "\\v";
        case '\\': return "\\\\";
        case '\'': return "\\\'";
        case '\"': return "\\\"";
        default  : return "";
    }
}

void lval_str_print(lval* v) {
    putchar('"');

    for (size_t i = 0; i < strlen(v->Str); ++i) {
        if (strchr(lval_str_escapable, v->Str[i])) {
            printf("%s", lval_str_escape(v->Str[i]));
        } else {
            putchar(v->Str[i]);
        }
    }

    putchar('"');
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
        case LVAL_STR: {
            lval_str_print(v);
        } break;
        case LVAL_FUN: {
            if (v->Builtin) {
                printf("<builtin '%s'>", v->Sym);
            } else {
                printf("(\\ "); 
                lval_print(v->Formals);
                putchar(' ');
                lval_print(v->Body);
                putchar(')');
            }
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

lval* lval_eval(lenv* e, lval* v) {
    switch (v->Type) {
        case LVAL_SYM: {
            lval* x = lenv_get(e, v);
            lval_free(v);

            return x;
        }
        case LVAL_SEXPR: 
            return lval_eval_sexpr(e, v);
        case LVAL_NUM: case LVAL_ERR: case LVAL_FUN: case LVAL_QEXPR: 
        default:
            return v;
    }
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

#define LASSERT(args, cond, ...)            \
    if (!(cond)) {                          \
        lval* err = lval_err(__VA_ARGS__);  \
        lval_free(args);                    \
        return err;                         \
    }

#define LASSERT_COUNT(args, name, count)          \
    LASSERT(args, args->Count == count,           \
        "Function '%s' passed incorrect number of arguments. Got %i, Expected %i.", \
        name, args->Count, count);

#define LASSERT_TYPE(args, name, i, type)         \
    LASSERT(args, args->Cell[i]->Type == type,    \
        "Function '%s' passed incorrect type for argument %i. Got %s, Expected %s.", \
        name, i, lval_type_name(args->Cell[i]->Type), lval_type_name(type));

lval* builtin_head(lenv* e, lval* a) {
    (void)e;

    LASSERT_COUNT(a, "head", 1);
    LASSERT_TYPE(a, "head", 0, LVAL_QEXPR);
    LASSERT(a, a->Cell[0]->Count != 0, 
        "Function 'head' passed {}");

    lval* result = lval_take(a, 0);

    while (result->Count > 1) lval_free(lval_pop(result, 1));

    return result;
}

lval* builtin_tail(lenv* e, lval* a) {
    (void)e;

    LASSERT_COUNT(a, "tail", 1);
    LASSERT_TYPE(a, "tail", 0, LVAL_QEXPR);
    LASSERT(a, a->Cell[0]->Count != 0, 
        "Function 'head' passed {}");

    lval* result = lval_take(a, 0);

    lval_free(lval_pop(result, 0));

    return result;
}

lval* builtin_list(lenv* e, lval* a) {
    (void)e;

    a->Type = LVAL_QEXPR;

    return a;
}

lval* builtin_eval(lenv* e, lval* v) {
    LASSERT_COUNT(v, "eval", 1);
    LASSERT_TYPE(v, "eval", 0, LVAL_QEXPR);

    lval* x = lval_take(v, 0);
    x->Type = LVAL_SEXPR;

    return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
    (void)e;

    for (size_t i = 0; i < a->Count; ++i) {
        LASSERT_TYPE(a, "join", i, LVAL_QEXPR);
    }

    lval* result = lval_pop(a, 0);

    while (a->Count) {
        result = lval_join(result, lval_pop(a, 0));
    }

    lval_free(a);

    return result;
}

lval* builtin_var(lenv* e, lval* a, char* fun) {
    LASSERT_TYPE(a, "def", 0, LVAL_QEXPR);
    
    lval* Syms = a->Cell[0];

    for (size_t i = 0; i < Syms->Count; ++i) {
        LASSERT(a, (Syms->Cell[i]->Type == LVAL_SYM), 
            "Function 'def' cannot define non-symbol. Got %s, Expected %s.",
            lval_type_name(Syms->Cell[i]->Type), lval_type_name(LVAL_SYM));
    }

    LASSERT(a, Syms->Count == a->Count-1, 
        "Function 'def' cannot define incorrect number of values. Got %i, Expected %i.",
        Syms->Count, a->Count-1);

    for (size_t i = 0; i < Syms->Count; ++i) {
        if (strcmp(fun, "def") == 0) {
            lenv_def(e, Syms->Cell[i], a->Cell[i+1]);
        } else if (strcmp(fun, "=") == 0) {
            lenv_put(e, Syms->Cell[i], a->Cell[i+1]);
        }
    }

    lval_free(a);

    return lval_sexpr();
}

lval* builtin_def(lenv* e, lval* a) {
    return builtin_var(e, a, "def");
}

lval* builtin_put(lenv* e, lval* a) {
    return builtin_var(e, a, "=");
}

lval* builtin_lambda(lenv* e, lval* a) {
    (void)e;

    LASSERT_COUNT(a, "\\", 2);
    LASSERT_TYPE(a, "\\", 0, LVAL_QEXPR);
    LASSERT_TYPE(a, "\\", 1, LVAL_QEXPR);

    for (size_t i = 0; i < a->Cell[0]->Count; ++i) {
        LASSERT(a, (a->Cell[0]->Cell[i]->Type == LVAL_SYM), 
            "Cannot define non-symbol. Got %s, Expected %s.",
            lval_type_name(a->Cell[0]->Cell[i]->Type), lval_type_name(LVAL_SYM));
    }

    lval* formals = lval_pop(a, 0);
    lval* body = lval_pop(a, 0);

    lval_free(a);

    return lval_lambda(formals, body);
}

lval* builtin_op(lenv* e, lval* a, char* op) {
    (void)e;

    // Ensure all arguments are numbers
    for (size_t i = 0; i < a->Count; ++i) {
        if (a->Cell[i]->Type != LVAL_NUM) {
            LASSERT_TYPE(a, op, i, LVAL_NUM);
        }
    }

    lval* result = lval_pop(a, 0);

    if ((strcmp(op, "-") == 0) && a->Count == 0) {
        result->Num = -result->Num;
    }

    while (a->Count > 0) {
        lval* y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) result->Num += y->Num;
        if (strcmp(op, "-") == 0) result->Num -= y->Num;
        if (strcmp(op, "*") == 0) result->Num *= y->Num;
        if (strcmp(op, "/") == 0) {
            if (y->Num == 0) {
                lval_free(result);
                lval_free(y);

                result = lval_err("Division by zero");
                break;
            }

            result->Num /= y->Num;
        }

        lval_free(y);
    }

    lval_free(a);
    return result;
}

lval* builtin_add(lenv* e, lval* a) {
    return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
    return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
    return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
    return builtin_op(e, a, "/");
}

lval* builtin_ord(lenv* e, lval* a, char* op) {
    (void)e;

    LASSERT_COUNT(a, op, 2);
    LASSERT_TYPE(a, op, 0, LVAL_NUM);
    LASSERT_TYPE(a, op, 1, LVAL_NUM);

    int result = 0;

    if (strcmp(op, ">") == 0) {
        result = a->Cell[0]->Num > a->Cell[1]->Num;
    } else if (strcmp(op, "<") == 0) {
        result = a->Cell[0]->Num < a->Cell[1]->Num;
    } else if (strcmp(op, ">=") == 0) {
        result = a->Cell[0]->Num >= a->Cell[1]->Num;
    } else if (strcmp(op, "<=") == 0) {
        result = a->Cell[0]->Num <= a->Cell[1]->Num;
    }

    lval_free(a);

    return lval_num(result);
}

lval* builtin_gt(lenv* e, lval* a) {
    return builtin_ord(e, a, ">");
}

lval* builtin_lt(lenv* e, lval* a) {
    return builtin_ord(e, a, "<");
}

lval* builtin_ge(lenv* e, lval* a) {
    return builtin_ord(e, a, ">=");
}

lval* builtin_le(lenv* e, lval* a) {
    return builtin_ord(e, a, "<=");
}

lval* builtin_cmp(lenv* e, lval* a, char* op) {
    (void)e;

    LASSERT_COUNT(a, op, 2);

    int result = 0;
    
    if (strcmp(op, "==") == 0) {
        result = lval_eq(a->Cell[0], a->Cell[1]);
    } else if (strcmp(op, "!=") == 0) {
        result = !lval_eq(a->Cell[0], a->Cell[1]);
    }

    lval_free(a);

    return lval_num(result);
}

lval* builtin_eq(lenv* e, lval* a) {
    return builtin_cmp(e, a, "==");
}

lval* builtin_ne(lenv* e, lval* a) {
    return builtin_cmp(e, a, "!=");
}

lval* builtin_if(lenv* e, lval* a) {
    LASSERT_COUNT(a, "if", 3);
    LASSERT_TYPE(a, "if", 0, LVAL_NUM);
    LASSERT_TYPE(a, "if", 1, LVAL_QEXPR);
    LASSERT_TYPE(a, "if", 2, LVAL_QEXPR);

    // Turn both Q-Expression into S-Expression, so they can be evaluated. 
    a->Cell[1]->Type = LVAL_SEXPR;
    a->Cell[2]->Type = LVAL_SEXPR;

    lval* result = NULL;

    if (a->Cell[0]->Num) {
        result = lval_eval(e, lval_pop(a, 1));
    } else {
        result = lval_eval(e, lval_pop(a, 2));
    }

    lval_free(a);

    return result;
}

lval* builtin_load(lenv* e, lval* a) {
    LASSERT_COUNT(a, "load", 1);
    LASSERT_TYPE(a, "load", 0, LVAL_STR);

    // Open file and check if exists 
    FILE* f = fopen(a->Cell[0]->Str, "rb");
    if (!f) {
        lval* err = lval_err("Could not load library %s", a->Cell[0]->Str);
        lval_free(a);

        return err; 
    }

    // Read file contents
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* input = calloc(length+1, 1);
    fread(input, 1, length, f);
    fclose(f);

    // Parse file
    int pos = 0;
    lval* expr = lval_read_expr(input, &pos, '\0');
    free(input);

    if (expr->Type != LVAL_ERR) {
        while (expr->Count) {
            lval* x = lval_eval(e, lval_pop(expr, 0));

            if (x->Type == LVAL_ERR) {
                lval_println(x);
            }

            lval_free(x);
        }
    } else {
        lval_println(expr);
    }

    lval_free(expr);
    lval_free(a);

    return lval_sexpr();
}

lval* builtin_print(lenv* e, lval* a) {
    (void)e;

    for (size_t i = 0; i < a->Count; ++i) {
        lval_print(a->Cell[i]);
        putchar(' ');
    }

    putchar('\n');
    lval_free(a);

    return lval_sexpr();
}

lval* builtin_error(lenv* e, lval* a) {
    (void)e;

    LASSERT_COUNT(a, "error", 1);
    LASSERT_TYPE(a, "error", 0, LVAL_STR);

    lval* err = lval_err(a->Cell[0]->Str);
    lval_free(a);

    return err;
}

#undef LASSERT
#undef LASSERT_COUNT 
#undef LASSERT_TYPE

void lenv_add_builtins(lenv* e) {
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "eval", builtin_eval);

    lenv_add_builtin(e, "def", builtin_def);
    lenv_add_builtin(e, "=", builtin_put);
    lenv_add_builtin(e, "\\", builtin_lambda);

    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);

    lenv_add_builtin(e, "if", builtin_if);
    lenv_add_builtin(e, "==", builtin_eq);
    lenv_add_builtin(e, "!=", builtin_ne);
    lenv_add_builtin(e, ">",  builtin_gt);
    lenv_add_builtin(e, "<",  builtin_lt);
    lenv_add_builtin(e, ">=", builtin_ge);
    lenv_add_builtin(e, "<=", builtin_le);

    lenv_add_builtin(e, "load", builtin_load);
    lenv_add_builtin(e, "print", builtin_print);
    lenv_add_builtin(e, "error", builtin_error);
}

lval* lval_call(lenv *e, lval* f, lval* a) {
    if (f->Builtin) return f->Builtin(e, a);

    int given = a->Count;
    int total = f->Formals->Count;

    while (a->Count) {
        if (f->Formals->Count == 0) {
            lval_free(a);
            return lval_err("Function passed too many arguments. Got %i, Expected %i.", given, total);
        }

        lval* sym = lval_pop(f->Formals, 0);

        if (strcmp(sym->Sym, "&") == 0) {
            if (f->Formals->Count != 1) {
                lval_free(sym);
                lval_free(a);

                return lval_err("Function format invalid. Symbol '&' not followed by single symbol.");
            }

            lval* nsym = lval_pop(f->Formals, 0);
            lenv_put(f->Env, nsym, builtin_list(e, a));

            lval_free(sym);
            lval_free(nsym);

            break;
        }

        lval* val = lval_pop(a, 0);

        // NOTE(daniel): bind the value to the symbol in the environment.
        lenv_put(f->Env, sym, val);

        lval_free(sym);
        lval_free(val);
    }

    lval_free(a);

    // NOTE(daniel): if '&' remains in the formal list bind it to an empty list.
    if (f->Formals->Count > 0 && strcmp(f->Formals->Cell[0]->Sym, "&") == 0) {
        if (f->Formals->Count != 2) {
            return lval_err("Function format invalid. Symbol '&' not followed by single symbol.");
        }

        lval_free(lval_pop(f->Formals, 0));

        lval* sym = lval_pop(f->Formals, 0);
        lval* val = lval_qexpr();

        lenv_put(f->Env, sym, val);
        lval_free(sym);
        lval_free(val);
    }

    // NOTE(daniel): if all formals have been bound evaluate the function,
    // otherwise return partially evaluated function.
    if (f->Formals->Count == 0) {
        f->Env->Parent = e;

        return builtin_eval(f->Env, lval_add(lval_sexpr(), lval_copy(f->Body)));
    } else {
        return lval_copy(f);
    }
}

lval* lval_eval_sexpr(lenv* e, lval* v) {
    // Recursively evaluate children
    for (size_t i = 0; i < v->Count; ++i) {
        v->Cell[i] = lval_eval(e, v->Cell[i]);
    }

    // Error checking
    for (size_t i = 0; i < v->Count; ++i) {
        if (v->Cell[i]->Type == LVAL_ERR) return lval_take(v, i);
    }

    // Empty expression
    if (v->Count == 0) return v;

    // Single expression
    if (v->Count == 1) return lval_take(v, 0);

    // Ensure first element is a function
    lval* f = lval_pop(v, 0);
    if (f->Type != LVAL_FUN) {
        lval* err = lval_err("S-expression does not start with function. Got %s, Expected %s.",
            lval_type_name(f->Type), lval_type_name(LVAL_FUN));

        lval_free(f);
        lval_free(v);

        return err;
    }

    // Call builtin with operator
    lval* result = lval_call(e, f, v);
    lval_free(f);

    return result;
}

void load_file(lenv* env, char* filename) {
    lval* args = lval_add(lval_sexpr(), lval_str(filename));
    lval* x = builtin_load(env, args);

    if (x->Type == LVAL_ERR) {
        lval_println(x);
    }

    lval_free(x);
}

lval* lval_read(char* s, int* i);

lval* lval_read_expr(char* s, int* i, char end) {
    lval* x = (end == '}') ? lval_qexpr() : lval_sexpr();

    while (s[*i] != end) {
        lval* y = lval_read(s, i);

        if (y->Type == LVAL_ERR) {
            lval_free(x);

            return y;
        }

        lval_add(x, y);
    }

    ++(*i);

    return x;
}

char* lval_str_sym_characters = 
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "_+-*\\/=<>!&";

lval* lval_read_sym(char* s, int* i) {
    char* part = calloc(1, 1);

    while (strchr(lval_str_sym_characters, s[*i]) && s[*i] != '\0') {
        part = realloc(part, strlen(part) + 2);
        part[strlen(part) + 0] = s[*i];
        part[strlen(part) + 1] = '\0';
        ++(*i);
    }

    // Check if identifier looks like a number
    int is_num = strchr("-0123456789", part[0]) != NULL;
    for (size_t i = 1; i < strlen(part); ++i) {
        if (!strchr("0123456789", part[i])) {
            is_num = 0;
            break;
        }
    }
    if (strlen(part) == 1 && part[0] == '-') is_num = 0;

    lval* x = NULL;
    if (is_num) {
        errno = 0;
        long num = strtol(part, NULL, 10);
        x = (errno != ERANGE) ? lval_num(num) : lval_err("Invalid number");
    } else {
        x = lval_sym(part);
    }

    free(part);

    return x;
}

lval* lval_read_str(char* s, int* i) {
    // Skip the initial quote
    ++(*i);

    char* part = calloc(1, 1);
    while (s[*i] != '"') {
        char c = s[*i];

        if (c == '\0') {
            free(part);

            return lval_err("Unexpected end of input");
        }

        // Read escape sequence
        if (c == '\\') {
            ++(*i);

            if (strchr(lval_str_unescapable, s[*i])) {
                c = lval_str_unescape(s[*i]);
            } else {
                free(part);

                return lval_err("Invalid escape sequence \\%c", s[*i]);
            }
        }

        // Append character to string
        part = realloc(part, strlen(part) + 2);
        part[strlen(part) + 0] = c;
        part[strlen(part) + 1] = '\0';

        ++(*i);
    }

    // Skip the final quote
    ++(*i);

    lval* x = lval_str(part);
    free(part);

    return x;
}

lval* lval_read(char* s, int* i) {
    // Skip trailing whitespace and comments
    while (strchr(" \t\v\r\n;", s[*i]) && s[*i] != '\0') {
        if (s[*i] == ';') {
            while (s[*i] != '\n' && s[*i] != '\0') ++(*i);
        }

        ++(*i);
    }

    lval* x = NULL;

    if (s[*i] == '\0') return lval_err("Unexpected end of input");

    if (s[*i] == '(') {
        ++(*i);
        x = lval_read_expr(s, i, ')');
    } else if (s[*i] == '{') {
        ++(*i);
        x = lval_read_expr(s, i, '}');
    } else if (strchr(lval_str_sym_characters, s[*i])) {
        x = lval_read_sym(s, i);
    } else if (strchr("\"", s[*i])) {
        x = lval_read_str(s, i);
    } else {
        x = lval_err("Unexpected character %c", s[*i]);
    }

    // Skip trailing whitespace and comments
    while (strchr(" \t\v\r\n;", s[*i]) && s[*i] != '\0') {
        if (s[*i] == ';') {
            while (s[*i] != '\n' && s[*i] != '\0') ++(*i);
        }

        ++(*i);
    }

    return x;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    lenv* env = lenv_new();
    lenv_add_builtins(env);
    load_file(env, "stdlib.lisp");

    if (argc == 1) {
        // Print version and exit information
        puts("Lispy Version 0.0.1");
        puts("Press Ctrl+c to Exit\n");

        // REPL
        for (;;) {
            // Output prompt and get input
            char* input = readline("lispy> ");

            // Add input to history
            add_history(input);

            // Read from input to create an S-Expression
            int pos = 0;
            lval* expr = lval_read_expr(input, &pos, '\0');

            lval* result = lval_eval(env, expr);
            lval_println(result);
            lval_free(result);

            free(input);
        }
    } else {
        // Load files
        for (size_t i = 1; i < (size_t)argc; ++i) {
            load_file(env, argv[i]);
        }
    }

    return 0;
}
