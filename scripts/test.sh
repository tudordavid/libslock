#!/bin/bash

#-----------------------------------------------------------------------
# SAMPLE SCRIPT RUNNING A NUMBER OF TESTS ON ATOMIC OPERATIONS AND LOCKS
#-----------------------------------------------------------------------

num_repeats=3
run_individual_tests=0
run_atomic_ops=0
run_atomic_latency=0
run_uncontended=0
run_latency_tests=0
run_stress_test=0
run_min_max_stress=1
run_best_stress=1


#------------------------------------------------------
# ATOMIC OPERATIONS
#------------------------------------------------------

num_locks=1
duration=1000
acquire=0
pause=100
pause_atomic=0
individual_ops_pause=0

entries=1

num_max_common=36

case "$1" in
opteron) echo "running tests on opteron"
    THE_LOCKS="HCLH TTAS ARRAY MCS TICKET HTICKET MUTEX SPINLOCK CLH"
    num_cores=48
#    optimize="-DOPTERON_OPTIMIZE"
    platform_def="-DOPTERON"
    make="make"
    freq=2100000000
    platform=opteron
    remote_cores="0 1 6 12 18 24 30 36 42"
    scal_cores="1 6 18 36"
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
    remote_cores="0 1 6 12 18 24 30 36 42"
    scal_cores="1 6 18 36"
    prog_prefix="numactl --physcpubind=0 ../"
;;
xeon) echo "running tests on xeon"
    THE_LOCKS="HCLH TTAS ARRAY MCS TICKET HTICKET MUTEX SPINLOCK CLH"
    num_cores=80
    platform_def="-DXEON"
    freq=2130000000
    remote_cores="1 2 11 21 31 0 50 60 70"
    make="make"
    scal_cores="1 10 18 36"
    platform=xeon
    prog_prefix="numactl --physcpubind=1 ../"
;;
niagara) echo "running tests on niagara"
    THE_LOCKS="TTAS ARRAY MCS TICKET MUTEX SPINLOCK CLH"
    ALTERNATE=-DALTERNATE_SOCKETS
    num_cores=64
    platform_def="-DSPARC"
    freq=1200000000
    remote_cores="0 1 8 16 24 32 40 48 56"
    scal_cores="1 8 18 36"
    make="make"
    platform=niagara
    prog_prefix="../"
;;
tilera) echo "running tests on tilera"
    THE_LOCKS="TTAS ARRAY MCS TICKET MUTEX SPINLOCK CLH"
    num_cores=36
    platform_def="-DTILERA"
    freq=1200000000
    remote_cores="0 1 2 3 4 5 11 17 23 29 35"
    scal_cores="1 8 18 36"
    make="make"
    platform=tilera
    prog_prefix="../run ../"
;;
*) echo "Program format ./run_all platform, where plafrom in opteron, xeon, niagara, tilera"
    exit;
;;
esac

ATOMIC_PRIMS="CAS TAS FAI SWAP CTR"

if [ ${run_atomic_ops} -eq 1 ]
then

for prim in ${ATOMIC_PRIMS}
do

cd ..; LOCK_VERSION=-DUSE_TTAS_LOCKS ALTERNATE_SOCKETS=${ALTERNATE} PRIMITIVE=-DTEST_${prim} OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;

for n in `seq 1 ${num_cores}`
do
if [ $n -eq 1 ]
then 
    pause_atomic=0
elif [ $n -le 2 ]
then
    pause_atomic=10
elif [ $n -le 6 ]
then
    pause_atomic=30
elif [ $n -le 12 ]
then
    pause_atomic=80
else
    pause_atomic=150
fi
max_val=0
for rep in `seq 1 ${num_repeats}`
do
rm ./results/atomic_ops_${prim}_${n}_${entries}.out
sleep 1
echo running atomic test: primitive = ${prim} lock = threads = ${n} entires = ${entires} duration = ${duration} pause = ${pause_atomic} 
${prog_prefix}atomic_bench -b 0 -n ${n} -e ${entries} -p ${pause_atomic} -d ${duration} >> ./results/atomic_ops_${prim}_${n}_${entries}.out
ops=`tail -n 1 ./results/atomic_ops_${prim}_${n}_${entries}.out | awk '{print $3}'`
if [ "${max_val}" -le "$ops" ]; then
    max_val=$ops
fi
done
echo ${n} ${max_val}>> ./results/stats_atomic_${prim}_${entries}.txt
done
cp ./results/stats_atomic_${prim}_${entries}.txt ./plots/atomic_${prim}.txt

