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
    LVAL_FUN,
    LVAL_SEXPR,
    LVAL_QEXPR,
} lval_type;

char *lval_type_name(lval_type t) {
    switch (t) {
        case LVAL_ERR: return "Error";
        case LVAL_NUM: return "Number";
        case LVAL_SYM: return "Symbol";
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

// NOTE(daniel): Syms and Vals are parallel arrays.
struct lenv {
    lenv*   Parent;
    int     Count;
    char**  Syms;
    lval**  Vals;
};

struct lval {
    lval_type       Type;

    // Basic values
    long            Num;
    char*           Err;
    char*           Sym;

    // Functions
    lbuiltin        Builtin;
    lenv*           Env;
    lval*           Formals;
    lval*           Body;

    // Expressions
    int             Count;
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
    for (int i = 0; i < e->Count; i++) {
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

    for (int i = 0; i < e->Count; i++) {
        n->Syms[i] = malloc(strlen(e->Syms[i]) + 1);
        strcpy(n->Syms[i], e->Syms[i]);
        n->Vals[i] = lval_copy(e->Vals[i]);
    }

    return n;
}

lval* lenv_get(lenv* e, lval* k) {
    for (int i = 0; i < e->Count; i++) {
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
    for (int i = 0; i < e->Count; i++) {
        if (strcmp(e->Syms[i], k->Sym) == 0) {
            lval_free(e->Vals[i]);
            e->Vals[i] = lval_copy(v);

            return;
        }
    }

    // NOTE(daniel): otherwise, create a new entry
    e->Count++;
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
        case LVAL_SEXPR: case LVAL_QEXPR: {
            for (int i = 0; i < v->Count; i++) {
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
        case LVAL_SEXPR: case LVAL_QEXPR: {
            x->Count = v->Count;
            x->Cell = malloc(sizeof(lval*) * x->Count);

            for (int i = 0; i < x->Count; i++) {
                x->Cell[i] = lval_copy(v->Cell[i]);
            }
        } break;
    }
    
    return x;
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

    lval* v = lval_take(a, 0);

    while (v->Count > 1) lval_free(lval_pop(v, 1));

    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    (void)e;

    LASSERT_COUNT(a, "tail", 1);
    LASSERT_TYPE(a, "tail", 0, LVAL_QEXPR);
    LASSERT(a, a->Cell[0]->Count != 0, 
        "Function 'head' passed {}");

    lval* v = lval_take(a, 0);

    lval_free(lval_pop(v, 0));

    return v;
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

    for (int i = 0; i < a->Count; i++) {
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

    for (int i = 0; i < Syms->Count; i++) {
        LASSERT(a, (Syms->Cell[i]->Type == LVAL_SYM), 
            "Function 'def' cannot define non-symbol. Got %s, Expected %s.",
            lval_type_name(Syms->Cell[i]->Type), lval_type_name(LVAL_SYM));
    }

    LASSERT(a, Syms->Count == a->Count-1, 
        "Function 'def' cannot define incorrect number of values. Got %i, Expected %i.",
        Syms->Count, a->Count-1);

    for (int i = 0; i < Syms->Count; i++) {
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

    for (int i = 0; i < a->Cell[0]->Count; i++) {
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
    for (int i = 0; i < a->Count; i++) {
        if (a->Cell[i]->Type != LVAL_NUM) {
            LASSERT_TYPE(a, op, i, LVAL_NUM);
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
}

void lenv_add_stdlib(lenv* env, mpc_parser_t* Lispy) {
    char* input = "                                                             \
        (def {fun} (\\ {args body} {def (head args) (\\ (tail args) body)}))    \
        (fun {unpack f xs} {eval (join (list f) xs)})                           \
        (fun {pack f & xs} {f xs})                                              \
    ";

    mpc_result_t r;
    if (mpc_parse("<stdlib>", input, Lispy, &r)) {
        // Convert to s-expression
        lval* result = lval_read(r.output);

        // Evaluate the s-expression
        result = lval_eval(env, result);

        lval_free(result);
        mpc_ast_delete(r.output);
    } else {
        mpc_err_print(r.error);
        mpc_err_delete(r.error);
    }
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
    for (int i = 0; i < v->Count; i++) {
        v->Cell[i] = lval_eval(e, v->Cell[i]);
    }

    // Error checking
    for (int i = 0; i < v->Count; i++) {
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
            symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;         \
            sexpr  : '(' <expr>* ')' ;                          \
            qexpr  : '{' <expr>* '}' ;                          \
            expr   : <number> | <symbol> | <sexpr> | <qexpr> ;  \
            lispy    : /^/ <expr>* /$/ ;                        \
        ", Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    // Print version and exit information
    puts("Lispy Version 0.0.1");
    puts("Press Ctrl+c to Exit\n");

    lenv* env = lenv_new();
    lenv_add_builtins(env);
    lenv_add_stdlib(env, Lispy);

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
            result = lval_eval(env, result);
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
