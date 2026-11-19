#!/bin/bash
#   t NAME INPUT(printf fmt) EXPECTED EXPECTED_EXIT
# The AST dump is squeezed onto one line (newlines/indent -> single spaces) before comparing.
# Reading the dump: "binary 0" = BINARY_ADD, "binary 2" = BINARY_MUL (numbers are the enum values),
# "lists=N" = number of parameter lists, "-> ?" = return type not written.
# For a non-zero EXPECTED_EXIT only the exit code (and no sanitizer report) is checked.
pass=0; fail=0
PARSER="${PARSER_TEST:-./parser_test}"   # `make ptest` points this at the sanitizer build
P='import ifj26.*\n'                      # the mandatory prolog

t() {
  name="$1"; input="$2"; exp="$3"; code="$4"
  printf "$input" | "$PARSER" >/tmp/pout 2>/tmp/perr; rc=$?
  out=$(tr '\n' ' ' </tmp/pout | sed 's/  */ /g; s/ $//')
  if { [ "$out" == "$exp" ] || [ "$code" != "0" ]; } && [ "$rc" == "$code" ] && ! grep -q "Sanitizer\|runtime error" /tmp/perr; then
    pass=$((pass+1))
  else
    fail=$((fail+1)); echo "FAIL: $name (rc=$rc want $code)"; echo "--- got:"; echo "$out"; echo "--- want:"; echo "$exp"; echo "--- stderr:"; head -5 /tmp/perr
  fi
}

# ---------------- program structure ----------------
t "spec example" "$P"'def factorial(n: Int): Int =\n  var result = 1\n  var i: Int = n\n  while i > 1 do\n    result = result * i\n    i = i - 1\n  result\n\n@main def runFoo(): Unit =\n  val n = readInt()\n  print(n)\n' \
'def factorial lists=1 [n:Int] -> Int block var result : ? int 1 var i : Int var n while binary 5 var i int 1 block assign result binary 2 var result var i assign i binary 1 var i int 1 expr var result def runFoo @main lists=1 [] -> Unit block val n : ? call readInt (1 list) args[0]: expr call print (1 list) args[0]: var n' 0
t "empty program" "$P" '' 0
t "comments and blank lines" '// hi\n\n'"$P"'\n// c\ndef f() = 1 // x\n\n' 'def f lists=1 [] -> ? block expr int 1' 0
t "two one-line defs" "$P"'def f() = a\ndef g() = b\n' 'def f lists=1 [] -> ? block expr var a def g lists=1 [] -> ? block expr var b' 0
t "def after block def" "$P"'def f() =\n  while a do\n    b\n  c\ndef g() = d\n' 'def f lists=1 [] -> ? block while var a block expr var b expr var c def g lists=1 [] -> ? block expr var d' 0
t "@main own line" "$P"'@main\ndef main(): Unit = x\n' 'def main @main lists=1 [] -> Unit block expr var x' 0
t "@main same line" "$P"'@main def main(): Unit = x\n' 'def main @main lists=1 [] -> Unit block expr var x' 0
t "no param list" "$P"'def answer: Int = 42\n' 'def answer lists=0 -> Int block expr int 42' 0
t "multiple param lists" "$P"'def add(a: Int)(b: Int): Int = a\ndef o()(v: Int) = v\n' 'def add lists=2 [a:Int] [b:Int] -> Int block expr var a def o lists=2 [] [v:Int] -> ? block expr var v' 0
t "params + trailing comma" "$P"'def f(a: Int, b: String,\n): Unit = a\n' 'def f lists=1 [a:Int,b:String] -> Unit block expr var a' 0
t "extension block" "$P"'extension (v: Int)\n  def twice: Int = v\n  def p(o: Int): Int = o\ndef z() = 1\n' 'def twice receiver(v: Int) lists=0 -> Int block expr var v def p receiver(v: Int) lists=1 [o:Int] -> Int block expr var o def z lists=1 [] -> ? block expr int 1' 0
t "extension one-liner" "$P"'extension (v: String) def l(): Int = 1\n' 'def l receiver(v: String) lists=1 [] -> Int block expr int 1' 0

