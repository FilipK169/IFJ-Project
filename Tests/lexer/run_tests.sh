#!/bin/bash
pass=0; fail=0
LEXER="${LEXER_TEST:-./lexer_test}"   # `make test` points this at the sanitizer build
# t NAME INPUT(printf fmt) EXPECTED_STDOUT EXPECTED_EXIT
t() {
  name="$1"; input="$2"; exp="$3"; code="$4"
  out=$(printf "$input" | "$LEXER" 2>/tmp/err); rc=$?
  if { [ "$out" == "$exp" ] || [ "$code" == "1" ]; } && [ "$rc" == "$code" ] && ! grep -q "Sanitizer\|runtime error" /tmp/err; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); echo "FAIL: $name (rc=$rc want $code)"; echo "--- got:"; echo "$out"; echo "--- want:"; echo "$exp"; echo "--- stderr:"; head -5 /tmp/err
  fi
}
t "guide example" 'def f(): Int =\n  val a = 1\n  if a < 2 then\n    a\n  else\n    2\n' \
'KW_DEF ID(f) LEFT_PAREN RIGHT_PAREN COLON KW_INT ASSIGN EOL
INDENT KW_VAL ID(a) ASSIGN INT_LIT(1) EOL
KW_IF ID(a) LESS INT_LIT(2) KW_THEN EOL
INDENT ID(a) EOL
DEDENT KW_ELSE EOL
INDENT INT_LIT(2) EOL
DEDENT DEDENT EOF' 0
t "no trailing newline" 'a\n  b' 'ID(a) EOL
INDENT ID(b) EOL
DEDENT EOF' 0
t "blank+comment lines" '\n\na\n\n  // c\n\nb\n' 'ID(a) EOL
ID(b) EOL
EOF' 0
t "parens newline" 'f(1,\n  2)\ng\n' 'ID(f) LEFT_PAREN INT_LIT(1) COMMA INT_LIT(2) RIGHT_PAREN EOL
ID(g) EOL
EOF' 0
t "block comment eol" 'a /* x\ny */\nb\n' 'ID(a) EOL
ID(b) EOL
EOF' 0
t "token after ml comment" 'a /* x\ny */ b\n' 'ID(a)' 1
t "inconsistent indent" 'a\n    b\n  c\n' 'ID(a) EOL
INDENT ID(b) EOL' 1
t "tab indent" 'a\n\tb\n' 'ID(a) EOL' 1
t "unmatched paren" 'a)\n' 'ID(a)' 1
t "annot main" '@main\ndef main(): Unit =\n  x.y\n' 'ANNOT_MAIN EOL
KW_DEF ID(main) LEFT_PAREN RIGHT_PAREN COLON KW_UNIT ASSIGN EOL
INDENT ID(x) DOT ID(y) EOL
DEDENT EOF' 0
t "annot same line" '@main def main(): Unit =\n  x\n' 'ANNOT_MAIN KW_DEF ID(main) LEFT_PAREN RIGHT_PAREN COLON KW_UNIT ASSIGN EOL
INDENT ID(x) EOL
DEDENT EOF' 0
t "import" 'import ifj26.*\n' 'KW_IMPORT ID(ifj26) DOT MULTIPLY EOL
EOF' 0
t "@mai" '@mai' '' 1
t "@mainx" '@mainx' '' 1
t "@ main" '@ main' '' 1
t "numbers" '12.5 .5 1e10 1.5e-3 2E+4 3.0d 1_0.2_5 7 .' 'DOUBLE_LIT(12.5) DOUBLE_LIT(0.5) DOUBLE_LIT(1e+10) DOUBLE_LIT(0.0015) DOUBLE_LIT(20000) DOUBLE_LIT(3) DOUBLE_LIT(10.25) INT_LIT(7) DOT EOL
EOF' 0
t "1." '1.' 'INT_LIT(1) DOT EOL
EOF' 0
for bad in '1e' '1e+' '1.5d4' '9d' '1.5x' '00.5' '1e999' '0123' '1_' '1__0' '12abc' '99999999999999999999' '9ad4' '_ '; do
  t "bad num $bad" "$bad" '' 1