done
fi

if [ ${run_atomic_latency} -eq 1 ]
then

for prim in ${ATOMIC_PRIMS}
do

cd ..; LOCK_VERSION=-DUSE_TTAS_LOCKS ALTERNATE_SOCKETS=${ALTERNATE} PRIMITIVE=-DTEST_${prim} OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;

for n in `seq 1 ${num_cores}`
do
min_val=10000000
for rep in `seq 1 ${num_repeats}`
do
rm ./results/atomic_latency_${prim}_${n}_${entries}.out
sleep 1
echo running atomic latency: primitive = ${prim} lock = threads = ${n} entires = ${entires} duration = ${duration} pause = 450 
${prog_prefix}atomic_bench -b 2 -n ${n} -e ${entries} -p 450 -d ${duration} >> ./results/atomic_latency_${prim}_${n}_${entries}.out
ops=`tail -n 1 ./results/atomic_latency_${prim}_${n}_${entries}.out | awk '{print $1}'`
if [ "${min_val}" -ge "$ops" ]; then
    min_val=$ops
fi
done
echo ${n} ${min_val}>> ./results/stats_atomic_latency_${prim}_${entries}.txt
done
cp ./results/stats_atomic_latency_${prim}_${entries}.txt ./plots/atomic_latency_${prim}.txt

done
for prim in ${ATOMIC_PRIMS}
do
awk '{printf "%d %d\n", $1,('${freq}'/$2 * $1)}' ./plots/atomic_latency_${prim}.txt >> ./plots/atomic_th_${prim}.txt
done

fi

#------------------------------------------------------
# LOCK TESTS
#------------------------------------------------------
acq_dur=0
inter_acq_dur=0
cl=1

if [ ${run_stress_test} -eq 1 ]
then
for prefix in ${THE_LOCKS}
do
cd ..; LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;

#stress test with reads only
for n in `seq 1 ${num_cores}`
do
max_val=0
for rep in `seq 1 ${num_repeats}`
do
rm ./results/stress_reads_${prefix}_${n}_${num_locks}.out
echo running stress test: lock = ${prefix} threads = ${n} locks = ${num_locks} duration = ${duration} acq_duration = ${acq_dur} inter_acq_delay = ${inter_acq_dur} 
${prog_prefix}stress_test -n ${n} -l ${num_locks}  -d ${duration} -a ${acq_dur} -p ${inter_acq_dur} -c ${cl} >> ./results/stress_reads_${prefix}_${n}_${num_locks}.out
ops=`tail -n 1 ./results/stress_reads_${prefix}_${n}_${num_locks}.out | awk '{print $3}'`
if [ "${max_val}" -le "$ops" ]; then
    max_val=$ops
fi
done
echo ${n} ${max_val}>> ./results/stats_stress_reads_${prefix}_${num_locks}.txt
done

#normalize results and copy them to plots dir
#ops_one=`head -n 1 ./results/stats_stress_reads_${prefix}_${num_locks}.txt | awk '{print $2}'`
#awk '{print $1, $2/'${ops_one}'}' ./results/stats_stress_reads_${prefix}_${num_locks}.txt >> ./plots/stress_reads_${prefix}.txt
cp ./results/stats_stress_reads_${prefix}_${num_locks}.txt ./plots/stress_reads_th_${prefix}.txt


#stress test with rw operations
for n in `seq 1 ${num_cores}`
do
max_val=0
for rep in `seq 1 ${num_repeats}`
do
rm ./results/stress_rw_${prefix}_${n}_${num_locks}.out

echo running stress test rw: lock = ${prefix} threads = ${n} locks = ${num_locks} duration = ${duration} acq_duration = ${acq_dur} inter_acq_delay = ${inter_acq_dur} 
${prog_prefix}stress_test -n ${n} -l ${num_locks} -w 1 -d ${duration} -a ${acq_dur} -p ${inter_acq_dur} -c ${cl}>> ./results/stress_rw_${prefix}_${n}_${num_locks}.out
ops=`tail -n 1 ./results/stress_rw_${prefix}_${n}_${num_locks}.out | awk '{print $3}'`
if [ "${max_val}" -le "$ops" ]; then
    max_val=$ops