# ---------------- expressions ----------------
t "prec * over +" "$P"'def f() = 1 + 2 * 3\n' 'def f lists=1 [] -> ? block expr binary 0 int 1 binary 2 int 2 int 3' 0
t "parens" "$P"'def f() = (1 + 2) * 3\n' 'def f lists=1 [] -> ? block expr binary 2 binary 0 int 1 int 2 int 3' 0
t "unary over *" "$P"'def f() = -a * b\n' 'def f lists=1 [] -> ? block expr binary 2 unary 1 var a var b' 0
t "left assoc" "$P"'def f() = 1 - 2 - 3\n' 'def f lists=1 [] -> ? block expr binary 1 binary 1 int 1 int 2 int 3' 0
t "&& over ||" "$P"'def f() = a || b && c\n' 'def f lists=1 [] -> ? block expr binary 11 var a binary 10 var b var c' 0
t "! over ==" "$P"'def f() = !a == b\n' 'def f lists=1 [] -> ? block expr binary 8 unary 2 var a var b' 0
t "rel under eq" "$P"'def f() = a < b == c < d\n' 'def f lists=1 [] -> ? block expr binary 8 binary 4 var a var b binary 4 var c var d' 0
t "unit literal" "$P"'def f() = ()\n' 'def f lists=1 [] -> ? block expr unit' 0
t "nested calls" "$P"'def f() = g(h(1), 2)\n' 'def f lists=1 [] -> ? block expr call g (1 list) args[0]: call h (1 list) args[0]: int 1 int 2' 0
t "member on literal" "$P"'def f() = "abc".length\n' 'def f lists=1 [] -> ? block expr member .length string "abc"' 0
t "member + method chain" "$P"'def f() = a.b.c(1)(2)\n' 'def f lists=1 [] -> ? block expr call c (2 lists) receiver: member .b var a args[0]: int 1 args[1]: int 2' 0
t "trailing comma in call" "$P"'def f() = g(1,\n  2,\n)\n' 'def f lists=1 [] -> ? block expr call g (1 list) args[0]: int 1 int 2' 0
t "if expression chain" "$P"'def f() = if a then 1 else if b then 2 else 3\n' 'def f lists=1 [] -> ? block expr if-expr var a block expr int 1 block expr if-expr var b block expr int 2 block expr int 3' 0
t "if expression multi-line" "$P"'def f() =\n  val b = if x < 0 then\n    0 - x\n  else\n    x\n  b\n' 'def f lists=1 [] -> ? block val b : ? if-expr binary 4 var x int 0 block expr binary 1 int 0 var x block expr var x expr var b' 0
t "if expression else-block" "$P"'def f() =\n  val c = if x < 0 then 0 - x else\n      x\n  c\n' 'def f lists=1 [] -> ? block val c : ? if-expr binary 4 var x int 0 block expr binary 1 int 0 var x block expr var x expr var c' 0

# ---------------- statements ----------------
t "decls assign return" "$P"'def f() =\n  val x: Int = 1\n  var y = 2.5\n  x = 3\n  return x\n' 'def f lists=1 [] -> ? block val x : Int int 1 var y : ? double 2.5 assign x int 3 return var x' 0
t "bare return" "$P"'def f(): Unit =\n  return\n' 'def f lists=1 [] -> Unit block return' 0
t "semicolons" "$P"'def f() =\n  a; b\n  c\n' 'def f lists=1 [] -> ? block expr var a expr var b expr var c' 0
t "if/else if/else" "$P"'def f(x: Int) =\n  if x < 0 then\n    a\n  else if x == 0 then\n    b\n  else\n    c\n  d\n' 'def f lists=1 [x:Int] -> ? block if binary 4 var x int 0 block expr var a block if binary 8 var x int 0 block expr var b block expr var c expr var d' 0
t "if without else" "$P"'def f(x: Int) =\n  if x then a\n  y\n' 'def f lists=1 [x:Int] -> ? block if var x block expr var a expr var y' 0
t "inline then, else below" "$P"'def f(x: Int) =\n  if x then a\n  else b\n' 'def f lists=1 [x:Int] -> ? block if var x block expr var a block expr var b' 0
t "while inline" "$P"'def f() =\n  while a do b\n  c\n' 'def f lists=1 [] -> ? block while var a block expr var b expr var c' 0
t "for inline" "$P"'def f() =\n  for i <- 1 to 3 do print(i)\n' 'def f lists=1 [] -> ? block for i to int 1 int 3 block expr call print (1 list) args[0]: var i' 0
t "for until by guard" "$P"'def f() =\n  for i <- 0 until n by 2 if i != 3 do x\n' 'def f lists=1 [] -> ? block for i until int 0 var n by: int 2 guard: binary 9 var i int 3 block expr var x' 0
t "for multi-line" "$P"'def f() =\n  for\n    i <- 0 until 9\n    if i != 3\n  do\n    x\n' 'def f lists=1 [] -> ? block for i until int 0 int 9 guard: binary 9 var i int 3 block expr var x' 0