done
t "strings" '"hello" "" "a b" "q\\"q" "back\\\\slash" "x\\ny"\n' 'STRING_LIT(hello) STRING_LIT() STRING_LIT(a b) STRING_LIT(q"q) STRING_LIT(back\\slash) STRING_LIT(x\ny) EOL
EOF' 0
t "str unterminated nl" '"abc\n' '' 1
t "str unterminated eof" '"abc' '' 1
t "str bad escape" '"a\\qb"\n' '' 1
t "str tab" '"a\tb"\n' '' 1
t "ml basic" '"""abc"""\n' 'STRING_LIT(abc) EOL
EOF' 0
t "ml empty" '""""""\n' 'STRING_LIT() EOL
EOF' 0
t "ml quotes" '"""say "hi" and ""ok"" """\n' 'STRING_LIT(say "hi" and ""ok"" ) EOL
EOF' 0
t "ml block" 'a\n  """x\ny"""\nb\n' 'ID(a) EOL
INDENT STRING_LIT(x\ny) EOL
DEDENT ID(b) EOL
EOF' 0
t "ml unterminated" '"""never' '' 1
t "ml two quotes" '"""a""\n' '' 1
t "istr plain" 's"hello" s"" s"cost $$5 \\"q\\" x\\ty"\n' 'ISTR_TEXT(hello) ISTR_TEXT() ISTR_TEXT(cost $5 "q" x\ty) EOL
EOF' 0
t "istr s ident" 'val m = s"x"\ns + 1\nsx"a"\ns "b"\n' 'KW_VAL ID(m) ASSIGN ISTR_TEXT(x) EOL
ID(s) PLUS INT_LIT(1) EOL
ID(sx) STRING_LIT(a) EOL
ID(s) STRING_LIT(b) EOL
EOF' 0
t "istr teammate" 's"$course$year: ${count + 1} examples cost $$0."\n' 'ISTR_TEXT() ISTR_ID(course) ISTR_TEXT() ISTR_ID(year) ISTR_TEXT(: ) ISTR_EXPR_BEGIN ID(count) PLUS INT_LIT(1) ISTR_EXPR_END ISTR_TEXT( examples cost $0.) EOL
EOF' 0
t "istr simple" 's"$x"\ns"${x}"\ns"a$b.c"\n' 'ISTR_TEXT() ISTR_ID(x) ISTR_TEXT() EOL
ISTR_TEXT() ISTR_EXPR_BEGIN ID(x) ISTR_EXPR_END ISTR_TEXT() EOL
ISTR_TEXT(a) ISTR_ID(b) ISTR_TEXT(.c) EOL
EOF' 0
t "istr nested" 's"a${s"b$c"}d"\n' 'ISTR_TEXT(a) ISTR_EXPR_BEGIN ISTR_TEXT(b) ISTR_ID(c) ISTR_TEXT() ISTR_EXPR_END ISTR_TEXT(d) EOL
EOF' 0
t "istr in call" 'print(s"n = ${n}")\n' 'ID(print) LEFT_PAREN ISTR_TEXT(n = ) ISTR_EXPR_BEGIN ID(n) ISTR_EXPR_END ISTR_TEXT() RIGHT_PAREN EOL
EOF' 0
for bad in 's"$"' 's"$1"' 's"$_"' '}' 's"${x' 's"abc' 's"a\\qb"'; do
  t "bad istr $bad" "$bad\n" '' 1