fi
done
echo ${n} ${max_val}>> ./results/stats_stress_rw_${prefix}_${num_locks}.txt
done
#normalize results and copy them to plots dir
#ops_one=`head -n 1 ./results/stats_stress_rw_${prefix}_${num_locks}.txt | awk '{print $2}'`
#awk '{print $1, $2/'${ops_one}'}' ./results/stats_stress_rw_${prefix}_${num_locks}.txt >> ./plots/stress_rw_${prefix}.txt
cp ./results/stats_stress_rw_${prefix}_${num_locks}.txt ./plots/stress_rw_th_${prefix}.txt


#stress test with no data
cd ..; NO_DELAYS=-DNO_DELAYS LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;

for n in `seq 1 ${num_cores}`
do
max_val=0
for rep in `seq 1 ${num_repeats}`
do
rm ./results/stress_noops_${prefix}_${n}_${num_locks}.out

echo running stress test: lock = ${prefix} threads = ${n} locks = ${num_locks} duration = ${duration} inter_acq_delay = ${inter_acq_dur} no_delays = 1 
${prog_prefix}stress_test -n ${n} -l ${num_locks} -p ${inter_acq_dur}  -d ${duration} -c ${cl} >> ./results/stress_noops_${prefix}_${n}_${num_locks}.out
ops=`tail -n 1 ./results/stress_noops_${prefix}_${n}_${num_locks}.out | awk '{print $3}'`
if [ "${max_val}" -le "$ops" ]; then
    max_val=$ops
fi
done
echo ${n} ${max_val} >> ./results/stats_stress_noops_${prefix}_${num_locks}.txt
done
#normalize results and copy them to plots dir
#ops_one=`head -n 1 ./results/stats_stress_noops_${prefix}_${num_locks}.txt | awk '{print $2}'`
#awk '{print $1, $2/'${ops_one}'}' ./results/stats_stress_noops_${prefix}_${num_locks}.txt >> ./plots/stress_noops_${prefix}.txt
cp ./results/stats_stress_noops_${prefix}_${num_locks}.txt ./plots/stress_noops_th_${prefix}.txt

done

#find best sequential lock for each of the versions
ops_max_reads=0
ops_max_rw=0
ops_max_noops=0

for prefix in ${THE_LOCKS}
do
    ops_one_reads=`head -n 1 ./results/stats_stress_reads_${prefix}_${num_locks}.txt | awk '{print $2}'`
    if [ $ops_one_reads -ge $ops_max_reads ]; then
        ops_max_reads=$ops_one_reads 
    fi
    ops_one_rw=`head -n 1 ./results/stats_stress_rw_${prefix}_${num_locks}.txt | awk '{print $2}'`
    if [ $ops_one_rw -ge $ops_max_rw ]; then
        ops_max_rw=$ops_one_rw 
    fi
    ops_one_noops=`head -n 1 ./results/stats_stress_noops_${prefix}_${num_locks}.txt | awk '{print $2}'`
    if [ $ops_one_noops -ge $ops_max_noops ]; then
        ops_max_noops=$ops_one_noops 
    fi
done

#normalize the results to the best sequential version
for prefix in ${THE_LOCKS}
do
    awk '{print $1, $2/'${ops_max_reads}'}' ./results/stats_stress_reads_${prefix}_${num_locks}.txt >> ./plots/stress_reads_${prefix}.txt
    awk '{print $1, $2/'${ops_max_rw}'}' ./results/stats_stress_rw_${prefix}_${num_locks}.txt >> ./plots/stress_rw_${prefix}.txt
    awk '{print $1, $2/'${ops_max_noops}'}' ./results/stats_stress_noops_${prefix}_${num_locks}.txt >> ./plots/stress_noops_${prefix}.txt
done
fi

if [ ${run_latency_tests} -eq 1 ]
then
#------------------------------------------------------
# LOCK LATENCY TESTS
#------------------------------------------------------
acq_dur=0
inter_acq_dur=100
cl=1

for prefix in ${THE_LOCKS}
do

cd ..; LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;

#stress test with reads only
for n in `seq 1 ${num_cores}`
do
max_val=10000000
for rep in `seq 1 ${num_repeats}`
do
rm ./results/temporary.out
echo running stress latency test: lock = ${prefix} threads = ${n} locks = ${num_locks} duration = ${duration} acq_duration = ${acq_dur} inter_acq_delay = ${inter_acq_dur} 
${prog_prefix}stress_latency -n ${n} -l ${num_locks}  -d ${duration} -a ${acq_dur} -p ${inter_acq_dur} -c ${cl} >> ./results/temporary.out
ops=`tail -n 1 ./results/temporary.out | awk '{print $2}'`
if [ "${max_val}" -ge "$ops" ]; then
    max_val=$ops
