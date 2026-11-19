/**
 * @file ast.h
 * @brief Abstract syntax tree
 *
 * Project: Implementace překladače jazyka IFJ26.
 * Academic year: 2026/2027
 *
 * @author Vojtěch Kabelka (xkabelv00)
 * @author Martin Kolouch (xkoloum00)
 * @author Ondřej Vondra (274921)
 * @author Filip Kachyňa (xkachyf00)
 */

#ifndef AST_H
#define AST_H

// ========================================== //
// =============== INCLUDES ================= //
// ========================================== //

// Standardní knihovny:
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

// ========================================== //
// ======== CONSTANTS AND TYPEDEFS ========== //
// ========================================== //

#define AST_INITIAL_CAPACITY 4 

/** Predeclarations */
typedef struct AST_EXPRESSION AST_EXPRESSION;
typedef struct AST_STATEMENT AST_STATEMENT;
typedef struct AST_BLOCK AST_BLOCK;
typedef struct AST_FUNCTION_DEF AST_FUNCTION_DEF;

// ========================================== //
// ================ ENUMS =================== //
// ========================================== //

/** @brief Return code of AST module. */
typedef enum {
  AST_OK,             /**< Success. */
  AST_INVALID_PARAM,    /**< Invalid parameter. */
  AST_INTERNAL_ERROR  /**< Failed memory allocation. */
} t_ast_result;


/** @brief Data types of IFJ26. */
typedef enum {
  TYPE_UNSPECIFIED, 
  TYPE_INT,
  TYPE_DOUBLE,
  TYPE_STRING,
  TYPE_BOOLEAN,
  TYPE_UNIT,
  TYPE_ANY // ADDED FOR EXTENSION FUNCTIONS BY SEMANTICS
} ENUM_DATA_TYPE;


/** @brief Implicit conversion added by semantics. */
typedef enum { 
  CONVERSION_NONE, 
  CONVERSION_INT_TO_DOUBLE 
} ENUM_CONVERSION;


/** @brief Statement type. */
typedef enum {
  STMT_VAR_DECL,
  STMT_ASSIGNMENT,
  STMT_RETURN,
  STMT_IF,
  STMT_WHILE,
  STMT_FOR, 
  STMT_EXPR
} ENUM_STATEMENT_TYPE;


/** @brief EXPRESSION type. */
typedef enum {
  EXPR_VAR_REF,
  EXPR_INT_LITERAL,
  EXPR_DOUBLE_LITERAL,
  EXPR_STRING_LITERAL,
  EXPR_UNIT_LITERAL,
  EXPR_UNARY,
  EXPR_BINARY,
  EXPR_BOOL_LITERAL,  
  EXPR_CALL,          
  EXPR_MEMBER,       
  EXPR_INTERPOLATED,
  EXPR_IF        
} ENUM_EXPRESSION_TYPE;


/** @brief Unary operators. */
typedef enum { 
  UNARY_PLUS, 
  UNARY_MINUS, 
  UNARY_NOT 
} ENUM_UNARY_OPERATOR;


/** @brief Binary operators. */
typedef enum {
  BINARY_ADD, 
  BINARY_SUB, 
  BINARY_MUL, 
  BINARY_DIV,
  BINARY_LT, 
  BINARY_GT, 
  BINARY_LE, 
  BINARY_GE, 
  BINARY_EQ, 
  BINARY_NE,
  BINARY_AND, 
  BINARY_OR 
} ENUM_BINARY_OPERATOR;


/** @brief Callee function kind. */
typedef enum { 
  CALLEE_UNKNOWN, 
  CALLEE_USER_FUNCTION, 
  CALLEE_BUILTIN, 
  CALLEE_EXTENSION 
} ENUM_CALLEE_KIND;


/** @brief Member kind. */
typedef enum {
  MEMBER_UNKNOWN,
  MEMBER_TO_STRING,   
  MEMBER_TO_INT,     
  MEMBER_FLOOR,       
  MEMBER_LENGTH,      
  MEMBER_EXT_PROPERTY 
} ENUM_MEMBER_KIND;

