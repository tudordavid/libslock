#!/bin/bash
case "$1" in
opteron) echo "running tests on opteron"
    THE_LOCKS="HCLH TTAS ARRAY MCS TICKET HTICKET MUTEX SPINLOCK CLH"
    num_cores=48
    platform_def="-DOPTERON"
    make="make"
    freq=2100000000
    platform=opteron
    prog_prefix="numactl --physcpubind=0 ../"
;;
opteron_optimize) echo "running tests on opteron"
    THE_LOCKS="HCLH TTAS ARRAY MCS TICKET HTICKET MUTEX SPINLOCK CLH"
    num_cores=48
    optimize="-DOPTERON_OPTIMIZE"
    platform_def="-DOPTERON"
    make="make"
    freq=2100000000
    platform=opteron
    prog_prefix="numactl --physcpubind=0 ../"
;;
xeon) echo "running tests on xeon"
    THE_LOCKS="HCLH TTAS ARRAY MCS TICKET HTICKET MUTEX SPINLOCK CLH"
    num_cores=80
    platform_def="-DXEON"
    freq=2130000000
    make="make"
    platform=xeon
    prog_prefix="numactl --physcpubind=1 ../"
;;
niagara) echo "running tests on niagara"
    THE_LOCKS="TTAS ARRAY MCS TICKET MUTEX SPINLOCK CLH"
    ALTERNATE=-DALTERNATE_SOCKETS
    num_cores=64
    platform_def="-DSPARC"
    freq=1200000000
    make="make"
    platform=niagara
    prog_prefix="../"
;;
tilera) echo "running tests on tilera"
    THE_LOCKS="TTAS ARRAY MCS TICKET MUTEX SPINLOCK CLH"
    num_cores=36
    platform_def="-DTILERA"
    freq=1200000000
    make="make"
    platform=tilera
    prog_prefix="../run ../"
;;
*) echo "Program format ./run_all platform, where plafrom in opteron, xeon, niagara, tilera"
    exit;
;;
esac

rm correctness_array.out

for prefix in ${THE_LOCKS}
do
cd ..; LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;
echo ${prefix} >> correctness_array.out
${prog_prefix}test_array_alloc -n ${num_cores} -d 1000 >> correctness_array.out
done