done
t "istr ml spec" 'print(s"""He said "hello", $name.\n Second line.""")\n' 'ID(print) LEFT_PAREN ISTR_TEXT(He said "hello", ) ISTR_ID(name) ISTR_TEXT(.\n Second line.) RIGHT_PAREN EOL
EOF' 0
t "istr ml empty" 's""""""\n' 'ISTR_TEXT() EOL
EOF' 0
t "istr ml expr" 's"""cost $$5 ${1+1}\n"""\n' 'ISTR_TEXT(cost $5 ) ISTR_EXPR_BEGIN INT_LIT(1) PLUS INT_LIT(1) ISTR_EXPR_END ISTR_TEXT(\n) EOL
EOF' 0
t "istr ml noescape" 's"""a\\nb"""\n' 'ISTR_TEXT(a\\nb) EOL
EOF' 0
t "istr ml block" 'a\n  s"""x\ny"""\nb\n' 'ID(a) EOL
INDENT ISTR_TEXT(x\ny) EOL
DEDENT ID(b) EOL
EOF' 0
t "istr ml unterminated" 's"""abc' '' 1
# ---- polish / edge cases ----
t "empty file" '' 'EOF' 0
t "only newlines" '\n\n\n' 'EOF' 0
t "only comment" '// hi' 'EOF' 0
t "only block comment" '/* a\nb */' 'EOF' 0
t "operators" '= == < <= <- > >= ! != && | || + - * / ( ) , ; : .\n' 'ASSIGN EQUAL LESS LESS_EQUAL LEFT_ARROW GREATER GREATER_EQUAL NOT NOT_EQUAL AND UNION OR PLUS MINUS MULTIPLY DIVIDE LEFT_PAREN RIGHT_PAREN COMMA SEMICOLON COLON DOT EOL
EOF' 0
t "keywords" 'def do Double else if import Int return String then Unit val var while true false Boolean for int Val\n' 'KW_DEF KW_DO KW_DOUBLE KW_ELSE KW_IF KW_IMPORT KW_INT KW_RETURN KW_STRING KW_THEN KW_UNIT KW_VAL KW_VAR KW_WHILE KW_TRUE KW_FALSE KW_BOOLEAN KW_FOR ID(int) ID(Val) EOL
EOF' 0
t "crlf" 'a\r\n  b\r\nc\r\n' 'ID(a) EOL
INDENT ID(b) EOL
DEDENT ID(c) EOL
EOF' 0
t "triple dedent eof" 'a\n  b\n    c\n      d' 'ID(a) EOL
INDENT ID(b) EOL
INDENT ID(c) EOL
INDENT ID(d) EOL
DEDENT DEDENT DEDENT EOF' 0
t "unknown char" 'a # b\n' 'ID(a)' 1
t "non ascii" 'a \xc3\xa9\n' 'ID(a)' 1
t "lone amp" '+ & +' 'PLUS' 1
t "unterminated block" 'a /* never closed' 'ID(a)' 1
t "nested block" '/* /* */' '' 1
t "5-3" '5-3\n' 'INT_LIT(5) MINUS INT_LIT(3) EOL
EOF' 0
t "x<-5" 'x<-5\n' 'ID(x) LEFT_ARROW INT_LIT(5) EOL
EOF' 0
t "comment after code" 'a // c\nb\n' 'ID(a) EOL
ID(b) EOL
EOF' 0
t "indented comment between blocks" 'a\n  b\n    // c\n  d\n' 'ID(a) EOL
INDENT ID(b) EOL
ID(d) EOL
DEDENT EOF' 0
t "ml comment in line" 'a /* c */ b\n' 'ID(a) ID(b) EOL
EOF' 0
# ---- extra edge cases from the IFJ26 spec (section 1.1 / 1.2) ----
t "num underscores" '1_000_000 1e1_0 1.5_0 1.5d 1e5D .5e3d 0e5 0.0 0' 'INT_LIT(1000000) DOUBLE_LIT(1e+10) DOUBLE_LIT(1.5) DOUBLE_LIT(1.5) DOUBLE_LIT(100000) DOUBLE_LIT(500) DOUBLE_LIT(0) DOUBLE_LIT(0) INT_LIT(0) EOL
EOF' 0
t "5.D is int dot id" '5.D' 'INT_LIT(5) DOT ID(D) EOL
EOF' 0
for bad in '0_1' '1_e5' '1e_5' '1.5_' '0d'; do
  t "bad num $bad" "$bad" '' 1
