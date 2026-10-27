
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

#define MAX_LINE 1000000

typedef enum {
    TOK_NUMBER,
    TOK_PLUS,
    TOK_MINUS,
    TOK_MUL,
    TOK_DIV,
    TOK_POW,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_END,
    TOK_INVALID
} TokenType;

typedef struct {
    TokenType type;
    double value;
    int position;
} Token;

static const char *input;
static size_t pos;
static Token current;
static int error_pos;

static void next_token(void) {
    while (isspace((unsigned char)input[pos])) {
        pos++;
    }

    current.position = (int)pos;
    current.value = 0;

    char c = input[pos];

    if (c == '\0') {
        current.type = TOK_END;
        return;
    }

    if (isdigit((unsigned char)c) || c == '.') {
        char *end;
        errno = 0;
        current.value = strtod(input + pos, &end);

        if (end == input + pos || errno == ERANGE ||
            !isfinite(current.value)) {
            current.type = TOK_INVALID;
            return;
        }

        pos = (size_t)(end - input);
        current.type = TOK_NUMBER;
        return;
    }

    pos++;

    switch (c) {
        case '+': current.type = TOK_PLUS; return;
        case '-': current.type = TOK_MINUS; return;
      case '*':
    if (input[pos] == '*') {
        pos++;
        current.type = TOK_POW;
    } else {
        current.type = TOK_MUL;
    }
    return;
        case '/': current.type = TOK_DIV; return;
        case '^': current.type = TOK_POW; return;
        case '(': current.type = TOK_LPAREN; return;
        case ')': current.type = TOK_RPAREN; return;
        default:
            current.type = TOK_INVALID;
            return;
    }
}

static void fail(void) {
    if (error_pos == 0) {
        error_pos = current.position + 1;
    }
}

static double parse_expr(void);

static double parse_primary(void) {
    if (current.type == TOK_NUMBER) {
        double value = current.value;
        next_token();
        return value;
    }

    if (current.type == TOK_LPAREN) {
        next_token();
        double value = parse_expr();

        if (current.type != TOK_RPAREN) {
            fail();
            return 0;
        }

        next_token();
        return value;
    }

    fail();
    return 0;
}

static double parse_unary(void) {
    if (current.type == TOK_PLUS) {
        next_token();
        return parse_unary();
    }

    if (current.type == TOK_MINUS) {
        next_token();
        return -parse_unary();
    }

    return parse_primary();
}

static double parse_power(void) {
    double left = parse_unary();

    if (current.type == TOK_POW) {
        next_token();
        double right = parse_power();

        if (right < 0 && floor(right) != right) {
            fail();
            return 0;
        }

        if (left == 0 && right < 0) {
            fail();
            return 0;
        }

        left = pow(left, right);

        if (!isfinite(left)) {
            fail();
            return 0;
        }
    }

    return left;
}

static double parse_term(void) {
    double value = parse_power();

    while (current.type == TOK_MUL ||
           current.type == TOK_DIV) {
        TokenType op = current.type;
        next_token();
        double right = parse_power();

        if (op == TOK_DIV && right == 0) {
            fail();
            return 0;
        }

        if (op == TOK_MUL) {
            value *= right;
        } else {
            value /= right;
        }

        if (!isfinite(value)) {
            fail();
            return 0;
        }
    }

    return value;
}

static double parse_expr(void) {
    double value = parse_term();

    while (current.type == TOK_PLUS ||
           current.type == TOK_MINUS) {
        TokenType op = current.type;
        next_token();
        double right = parse_term();

        if (op == TOK_PLUS) {
            value += right;
        } else {
            value -= right;
        }

        if (!isfinite(value)) {
            fail();
            return 0;
        }
    }

    return value;
}

static int evaluate(const char *text, double *result) {
    input = text;
    pos = 0;
    error_pos = 0;

    next_token();

    if (current.type == TOK_END) {
        return 0;
    }

    *result = parse_expr();

    if (error_pos == 0 && current.type != TOK_END) {
        fail();
    }

    if (error_pos != 0) {
        return error_pos;
    }

    return 0;
}

static void output_error(FILE *out, int position) {
    fprintf(out, "ERROR:%d\n", position);
}

int main(int argc, char *argv[]) {
    const char *input_dir = NULL;
    const char *output_dir = NULL;
    int output_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 ||
            strcmp(argv[i], "--dir") == 0) {
            if (++i >= argc || input_dir != NULL) {
                fprintf(stderr, "Invalid arguments\n");
                return 1;
            }
            input_dir = argv[i];
        } else if (strcmp(argv[i], "-o") == 0 ||
                   strcmp(argv[i], "--output-dir") == 0) {
            if (++i >= argc || output_dir != NULL) {
                fprintf(stderr, "Invalid arguments\n");
                return 1;
            }
            output_dir = argv[i];
            output_mode = 1;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Invalid arguments\n");
            return 1;
        } else {
            if (input_dir != NULL) {
                fprintf(stderr, "Invalid arguments\n");
                return 1;
            }
            input_dir = argv[i];
        }
    }

    if (input_dir == NULL) {
        fprintf(stderr, "Input file or directory required\n");
        return 1;
    }

    FILE *in = fopen(input_dir, "r");
    if (in == NULL) {
        fprintf(stderr, "Cannot open input file\n");
        return 1;
    }

    FILE *out = stdout;

    if (output_mode) {
        out = fopen(output_dir, "w");
        if (out == NULL) {
            fclose(in);
            fprintf(stderr, "Cannot open output file\n");
            return 1;
        }
    }

    char *line = malloc(MAX_LINE);
    if (line == NULL) {
        fclose(in);
        if (out != stdout) fclose(out);
        return 1;
    }

    while (fgets(line, MAX_LINE, in) != NULL) {
        size_t len = strlen(line);

        if (len > 0 && line[len - 1] == '\n') {
            line[--len] = '\0';
        }

        int comment = 0;
        for (size_t i = 0; i < len; i++) {
            if (line[i] == '#') {
                comment = 1;
                break;
            }
        }

        if (comment) {
            continue;
        }

        int blank = 1;
        for (size_t i = 0; i < len; i++) {
            if (!isspace((unsigned char)line[i])) {
                blank = 0;
                break;
            }
        }

        if (blank) {
            continue;
        }

        double result = 0;
        int error = evaluate(line, &result);

        if (error != 0) {
            output_error(out, error);
        } else if (floor(result) == result &&
                   result >= -9223372036854775808.0 &&
                   result < 9223372036854775808.0) {
            fprintf(out, "%.0f\n", result);
        } else {
            fprintf(out, "%.10g\n", result);
        }
    }

    free(line);
    fclose(in);

    if (out != stdout) {
        fclose(out);
    }

    return 0;
}