// ========================================== //
// ============== STRUCTURES ================ //
// ========================================== //

/** @brief Position in source code. */
typedef struct {
  size_t line;
  size_t column;
} SOURCE_LOCATION;


/** @brief Function parameter. */
typedef struct {
  char *name;
  size_t variable_id;   /**< Semantics fill */
  SOURCE_LOCATION location;
  ENUM_DATA_TYPE type;
} AST_PARAMETER;


/** @brief One list of parameters; count == 0 => (). */
typedef struct {
  AST_PARAMETER *parameters;
  size_t parameter_count;
  size_t parameter_capacity;
} AST_PARAM_LIST;


/** @brief One list of arguments in parentheses; count == 0 => (). */
typedef struct {
  AST_EXPRESSION **arguments;
  size_t argument_count;
  size_t argument_capacity;
} AST_ARG_LIST;


/** @brief Block of statements. */
struct AST_BLOCK {
  AST_STATEMENT **statements;
  size_t statement_count;
  size_t statement_capacity;
  ENUM_DATA_TYPE resolved_type; /**< Semantics fill */
};


/** @brief Function definition. */
struct AST_FUNCTION_DEF {
  char *name;
  size_t function_id; /**< Semantics fill */
  bool is_main;
  SOURCE_LOCATION location;
  AST_PARAM_LIST *param_lists;
  size_t param_list_count;
  size_t param_list_capacity;
  AST_PARAMETER *receiver;              /**< NULL = common function */
  ENUM_DATA_TYPE declared_return_type;  /**< TYPE_UNSPECIFIED if not written */
  ENUM_DATA_TYPE resolved_return_type;  /**< Semantics fill */
  AST_BLOCK *body;
};


/** @brief Whole program. */
typedef struct {
  AST_FUNCTION_DEF **functions;
  size_t function_count;
  size_t function_capacity;
} AST_PROGRAM;

// ---------- Statements ---------- //

typedef struct {
  char *name;
  size_t variable_id;
  bool is_mutable; /**< val = false, var = true. */
  ENUM_DATA_TYPE declared_type;
  ENUM_DATA_TYPE resolved_type;
  AST_EXPRESSION *initializer;
} AST_VAR_DECL;


typedef struct {
  char *name;
  size_t variable_id; /**< Semantics fill */
  AST_EXPRESSION *value;
} AST_ASSIGNMENT;


typedef struct {
  AST_EXPRESSION *value; 
} AST_RETURN;


typedef struct {
  AST_EXPRESSION *condition;
  AST_BLOCK *then_block;
  AST_BLOCK *else_block;        
  ENUM_DATA_TYPE resolved_type; /**<  Semantics fill */
} AST_IF;


typedef struct {
  AST_EXPRESSION *condition;
  AST_BLOCK *body;
} AST_WHILE;


typedef struct { 
  char *var_name;
  size_t variable_id;
  bool is_exclusive; /**< true = until, false = to. */
  AST_EXPRESSION *from;
  AST_EXPRESSION *to;
  AST_EXPRESSION *step; 
  AST_EXPRESSION **guards;
  size_t guard_count;
  size_t guard_capacity;
  AST_BLOCK *body;
} AST_FOR;


typedef struct {
  AST_EXPRESSION *expression;
} AST_EXPR_STMT;


struct AST_STATEMENT {
  ENUM_STATEMENT_TYPE type;
  SOURCE_LOCATION location;
  union {
    AST_VAR_DECL var_decl;
    AST_ASSIGNMENT assignment;
    AST_RETURN return_stmt;
    AST_IF if_stmt;
    AST_WHILE while_stmt;
    AST_FOR for_stmt;
    AST_EXPR_STMT expr_stmt;
  } data;
};

// ---------- Expressions ---------- //

typedef struct {
  char *name;
  size_t variable_id;
} AST_VAR_REF;