# ---------------- interpolated strings ----------------
t "istr simple" "$P"'def f() = s"a$b${c}d"\n' 'def f lists=1 [] -> ? block expr interpolated text "a" var b var c text "d"' 0
t "istr empty" "$P"'def f() = s""\n' 'def f lists=1 [] -> ? block expr interpolated text ""' 0
t "istr dollar dollar" "$P"'def f() = s"$$5"\n' 'def f lists=1 [] -> ? block expr interpolated text "$5"' 0
t "istr nested" "$P"'def f() = s"a${s"b$c"}d"\n' 'def f lists=1 [] -> ? block expr interpolated text "a" interpolated text "b" var c text "d"' 0
t "istr in call" "$P"'def f() = print(s"n = ${n}")\n' 'def f lists=1 [] -> ? block expr call print (1 list) args[0]: interpolated text "n = " var n' 0
t "istr then more code" "$P"'def f() =\n  val a = s"x$y"\n  a\n' 'def f lists=1 [] -> ? block val a : ? interpolated text "x" var y expr var a' 0

# ---------------- errors: syntax (2) ----------------
t "no prolog" 'def f() = 1\n' '' 2
t "empty file" '' '' 2
t "wrong prolog" 'import ifj27.*\n' '' 2
t "prolog twice" "$P$P" '' 2
t "statement at top level" "$P"'x = 1\n' '' 2
t "chained <" "$P"'def f() = a < b < c\n' '' 2
t "chained ==" "$P"'def f() = a == b == c\n' '' 2
t "if expr needs else" "$P"'def f() = if a then 1\n' '' 2
t "empty block" "$P"'def f() =\ng\n' '' 2
t "missing )" "$P"'def f(a: Int = 1\n' '' 2
t "param without type" "$P"'def f(a) = 1\n' '' 2
t "val without init" "$P"'def f() =\n  val x\n' '' 2
t "trailing comma same line" "$P"'def f() = g(1, 2,)\n' '' 2
t "two statements one line" "$P"'def f() =\n  a b\n' '' 2
t "stray else" "$P"'def f() =\n  else\n' '' 2
t "bad range word" "$P"'def f() =\n  for i <- 1 till 3 do\n    x\n' '' 2
t "dot without member" "$P"'def f() = a.\n' '' 2
t "garbage after body" "$P"'def f() = 1 2\n' '' 2
t "def inside block" "$P"'def f() =\n  def g() = 1\n  g\n' '' 2
t "extension without colon" "$P"'extension (x Int)\n  def a: Int = 1\n' '' 2
t "@main without def" "$P"'@main\n' '' 2
t "unfinished nested" "$P"'def f() =\n  if a then\n    while b do\n      x = \n' '' 2
t "over-indented line" "$P"'def f() =\n  a\n    b\n' '' 2
t "unclosed call" "$P"'def f() = g(1,\n' '' 2

# ---------------- errors: lexical (1) are passed through ----------------
t "lexical error" "$P"'def f() = 1 # 2\n' '' 1
t 'unclosed dollar-brace' "$P"'def f() = s"${a\n' '' 1
t "tab indent" "$P"'def f() =\n\ta\n' '' 1

