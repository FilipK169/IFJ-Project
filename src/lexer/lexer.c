/**
* @file template.c
* @brief Lexer implementation
*
* Project: Implementace překladače jazyka IFJ26.
* Academic year: 2026/2027
*
* @author Vojtěch Kabelka (xkabelv00)
* @author Martin Kolouch (xkoloum00)
* @author Ondřej Vondra (x274921)
* @author Filip Kachyňa (xkachyf00)
*/

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

#include "lexer.h"

#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>

// ========================================== //
// ===== PRIVATE CONSTANTS AND TYPEDEFS ===== //
// ========================================== //

// ========================================== //
// ============ PRIVATE ENUMS =============== //
// ========================================== //

// ========================================== //
// ========== PRIVATE STRUCTURES ============ //
// ========================================== //

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} t_lexeme_buffer;

// ========================================== //
// === DECLARATION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

/**
 * @brief Gets the next character from the source file.
 *
 * @param[in,out] lexer Pointer to the lexer structure.
 *
 * @return The next character from the source file, or EOF if the end of the file is reached.
 */
static void lexer_char_advance(t_lexeme *lexer);

/**
 * @brief Initializes a lexeme buffer.
 *
 * @param[in,out] buffer Pointer to the buffer to be initialized.
 *
 * @return 0 on success, 1 on error.
 */
static int lexeme_buffer_init(t_lexeme_buffer *buffer);

/**
 * @brief Destroys a lexeme buffer.
 *
 * @param[in,out] buffer Pointer to the buffer to be destroyed.
 */
static void lexeme_buffer_destroy(t_lexeme_buffer *buffer);

/**
 * @brief Appends a character to a lexeme buffer.
 *
 * @param[in,out] buffer Pointer to the buffer.
 * @param[in] c Character to be appended.
 *
 * @return 0 on success, 1 on error.
 */
static int lexeme_buffer_append(t_lexeme_buffer *buffer, char c);

/**
 * @brief Resizes a buffer.
 *
 * @param[in,out] buffer Pointer to a buffer.
 * @param[in,out] capacity Pointer to the capacity of a buffer.
 *
 * @return 0 on success, 1 on error.
 */
static int lexeme_buffer_resize(t_lexeme_buffer *buffer);

/**
 * @brief Scans a word (identifier or keyword) from the source.
 *
 * @param[in,out] lexer Pointer to the lexer structure.
 * @param[out] token Pointer to the token structure to be filled.
 *
 * @return 0 on success, 1 on error.
 */
static int lexer_scan_word(t_lexer *lexer, t_token *token);

static int lexer_get_word_type(const char *word, t_token_type *type);

// ========================================== //
// ==== DEFINITION OF PRIVATE FUNCTIONS ===== //
// ========================================== //

static void lexer_char_advance(t_lexer *lexer) {
    lexer->current_char = fgetc(lexer->source);
}

static int lexeme_buffer_init(t_lexeme_buffer *buffer) {
    buffer->capacity = 16;
    buffer->length = 0;
    buffer->data = malloc(buffer->capacity);

    if (buffer->data == NULL) {
        return 1;
    }
    buffer->data[0] = '\0';

    return 0;
}

static void lexeme_buffer_destroy(t_lexeme_buffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;

    return 0;
}

static int lexeme_buffer_append(t_lexeme_buffer *buffer, char c) {
    if (buffer->length + 1 >= buffer->capacity) {
        if (lexeme_buffer_resize(buffer) != 0) {
            return 1;
        }
    }

    buffer->data[buffer->length] = c;
    buffer->length++;
    buffer->data[buffer->length] = '\0';

    return 0;
}

static int lexeme_buffer_resize(t_lexeme_buffer *buffer) {
    size_t new_capacity = buffer->capacity * 2;

    char *new_buffer = realloc(buffer->data, new_capacity);

    if (new_buffer == NULL) {
        return 1;
    }

    buffer->data = new_buffer;
    buffer->capacity = new_capacity;

    return 0;
}

static int lexer_scan_word(t_lexer *lexer, t_token *token) {
    t_lexeme_buffer buffer;

    if (lexeme_buffer_init(&buffer) != 0) {
        return 1;
    }

    while (isalnum(lexer->current_char) || lexer->current_char == '_') {
        if (lexeme_buffer_append(&buffer, (char)lexer->current_char) != 0) {
            lexeme_buffer_destroy(&buffer);
            return 1;
        }

        lexer_char_advance(lexer);
    }

    token->lexeme = buffer.data;

    return 0;
}

static int lexer_get_word_type(const char *word, t_token_type *type) {
    if (strcmp(word, "def") == 0) {
        *type = TOKEN_DEF;
    } else if (strcmp(word, "do") == 0) {
        *type = TOKEN_DO;
    } else if (strcmp(word, "Double") == 0) {
        *type = TOKEN_DOUBLE_TYPE;
    } else if (strcmp(word, "else") == 0) {
        *type = TOKEN_ELSE;
    } else if (strcmp(word, "if") == 0) {
        *type = TOKEN_IF;
    } else if (strcmp(word, "import") == 0) {
        *type = TOKEN_IMPORT;
    } else if (strcmp(word, "Int") == 0) {
        *type = TOKEN_INT_TYPE;
    } else if (strcmp(word, "return") == 0) {
        *type = TOKEN_RETURN;
    } else if (strcmp(word, "String") == 0) {
        *type = TOKEN_STRING_TYPE;
    } else if (strcmp(word, "then") == 0) {
        *type = TOKEN_THEN;
    } else if (strcmp(word, "Unit") == 0) {
        *type = TOKEN_UNIT;
    } else if (strcmp(word, "val") == 0) {
        *type = TOKEN_VAL;
    } else if (strcmp(word, "var") == 0) {
        *type = TOKEN_VAR;
    } else if (strcmp(word, "while") == 0) {
        *type = TOKEN_WHILE;
    } else {
        *type = TOKEN_IDENTIFIER;
    }

    return 0;
}

// ========================================== //
// ===== DEFINITION OF PUBLIC FUNCTIONS ===== //
// ========================================== //

void lexer_init(t_lexer *lexer, FILE *source) {
    lexer->source = source;
    lexer_char_advance(lexer);
    lexer->at_line_start = true;
}

int lexer_token_advance(t_lexer *lexer, t_token *token) {
    while (lexer->current_char == ' ') {
        lexer_char_advance(lexer);
    }

    switch (lexer->current_char) {
        case '\n':
            lexer_char_advance(lexer);
            lexer->at_line_start = true;

            token->type = TOKEN_EOL;
            token->lexeme = NULL;
            return 0;

        case '\t':
            if (lexer->at_line_start) {
                return 1;
            }

            lexer_char_advance(lexer);
            break;

        case EOF:
            token->type = TOKEN_EOF;
            token->lexeme = NULL;
            return 0;

        default:
            if (isalpha(lexer->current_char) || lexer->current_char == '_') {
                return lexer_scan_word(lexer, token);
            }

            return 1;
    }
}


void token_destroy(t_token *token) {
    free(token->lexeme);
    token->lexeme = NULL;
}