typedef struct {
  char *value;
  size_t length;
} AST_STRING_LITERAL;


typedef struct {
  ENUM_UNARY_OPERATOR op;
  AST_EXPRESSION *operand;
} AST_UNARY_EXPRESSION;


typedef struct {
  ENUM_BINARY_OPERATOR op;
  AST_EXPRESSION *left;
  AST_EXPRESSION *right;
} AST_BINARY_EXPRESSION;


typedef struct {
  ENUM_CALLEE_KIND callee_kind;
  char *name;
  AST_EXPRESSION *receiver;     
  AST_ARG_LIST *argument_lists; 
  size_t argument_list_count;   
  size_t argument_list_capacity;
  size_t function_id;
} AST_CALL;


typedef struct {
  AST_EXPRESSION *receiver;
  char *member_name;
  ENUM_MEMBER_KIND member_kind;
  size_t function_id;
} AST_MEMBER;


typedef struct {
  bool is_literal;
  char *text;                  
  AST_EXPRESSION *expression;  
} AST_INTERP_PART;


typedef struct {
  AST_INTERP_PART *parts;
  size_t part_count;
  size_t part_capacity;
} AST_INTERPOLATED;


struct AST_EXPRESSION {
  ENUM_EXPRESSION_TYPE type;
  ENUM_DATA_TYPE resolved_type; /**< Semantics fill */
  ENUM_CONVERSION conversion;   /**< Semantics fill */
  SOURCE_LOCATION location;
  union {
    AST_VAR_REF var_ref;
    int64_t int_literal;
    double double_literal;
    AST_STRING_LITERAL string_literal;
    AST_UNARY_EXPRESSION unary;
    AST_BINARY_EXPRESSION binary;
    bool bool_literal;
    AST_CALL call;
    AST_MEMBER member;
    AST_INTERPOLATED interpolated;
    AST_IF if_expr;
  } data;
};


// ========================================== //
// ===== DECLARATION OF PUBLIC FUNCTIONS ==== //
// ========================================== //

// ---------- Tree ---------- //

/**
 * @brief Creates empty tree (program).
 * @return New tree, NULL if allocation failed.
 */
AST_PROGRAM *ast_program_create(void);

/**
 * @brief Add function def into array of functions.
 * @param[in,out] program Tree.
 * @param[in] function Function to add.
 * @return Function result.
 * @retval AST_OK.
 * @retval AST_INVALID_PAR.
 * @retval AST_INTERNAL_ERROR.
 */
t_ast_result ast_program_add_function(AST_PROGRAM *program, AST_FUNCTION_DEF *function);

/**
 * @brief Deletes whole tree.
 * @param[in] program Tree.
 */
void ast_program_destroy(AST_PROGRAM *program);

// ---------- Functions ---------- //

/**
 * @brief Creates a function def.
 * @param[in] name Function name.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_FUNCTION_DEF *ast_function_def_create(const char *name, SOURCE_LOCATION location);

/**
 * @brief Sets receiver of function extension.
 * @param[in,out] function Function.
 * @param[in] name Name of receiver.
 * @param[in] type Type of receiver.
 * @param[in] location Position in source.
 * @return Function result.
 */
t_ast_result ast_function_def_set_receiver(AST_FUNCTION_DEF *function, const char *name,
                                           ENUM_DATA_TYPE type, SOURCE_LOCATION location);

/**
 * @brief Add new empty paramlist.
 * @param[in,out] function Function.
 * @return Function result.
 */
t_ast_result ast_function_def_add_param_list(AST_FUNCTION_DEF *function);

/**
 * @brief Add parameter into last paramlist of function.
 * @param[in,out] function Function.
 * @param[in] name Name of parameter.
 * @param[in] type Type of parameter.
 * @param[in] location Position in source.
 * @return Function result.
 */
t_ast_result ast_function_def_add_parameter(AST_FUNCTION_DEF *function, const char *name,
                                            ENUM_DATA_TYPE type, SOURCE_LOCATION location);

