#!/bin/bash


if [ $# -lt 4 ];
then
    echo "Usage: ./$@ FROM_NUM_CORES STEP_NUM_CORES TO_NUM_CORES APPLICATION [PARAMETERS]";
    exit;
fi;

lc=$1;
shift;
step=$1;
shift;
hc=$1;
shift;
app=$1;
shift;

for c in $(seq $lc $step $hc);
do
    printf "%-4d" $c;
    ./$app $@ -n$c
done;