fi
done
echo ${n} ${max_val} >> ./plots/stress_latency_reads_${prefix}.txt
done

#stress test with rw operations
for n in `seq 1 ${num_cores}`
do
max_val=10000000
for rep in `seq 1 ${num_repeats}`
do
rm ./results/temporary.out
echo running stress latency test rw: lock = ${prefix} threads = ${n} locks = ${num_locks} duration = ${duration} acq_duration = ${acq_dur} inter_acq_delay = ${inter_acq_dur} 
${prog_prefix}stress_latency -n ${n} -l ${num_locks} -w 1 -d ${duration} -a ${acq_dur} -p ${inter_acq_dur} -c ${cl} >>  ./results/temporary.out
ops=`tail -n 1 ./results/temporary.out | awk '{print $2}'`
if [ "${max_val}" -ge "$ops" ]; then
    max_val=$ops
fi
done
echo ${n} ${max_val} >> ./plots/stress_latency_rw_${prefix}.txt
done

#stress test with no data
cd ..; NO_DELAYS=-DNO_DELAYS LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;

for n in `seq 1 ${num_cores}`
do
max_val=10000000
for rep in `seq 1 ${num_repeats}`
do
rm ./results/temporary.out

echo running stress latency test: lock = ${prefix} threads = ${n} locks = ${num_locks} duration = ${duration} inter_acq_delay = ${inter_acq_dur} no_delays = 1 
${prog_prefix}stress_latency -n ${n} -l ${num_locks} -p ${inter_acq_dur}  -d ${duration} -c ${cl} >> ./results/temporary.out
ops=`tail -n 1 ./results/temporary.out | awk '{print $2}'`
if [ "${max_val}" -ge "$ops" ]; then
    max_val=$ops
fi
done
echo ${n} ${max_val} >> ./plots/stress_latency_noops_${prefix}.txt

done

done
fi

#------------------------------------------------------
# INDIVIDUAL OPS TEST
#------------------------------------------------------

if [ ${run_individual_tests} -eq 1 ]
then
for prefix in ${THE_LOCKS}
do

cd ..; LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;

for n in `seq 1 ${num_cores}`
do
max_val_acq=10000000
max_val_rel=10000000
for rep in `seq 1 ${num_repeats}`
do
rm ./results/temporary.out

echo running individual_ops: lock = ${prefix} threads = ${n} duration = ${duration} pause = ${individual_ops_pause} 
${prog_prefix}individual_ops -n ${n} -d ${duration} -p ${individual_ops_pause} >> ./results/temporary.out
ops=`tail -n 1 ./results/temporary.out | awk '{print $2}'`
if [ "${max_val_acq}" -ge "$ops" ]; then
    max_val_acq=$ops
fi
ops=`tail -n 1 ./results/temporary.out | awk '{print $3}'`
if [ "${max_val_rel}" -ge "$ops" ]; then
    max_val_rel=$ops
fi

done
echo ${n} ${max_val_acq} ${max_val_rel} >> ./plots/individual_ops_${prefix}.txt

done

done
fi

#------------------------------------------------------
# UNCONTENDED OPS TEST
#------------------------------------------------------
if [ ${run_uncontended} -eq 1 ]
then
for prefix in ${THE_LOCKS}
do

cd ..; LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;

for n in ${remote_cores}
do
max_val_acq=10000000
max_val_rel=10000000
the_socket=0
for rep in `seq 1 ${num_repeats}`
do
rm ./results/temporary.out

echo running uncontended test: lock = ${prefix} threads = 2  duration = ${duration} pause = ${individual_ops_pause} remote core = ${n}
${prog_prefix}uncontended -r ${n} -d ${duration} -p ${individual_ops_pause} >> ./results/temporary.out
the_socket=`tail -n 1 ./results/temporary.out | awk '{print $1}'`
ops=`tail -n 1 ./results/temporary.out | awk '{print $2}'`
if [ "${max_val_acq}" -ge "$ops" ]; then
    max_val_acq=$ops
fi
ops=`tail -n 1 ./results/temporary.out | awk '{print $3}'`
if [ "${max_val_rel}" -ge "$ops" ]; then
    max_val_rel=$ops
fi

done
echo ${the_socket} ${max_val_acq} ${max_val_rel} >> ./plots/uncontended_${prefix}.txt