/**
 * @brief Deletes definition of function including params and block.
 * @param[in] function Function.
 */
void ast_function_def_destroy(AST_FUNCTION_DEF *function);

// ---------- Block ---------- //

/**
 * @brief Creates empty block.
 * @return New block, NULL if failed.
 */
AST_BLOCK *ast_block_create(void);

/**
 * @brief Adds new statement at the end of block.
 * @param[in,out] block Block.
 * @param[in] statement Statement.
 * @return Function result.
 */
t_ast_result ast_block_add_statement(AST_BLOCK *block, AST_STATEMENT *statement);

/**
 * @brief Deletes block including statements.
 * @param[in] block Block.
 */
void ast_block_destroy(AST_BLOCK *block);

// ---------- Statements ---------- //

/**
 * @brief Creates declaration of val/var.
 * @param[in] name Name of variable.
 * @param[in] is_mutable true = var, false = val.
 * @param[in] declared_type Declared type or TYPE_UNSPECIFIED.
 * @param[in] initializer Inicialization expression.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_STATEMENT *ast_statement_create_var_decl(const char *name, bool is_mutable,
                                             ENUM_DATA_TYPE declared_type,
                                             AST_EXPRESSION *initializer, SOURCE_LOCATION location);

/**
 * @brief Creates an ASSIGNMENT.
 * @param[in] name Name of variable.
 * @param[in] value Assigned expression.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_STATEMENT *ast_statement_create_assignment(const char *name, AST_EXPRESSION *value,
                                               SOURCE_LOCATION location);

/**
 * @brief Creates statement RETURN.
 * @param[in] value Returned expression, NULL = "return".
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_STATEMENT *ast_statement_create_return(AST_EXPRESSION *value, SOURCE_LOCATION location);

/**
 * @brief Creates statement IF.
 * @param[in] condition Condition.
 * @param[in] then_block Then branch.
 * @param[in] else_block Else branch, NULL if without else.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_STATEMENT *ast_statement_create_if(AST_EXPRESSION *condition, AST_BLOCK *then_block,
                                       AST_BLOCK *else_block, SOURCE_LOCATION location);

/**
 * @brief Creates statement WHILE.
 * @param[in] condition Condition.
 * @param[in] body Body of cycle.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_STATEMENT *ast_statement_create_while(AST_EXPRESSION *condition, AST_BLOCK *body,
                                          SOURCE_LOCATION location);

/**
 * @brief Creates statement FOR without guards.
 * @param[in] var_name Loop control variable.
 * @param[in] is_exclusive true = until, false = to.
 * @param[in] from Starting value.
 * @param[in] to Ending value.
 * @param[in] step Step.
 * @param[in] body Body of cycle.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_STATEMENT *ast_statement_create_for(const char *var_name, bool is_exclusive,
                                        AST_EXPRESSION *from, AST_EXPRESSION *to,
                                        AST_EXPRESSION *step, AST_BLOCK *body,
                                        SOURCE_LOCATION location);

/**
 * @brief Adds guard into FOR.
 * @param[in,out] statement Node type of STMT_FOR.
 * @param[in] guard Bool expression.
 * @return Function result.
 * @retval AST_INVALID_PAR Node is not STMT_FOR.
 */
t_ast_result ast_statement_for_add_guard(AST_STATEMENT *statement, AST_EXPRESSION *guard);

/**
 * @brief Creates statement EXPRESSION.
 * @param[in] expression EXPRESSION.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_STATEMENT *ast_statement_create_expr(AST_EXPRESSION *expression, SOURCE_LOCATION location);

/**
 * @brief Deletes statement including subtree.
 * @param[in] statement Statement.
 */
void ast_statement_destroy(AST_STATEMENT *statement);

// ---------- Expressions ---------- //

