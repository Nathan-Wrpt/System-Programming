#!/bin/bash

PROG="./calculatrice"
FILE="$PROG.c"

OUTPUT=/tmp/$$
mkdir $OUTPUT

######################################

echo -n "test 01 - coding style: "

! which clang-format > /dev/null && echo "KO -> please install clang-format" && exit 1
! clang-format --style='{BasedOnStyle: llvm, IndentWidth: 4, ColumnLimit: 80, BreakBeforeBraces: Linux}' --dry-run -Werror $FILE && exit 1

echo ".........................OK"

######################################

echo -n "test 02 - fork failure: "

! grep "case -1:" $FILE > /dev/null && echo "KO -> no \"case -1\" found for fork ()" && exit 1

echo ".........................OK"

######################################

echo -n "test 03 - program creates 4 child processes: "

! grep "sleep" $FILE > /dev/null && echo "KO -> child should call sleep(2) before using pipe" && exit 1
$PROG 5 > /dev/null 2>&1 &
PID=$!

sleep 1

pgrep -P $PID > $OUTPUT/childs
NB=$(cat $OUTPUT/childs | wc -l)
[ $NB -ne 4 ] && echo "KO -> $NB child processes while 4 expected" && exit 1

wait $PID

echo "....OK"

######################################

echo -n "test 04 - program supports addition: "

echo "30" > $OUTPUT/output1
! $PROG 10 + 20 > $OUTPUT/output2 2> /dev/null && echo "KO -> exit code !=0" && exit 1
! cmp -s $OUTPUT/output1 $OUTPUT/output2  && echo "KO -> output differs, check \"$OUTPUT/output2\" (yours) and \"$OUTPUT/output1\" (expected)" && exit 1

echo "............OK"

######################################

echo -n "test 05 - program supports substraction: "

echo "-10" > $OUTPUT/output1
! $PROG 10 - 20 > $OUTPUT/output2 2> /dev/null && echo "KO -> exit code !=0" && exit 1
! cmp -s $OUTPUT/output1 $OUTPUT/output2  && echo "KO -> output differs, check \"$OUTPUT/output2\" (yours) and \"$OUTPUT/output1\" (expected)" && exit 1

echo "........OK"

######################################

echo -n "test 06 - program supports multiplication: "

echo "200" > $OUTPUT/output1
! $PROG 50 \* 4 > $OUTPUT/output2 2> /dev/null && echo "KO -> exit code !=0" && exit 1
! cmp -s $OUTPUT/output1 $OUTPUT/output2  && echo "KO -> output differs, check \"$OUTPUT/output2\" (yours) and \"$OUTPUT/output1\" (expected)" && exit 1

echo "......OK"

######################################

echo -n "test 07 - program supports division: "

echo "256" > $OUTPUT/output1
! $PROG 1024 / 4 > $OUTPUT/output2 2> /dev/null && echo "KO -> exit code !=0" && exit 1
! cmp -s $OUTPUT/output1 $OUTPUT/output2  && echo "KO -> output differs, check \"$OUTPUT/output2\" (yours) and \"$OUTPUT/output1\" (expected)" && exit 1

echo "............OK"

######################################

echo -n "test 08 - combination of operands and operators: "

echo "45" > $OUTPUT/output1
! $PROG 10 - 100 \* -2 / 4 > $OUTPUT/output2 2> /dev/null && echo "KO -> exit code !=0" && exit 1
! cmp -s $OUTPUT/output1 $OUTPUT/output2  && echo "KO -> output differs, check \"$OUTPUT/output2\" (yours) and \"$OUTPUT/output1\" (expected)" && exit 1

echo "OK"

######################################

echo -n "test 09 - program with unknown operand: "

$PROG 42 x 3 > $OUTPUT/stdout 2> $OUTPUT/stderr  && echo "KO -> exit code = 0"                && exit 1
[ -s $OUTPUT/stdout ]                            && echo "KO -> output detected on stdout"    && exit 1
! [ -s $OUTPUT/stderr ]                          && echo "KO -> no output detected on stderr" && exit 1

echo ".........OK"

######################################

echo -n "test 10 - test of children termination: "

$PROG 5 > $OUTPUT/stdout 2> $OUTPUT/stderr &
PID=$!

sleep 1

kill -s SIGTERM `pgrep -P $PID | head -1` 
wait $PID

[ $? -ne "1" ]          && echo "KO -> exit status != 1"             && exit 1 
[ -s $OUTPUT/stdout ]   && echo "KO -> output detected on stdout"    && exit 1
! [ -s $OUTPUT/stderr ] && echo "KO -> no output detected on stderr" && exit 1

echo ".........OK"

######################################

echo -n "test 11 - memory error: "

! which valgrind > /dev/null && echo "KO -> please install valgrind" && exit 1

# mixing ASan and Valgrind is not yet supported
gcc -Wall -Werror -Wextra -pedantic -g $FILE -o $PROG

valgrind --leak-check=full --error-exitcode=100 --trace-children=yes --log-file=$OUTPUT/valgrind.log $PROG 10 - 100 \* -2 / 4 > /dev/null 2>&1
[ "$?" == "100" ] && echo "KO -> memory pb please check $OUTPUT/valgrind.log" && exit 1

# restore exe with asan
scons -c > /dev/null ; scons > /dev/null

echo ".........................OK"

######################################

rm -r $OUTPUT