done
t "trailing comma + semicolon" 'f(a,\n  b,\n)\nx = 1; y = 2\n' 'ID(f) LEFT_PAREN ID(a) COMMA ID(b) COMMA RIGHT_PAREN EOL
ID(x) ASSIGN INT_LIT(1) SEMICOLON ID(y) ASSIGN INT_LIT(2) EOL
EOF' 0
t "comment separates tokens" '1/**/2 a/*x*/b\n' 'INT_LIT(1) INT_LIT(2) ID(a) ID(b) EOL
EOF' 0
t "nested comment ok" '/* a /* b */ c */ x\n' 'ID(x) EOL
EOF' 0
t "ml comment then line comment" 'a /* c\n */ // d\nb\n' 'ID(a) EOL
ID(b) EOL
EOF' 0
t "ml comment at line start" '/* x\n */ a\n' '' 1
t "ml comment mid-block" 'a\n  /* x\n */ b\n' '' 1
t "comment in parens" 'f(a, // c\n  /* d */ b)\n' 'ID(f) LEFT_PAREN ID(a) COMMA ID(b) RIGHT_PAREN EOL
EOF' 0
t "whitespace-only line" 'a\n   \n  b\n' 'ID(a) EOL
INDENT ID(b) EOL
DEDENT EOF' 0
t "dedent multi" 'a\n  b\n    c\nd\n' 'ID(a) EOL
INDENT ID(b) EOL
INDENT ID(c) EOL
DEDENT DEDENT ID(d) EOL
EOF' 0
t "dedent to middle" 'a\n  b\n    c\n  d\n' 'ID(a) EOL
INDENT ID(b) EOL
INDENT ID(c) EOL
DEDENT ID(d) EOL
DEDENT EOF' 0
t "dedent to unseen" 'a\n  b\n    c\n e\n' '' 1
t "indent first line" '  a\n' 'INDENT ID(a) EOL
DEDENT EOF' 0
t "space then tab" 'a\n \tb\n' '' 1
t "ident lookalikes" 'define iff Int2 _x x_1 __ Do DEF\n' 'ID(define) ID(iff) ID(Int2) ID(_x) ID(x_1) ID(__) ID(Do) ID(DEF) EOL
EOF' 0
t "string escapes" '"\\t\\r\\\\n"\n' 'STRING_LIT(\t\r\\n) EOL
EOF' 0
t "raw CR in string" '"a\rb"\n' '' 1
t "percent unknown" 'a %% b\n' '' 1
t "dollar in code" 'a $ b\n' '' 1
t "lone brace" '{ x }\n' '' 1
t "@ alone" '@\n' '' 1
t "@main at EOF" '@main' 'ANNOT_MAIN EOL
EOF' 0
t "triple bar" 'a|||b\n' 'ID(a) OR UNION ID(b) EOL
EOF' 0
t "triple amp" 'a&&&b\n' '' 1
t "istr adjacent" 's"$a$b" s"${a}${b}"\n' 'ISTR_TEXT() ISTR_ID(a) ISTR_TEXT() ISTR_ID(b) ISTR_TEXT() ISTR_TEXT() ISTR_EXPR_BEGIN ID(a) ISTR_EXPR_END ISTR_TEXT() ISTR_EXPR_BEGIN ID(b) ISTR_EXPR_END ISTR_TEXT() EOL
EOF' 0
t "istr string inside expr" 's"${"x"}"\n' 'ISTR_TEXT() ISTR_EXPR_BEGIN STRING_LIT(x) ISTR_EXPR_END ISTR_TEXT() EOL
EOF' 0
t "istr newline in expr" 's"${\n1}"\n' 'ISTR_TEXT() ISTR_EXPR_BEGIN INT_LIT(1) ISTR_EXPR_END ISTR_TEXT() EOL
EOF' 0
t "istr ml quotes" 's"""a"" """\n' 'ISTR_TEXT(a"" ) EOL
EOF' 0
t "istr dollar then quote" 's"a$"\n' '' 1
t "tab on blank line" 'x\n\t\ny\n' 'ID(x) EOL
ID(y) EOL
EOF' 0
echo "passed=$pass failed=$fail"
