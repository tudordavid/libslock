#!/bin/bash


if [ $# -lt 4 ];
then
    echo "Usage: ./$@ \"RANGE\" APPLICATION [PARAMETERS]";
    echo " e.g., ./$@ \"1 6 12 18\" stress_test -l1 -a0 -d1000";
    exit;
fi;

cores="$1";
shift;
app=$1;
shift;

for c in $cores;
do
    printf "%-4d" $c;
    ./$app $@ -n$c
done;