# ---------------- spec edge cases (prolog, layout, commas, nesting) ----------------
t "prolog with odd spacing + comment" 'import   ifj26 . *   // c\ndef f() = 1\n' 'def f lists=1 [] -> ? block expr int 1' 0
t "block comment before prolog" '/* c */ import ifj26.*\ndef f() = 1\n' 'def f lists=1 [] -> ? block expr int 1' 0
t "code on the prolog line" 'import ifj26.*  def f() = 1\n' '' 2
t "no trailing newline (1-line def)" "$P"'def f() = 1' 'def f lists=1 [] -> ? block expr int 1' 0
t "no trailing newline (block)" "$P"'def f() =\n  a\n  b' 'def f lists=1 [] -> ? block expr var a expr var b' 0
t "@main, blank + comment, def" "$P"'@main\n\n// c\ndef main(): Unit = x\n' 'def main @main lists=1 [] -> Unit block expr var x' 0
t "empty param list with comma" "$P"'def f(,) = 1\n' '' 2
t "param trailing comma, same line" "$P"'def f(a: Int, ) = 1\n' '' 2
t "param trailing comma + comment" "$P"'def f(a: Int, // c\n) = 1\n' 'def f lists=1 [a:Int] -> ? block expr int 1' 0
t "empty arg list with comma" "$P"'def f() = g(,)\n' '' 2
t "arg trailing comma + comment" "$P"'def f() = g(1, // c\n)\n' 'def f lists=1 [] -> ? block expr call g (1 list) args[0]: int 1' 0
t "double comma" "$P"'def f() = g(1,,\n2)\n' '' 2
t "expression cannot continue on next line" "$P"'def f() = 1 +\n  2\n' '' 2
t "assignment is not an expression" "$P"'def f() =\n  x = y = 1\n' '' 2
t "dangling else goes to outer if" "$P"'def f() =\n  if a then\n    if b then\n      x\n  else\n    z\n' 'def f lists=1 [] -> ? block if var a block if var b block expr var x block expr var z' 0
t "nested if both with else" "$P"'def f() =\n  if a then\n    if b then\n      x\n    else\n      y\n  else\n    z\n' 'def f lists=1 [] -> ? block if var a block if var b block expr var x block expr var y block expr var z' 0
t "else misaligned (inside block)" "$P"'def f() =\n  if a then\n    b\n    else\n    c\n' '' 2
t "else-block on next line after inline then" "$P"'def f() =\n  if a then b else\n    c\n' 'def f lists=1 [] -> ? block if var a block expr var b block expr var c' 0
t "if expr: block then, inline else" "$P"'def f() =\n  val v = if c then\n    1\n  else 2\n  v\n' 'def f lists=1 [] -> ? block val v : ? if-expr var c block expr int 1 block expr int 2 expr var v' 0
t "if expr as argument" "$P"'def f() = g(if a then 1 else 2)\n' 'def f lists=1 [] -> ? block expr call g (1 list) args[0]: if-expr var a block expr int 1 block expr int 2' 0
t "return if-expr" "$P"'def f(): Int =\n  return if a then 1 else 2\n' 'def f lists=1 [] -> Int block return if-expr var a block expr int 1 block expr int 2' 0
t "spec: else with ()" "$P"'def f() =\n  if x < 5 then\n    print("a")\n  else\n    ()\n' 'def f lists=1 [] -> ? block if binary 4 var x int 5 block expr call print (1 list) args[0]: string "a" block expr unit' 0
t "while inline assignment" "$P"'def f() =\n  while i < 3 do i = i + 1\n' 'def f lists=1 [] -> ? block while binary 4 var i int 3 block assign i binary 0 var i int 1' 0
t "def without parens, no type" "$P"'def f = 1\n' 'def f lists=0 -> ? block expr int 1' 0
t "5.toString" "$P"'def f() = 5.toString\n' 'def f lists=1 [] -> ? block expr member .toString int 5' 0
t "1.5.floor" "$P"'def f() = 1.5.floor\n' 'def f lists=1 [] -> ? block expr member .floor double 1.5' 0
t "x.twice() is a call, x.twice a member" "$P"'def f() = x.twice()\n' 'def f lists=1 [] -> ? block expr call twice (1 list) receiver: var x args[0]:' 0
t "member on istr" "$P"'def f() = s"abc".length\n' 'def f lists=1 [] -> ? block expr member .length interpolated text "abc"' 0
t "istr \$id then text" "$P"'def f() = s"$a.c"\n' 'def f lists=1 [] -> ? block expr interpolated var a text ".c"' 0
t "istr string literal inside expr" "$P"'def f() = s"${"x"}"\n' 'def f lists=1 [] -> ? block expr interpolated string "x"' 0
t "istr call inside expr" "$P"'def f() = s"${f(1)}"\n' 'def f lists=1 [] -> ? block expr interpolated call f (1 list) args[0]: int 1' 0
t "istr newline inside expr" "$P"'def f() = s"${a +\n b}"\n' 'def f lists=1 [] -> ? block expr interpolated binary 0 var a var b' 0
t "for with expression bounds, negative step" "$P"'def f() =\n  for i <- a + 1 to f(2) by -1 do x\n' 'def f lists=1 [] -> ? block for i to binary 0 var a int 1 call f (1 list) args[0]: int 2 by: unary 1 int 1 block expr var x' 0
t "for: guard indented too far" "$P"'def f() =\n  for\n    i <- 0 to 9\n      if i > 1\n  do\n    x\n' '' 2
t "for: do not dedented" "$P"'def f() =\n  for\n    i <- 0 to 9\n    do\n    x\n' '' 2
t "for: do dedented too far" "$P"'def f() =\n  for\n    i <- 0 to 9\n    if a\n   do\n    x\n' '' 1
# very deep nesting must be a syntax error, never a crash
deep() { yes "$1" | head -n "$2" | tr -d '\n'; }
t "900 nested parens ok" "$P"'def f() = '"$(deep '(' 900)"'1'"$(deep ')' 900)"'\n' "$(printf 'def f lists=1 [] -> ? block expr int 1')" 0
t "100000 nested parens" "$P"'def f() = '"$(deep '(' 100000)"'1'"$(deep ')' 100000)"'\n' '' 2
t "100000 unary minus" "$P"'def f() = '"$(deep '-' 100000)"'1\n' '' 2
t "100000 else-if expr" "$P"'def f() = '"$(deep 'if a then 1 else ' 100000)"'2\n' '' 2

echo "passed=$pass failed=$fail"
