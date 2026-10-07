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
#include "token.h"

// ========================================== //
// ===== PRIVATE CONSTANTS AND TYPEDEFS ===== //
// ========================================== //

static const t_token_match keywords[] = {
    {"def", TOKEN_DEF},
    {"do", TOKEN_DO},
    {"Double", TOKEN_DOUBLE_TYPE},
    {"else", TOKEN_ELSE},
    {"if", TOKEN_IF},
    {"import", TOKEN_IMPORT},
    {"Int", TOKEN_INT_TYPE},
    {"return", TOKEN_RETURN},
    {"String", TOKEN_STRING_TYPE},
    {"then", TOKEN_THEN},
    {"Unit", TOKEN_UNIT},
    {"val", TOKEN_VAL},
    {"var", TOKEN_VAR},
    {"while", TOKEN_WHILE}
};

/*
static const t_token_match operators[] = {
    {"+", TOKEN_PLUS},
    {"-", TOKEN_MINUS},
    {"*", TOKEN_MULTIPLY},
    {"/", TOKEN_DIVIDE},
    {"<", TOKEN_LESS},
    {"<=", TOKEN_LESS_EQUAL},
    {">", TOKEN_GREATER},
    {">=", TOKEN_GREATER_EQUAL},
    {"==", TOKEN_EQUAL_EQUAL},
    {"!=", TOKEN_NOT_EQUAL}
};

static const t_token_match special_chars[] = {
    {"=", TOKEN_ASSIGN},
    {":", TOKEN_COLON},
    {"@", TOKEN_AT},
    {".", TOKEN_DOT},
    {"(", TOKEN_LPAREN},
    {")", TOKEN_RPAREN},
    {",", TOKEN_COMMA},
    {";", TOKEN_SEMICOLON}
};
*/

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
static void lexer_char_advance(t_lexer *lexer);

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

/**
 * @brief Determines the type of a word.
 *
 * @param[in] word The word to be checked.
 * @param[out] type The type of the word.
 *
 * @return 0 on success, 1 on error.
 */
static int lexer_get_word_type(t_token *token);

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

    return lexer_get_word_type(token);
}

static int lexer_get_word_type(t_token *token) {
    size_t keyword_count = sizeof(keywords) / sizeof(keywords[0]);

    for (size_t i = 0; i < keyword_count; i++) {
        if (strcmp(token->lexeme, keywords[i].text) == 0) {
            token->type = keywords[i].type;
            return 0;
        }
    }

    token->type = TOKEN_IDENTIFIER;
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
            return 0;

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