/** @brief Creates Int literal.
 *  @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_int(int64_t value, SOURCE_LOCATION location);

/** @brief Creates Double literal.
 *  @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_double(double value, SOURCE_LOCATION location);

/**
 * @brief Creates String literal.
 * @param[in] value Data .
 * @param[in] length Length in bytes.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_string(const char *value, size_t length,
                                             SOURCE_LOCATION location);

/** @brief Creates Bool literal.
 *  @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_bool(bool value, SOURCE_LOCATION location);

/** @brief Creates Unit literal ().
 *  @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_unit(SOURCE_LOCATION location);

/** @brief Creates reference on variable.
 *  @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_var_ref(const char *name, SOURCE_LOCATION location);

/**
 * @brief Creates Unary expression.
 * @param[in] op Operator.
 * @param[in] operand Operand.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_unary(ENUM_UNARY_OPERATOR op, AST_EXPRESSION *operand,
                                            SOURCE_LOCATION location);

/**
 * @brief Creates Binary expression.
 * @param[in] op Operator.
 * @param[in] left Left operand.
 * @param[in] right Right operand.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_binary(ENUM_BINARY_OPERATOR op, AST_EXPRESSION *left,
                                             AST_EXPRESSION *right, SOURCE_LOCATION location);

/**
 * @brief Creates Call expression without args list.
 * @param[in] name Function name.
 * @param[in] receiver Receiver, otherwise NULL.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_call(const char *name, AST_EXPRESSION *receiver,
                                           SOURCE_LOCATION location);

/**
 * @brief Adds empty args list "()" into Call.
 * @param[in,out] call Type of node EXPR_CALL.
 * @return Function result.
 * @retval AST_INVALID_PAR Node is not EXPR_CALL.
 */
t_ast_result ast_expression_call_add_argument_list(AST_EXPRESSION *call);

/**
 * @brief Add argument into LAST args list of Call.
 * @param[in,out] call Type of node EXPR_CALL.
 * @param[in] argument Expression of argument.
 * @return Function result.
 */
t_ast_result ast_expression_call_add_argument(AST_EXPRESSION *call, AST_EXPRESSION *argument);

/**
 * @brief Rewrites EXPR_VAR_REF on EXPR_CALL without args list.
 * Typically calls semantics.
 * @param[in,out] expression Type of node EXPR_VAR_REF.
 * @return Function result.
 * @retval AST_INVALID_PAR Node is not EXPR_VAR_REF.
 */
t_ast_result ast_expression_var_ref_to_call(AST_EXPRESSION *expression);

/**
 * @brief Creates an access to member expression.id without parentheses.
 * @param[in] receiver Receiver.
 * @param[in] member_name Name of member.
 * @param[in] location Posiiton in source.
 * @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_member(AST_EXPRESSION *receiver, const char *member_name,
                                             SOURCE_LOCATION location);

/** @brief Creates empty interpolated String.
 *  @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_interpolated(SOURCE_LOCATION location);

/**
 * @brief Adds into interpolated String literal part.
 * @param[in,out] expression Type of node EXPR_INTERPOLATED.
 * @param[in] text Text.
 * @return Function result.
 */
t_ast_result ast_expression_interpolated_add_literal(AST_EXPRESSION *expression, const char *text);

/**
 * @brief Adds into interpolated String expression part.
 * @param[in,out] expression Type of node EXPR_INTERPOLATED.
 * @param[in] part Expression.
 * @return Function result.
 */
t_ast_result ast_expression_interpolated_add_expression(AST_EXPRESSION *expression,
                                                        AST_EXPRESSION *part);

/**
 * @brief Creates condition expression IF ... else ...
 * @param[in] condition Condition.
 * @param[in] then_block Then branch.
 * @param[in] else_block Else branch - compulsory.
 * @param[in] location Position in source.
 * @return New node, NULL if failed.
 */
AST_EXPRESSION *ast_expression_create_if(AST_EXPRESSION *condition, AST_BLOCK *then_block,
                                         AST_BLOCK *else_block, SOURCE_LOCATION location);

/**
 * @brief Deletes expression including subtree.
 * @param[in] expression Expression.
 */
void ast_expression_destroy(AST_EXPRESSION *expression);

#endif // AST_H