done

done
fi

#choose best lock

l_small=1
l_large=512
acq_dur=0
inter_acq_dur=0
inter_acq_dur_one=100
cl=1
the_write=0

if [ ${run_min_max_stress} -eq 1 ]
then
for prefix in ${THE_LOCKS}
do

cd ..; LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;
#stress test with rw operations
for n in `seq 1 ${num_cores}`
do
max_val=0
for rep in `seq 1 ${num_repeats}`
do
rm ./results/stress_min_${prefix}_${n}_${num_locks}.out

echo running stress test min: lock = ${prefix} threads = ${n} locks = ${l_small} duration = ${duration} acq_duration = ${acq_dur} inter_acq_delay = ${inter_acq_dur_one} 
${prog_prefix}stress_test -n ${n} -l ${l_small} -w ${the_write} -d ${duration} -a ${acq_dur} -p ${inter_acq_dur_one} -c ${cl} >> ./results/stress_min_${prefix}_${n}_${num_locks}.out
ops=`tail -n 1 ./results/stress_min_${prefix}_${n}_${num_locks}.out | awk '{print $3}'`
if [ "${max_val}" -le "$ops" ]; then
    max_val=$ops
fi
done
if [ ${n} -eq 1 ]
then
echo ${prefix} ${max_val}>> ./plots/1lock_1thread.txt
fi
echo ${n} ${max_val}>> ./results/stats_stress_min_${prefix}_${num_locks}.txt
done
cp ./results/stats_stress_min_${prefix}_${num_locks}.txt ./plots/stress_min_th_${prefix}.txt


#stress test with rw operations
for n in `seq 1 ${num_cores}`
do
max_val=0
for rep in `seq 1 ${num_repeats}`
do
rm ./results/stress_max_${prefix}_${n}_${num_locks}.out

echo running stress test max: lock = ${prefix} threads = ${n} locks = ${l_large} duration = ${duration} acq_duration = ${acq_dur} inter_acq_delay = ${inter_acq_dur} 
${prog_prefix}stress_test -n ${n} -l ${l_large} -w ${the_write} -d ${duration} -a ${acq_dur} -p ${inter_acq_dur} -c ${cl} >> ./results/stress_max_${prefix}_${n}_${num_locks}.out
ops=`tail -n 1 ./results/stress_max_${prefix}_${n}_${num_locks}.out | awk '{print $3}'`
if [ "${max_val}" -le "$ops" ]; then
    max_val=$ops
fi
done
if [ ${n} -eq 1 ]
then
echo ${prefix} ${max_val} >> ./plots/maxlocks_1thread.txt
fi
echo ${n} ${max_val}>> ./results/stats_stress_max_${prefix}_${num_locks}.txt
done
cp ./results/stats_stress_max_${prefix}_${num_locks}.txt ./plots/stress_max_th_${prefix}.txt

done
fi

#scalability

acq_dur=0
inter_acq_dur=0
cl=1
the_write=0
stress_locks="4 16 32 128"

if [ ${run_best_stress} -eq 1 ]
then
for total_locks in ${stress_locks} 
do
for n in ${scal_cores}
do
if [ ${total_locks} -eq 1 ]
then
    if [ $n -eq 1 ]
    then
        the_pause=0
    else
        the_pause=100
    fi
else
    the_pause=${inter_acq_dur}
fi
max_val=0
max_lock=""
for prefix in ${THE_LOCKS}
do
cd ..; LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;
max_partial=0
for rep in `seq 1 ${num_repeats}`
do
rm ./results/temporary.out

echo running stress test rw: lock = ${prefix} threads = ${n} locks = ${total_locks} duration = ${duration} acq_duration = ${acq_dur} inter_acq_delay = ${the_pause} 
${prog_prefix}stress_test -n ${n} -l ${total_locks} -w ${the_write} -d ${duration} -a ${acq_dur} -p ${the_pause} -c ${cl} >> ./results/temporary.out
ops=`tail -n 1 ./results/temporary.out | awk '{print $3}'`
if [ "${max_val}" -le "$ops" ]; then
    max_val=$ops
    max_lock=${prefix}
fi
if [ "${max_partial}" -le "$ops" ]; then
    max_partial=$ops
fi
done
echo ${n} ${max_partial}>> ./plots/stress_${total_locks}_${prefix}.txt
done

echo ${n} ${max_val} ${max_lock}>> ./plots/stress_best_${total_locks}.txt

done
done
fi


