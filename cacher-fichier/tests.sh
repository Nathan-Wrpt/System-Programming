#!/bin/bash

PROG="./cacher"
FILE="$PROG.c"

OUTPUT=/tmp/$$
mkdir $OUTPUT

######################################

echo -n "test 01 - coding style: "

! which clang-format > /dev/null && echo "KO -> please install clang-format" && exit 1
! clang-format --style='{BasedOnStyle: llvm, IndentWidth: 4, ColumnLimit: 80, BreakBeforeBraces: Linux }' --dry-run -Werror $FILE && exit 1

echo ".........................OK"

######################################

echo "test with option -c"

######################################

echo -n "test 02 - program usage: "

touch $OUTPUT/toto ; mkdir $OUTPUT/toto.d

$PROG -c $OUTPUT/abcd > /dev/null 2> $OUTPUT/stderr  && echo "KO -> exit status == 0 if source file not found"                          && exit 1
! [ -s $OUTPUT/stderr ]                              && echo "KO -> no error message on stderr if source file not found"                && exit 1

$PROG -c  $OUTPUT/toto > /dev/null 2> $OUTPUT/stderr && echo "KO -> exit status == 0 if destination directory already exists"           && exit 1
! [ -s $OUTPUT/stderr ]                              && echo "KO -> no error message on stderr if destination directory already exists" && exit 1

rmdir $OUTPUT/toto.d

echo "........................OK"

######################################

echo -n "test 03 - program output with empty source file: "

$PROG -c $OUTPUT/toto > /dev/null 2>&1
! [ -d $OUTPUT/toto.d ] && echo "KO -> destination directory not created" && exit 1
RES=$(ls $OUTPUT/toto.d)
! [ -z $RES ] && echo "KO -> destination directory not empty" && exit 1

rmdir $OUTPUT/toto.d

echo "OK"

######################################

echo -n "test 04 - program output with small file: "

printf "abcdefgh" > $OUTPUT/toto

$PROG -c $OUTPUT/toto > /dev/null 2>&1
! [ -d $OUTPUT/toto.d/0/abcdefgh ] && echo "KO -> data is missing" && exit 1

echo ".......OK"

######################################

echo -n "test 05 - program output with large file: "

LC_ALL=C tr -dc "A-Za-z0-9" < /dev/urandom | head -c 4093 > $OUTPUT/titi

$PROG -c $OUTPUT/titi > /dev/null 2>&1
for i in `seq 0 5`; do ls -lR $OUTPUT/titi.d/$i | grep ^d | tr -s ' ' | cut -d ' ' -f9 >> $OUTPUT/tmp ; done
tr -d '\n' < $OUTPUT/tmp > $OUTPUT/output
! cmp -s $OUTPUT/titi $OUTPUT/output && echo "KO -> data is missing" && exit 1

rm -r $OUTPUT/titi.d

echo ".......OK"

######################################

echo -n "test 06 - memory error: "

! which valgrind > /dev/null && echo "KO -> please install valgrind" && exit 1

# mixing ASan and Valgrind is not yet supported
gcc -Wall -Werror -Wextra -pedantic -g $FILE -o $PROG

valgrind --leak-check=full --error-exitcode=100 --trace-children=yes --log-file=$OUTPUT/valgrind.log $PROG -c $OUTPUT/titi > /dev/null 2>&1
[ "$?" == "100" ] && echo "KO -> memory pb please check $OUTPUT/valgrind.log" && exit 1

# restore exe with asan
scons -c > /dev/null ; scons > /dev/null

echo ".........................OK"

######################################

echo "test with option -x"

######################################

echo -n "test 07 - program failure if dirname not found: "

$PROG -x $OUTPUT/x > /dev/null 2> $OUTPUT/stderr && echo "KO -> exit status == 0"           && exit 1
! [ -s $OUTPUT/stderr ]                          && echo "KO -> no error message on stderr" && exit 1

echo ".OK"

######################################

echo -n "test 08 - program output with small file: "

touch $OUTPUT/output
printf "abcdefgh" > $OUTPUT/tata ; mkdir -p $OUTPUT/tata.d/0/abcdefgh

! $PROG -x $OUTPUT/tata.d $OUTPUT/output > /dev/null 2>&1 && echo "KO -> exit status != 0"                                                                     && exit 1
! cmp -s $OUTPUT/tata $OUTPUT/output                      && echo "KO -> missing data, check files \"$OUTPUT/output\" (yours) and \"$OUTPUT/tata\" (expected)" && exit 1

echo ".......OK"

######################################

echo -n "test 09 - program output with large file: "

LC_ALL=C tr -dc "A-Za-z0-9" < /dev/urandom | head -c 1000000 > $OUTPUT/tutu

! $PROG -c $OUTPUT/tutu                  > /dev/null 2>&1 && echo "KO -> exit status != 0" && exit 1

! $PROG -x $OUTPUT/tutu.d $OUTPUT/output > /dev/null 2>&1 && echo "KO -> exit status != 0" && exit 1
! cmp -s $OUTPUT/tutu $OUTPUT/output                      && echo "KO -> missing data, check files \"$OUTPUT/output\" (yours) and \"$OUTPUT/tutu\" (expected)" && exit 1

echo ".......OK"

######################################

echo -n "test 10 - memory error: "

! which valgrind > /dev/null && echo "KO -> please install valgrind" && exit 1

# mixing ASan and Valgrind is not yet supported
gcc -Wall -Werror -Wextra -pedantic -g $FILE -o $PROG

valgrind --leak-check=full --error-exitcode=100 --trace-children=yes --log-file=$OUTPUT/valgrind.log $PROG -x $OUTPUT/tutu.d $OUTPUT/output > /dev/null 2>&1
[ "$?" == "100" ] && echo "KO -> memory pb please check $OUTPUT/valgrind.log" && exit 1

# restore exe with asan
scons -c > /dev/null ; scons > /dev/null

echo ".........................OK"

######################################

rm -r $OUTPUT
