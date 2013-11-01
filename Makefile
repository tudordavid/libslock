PLATFORM_NUMA=1

ifeq ($(DEBUG),1)
  DEBUG_FLAGS=-Wall -ggdb -DDEBUG
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O3 -DADD_PADDING
#COMPILE_FLAGS=-O3 -DADD_PADDING -DALTERNATE_CORES
endif

ifndef PLATFORM
# PLATFORM=-DSPARC
# PLATFORM=-DTILERA
# PLATFORM=-DXEON
# PLATFORM=-DOPTERON
PLATFORM=-DDEFAULT
endif

ifeq ($(PLATFORM), -DDEFAULT)
CORE_NUM := $(shell nproc)
ifneq ($(CORE_SPEED_KHz), )
COMPILE_FLAGS += -DCORE_NUM=${CORE_NUM}
else
COMPILE_FLAGS += -DCORE_NUM=8
endif
$(info ********************************** Using as a default number of cores: $(CORE_NUM) on 1 socket)
$(info ********************************** Is this correct? If not, fix it in platform_defs.h)
endif

ifeq ($(PLATFORM), -DOPTERON)	#allow OPTERON_OPTIMIZE only for OPTERON platform
OPTIMIZE=-DOPTERON_OPTIMIZE
else
OPTIMIZE=
endif

COMPILE_FLAGS += $(PLATFORM)
COMPILE_FLAGS += $(OPTIMIZE)

UNAME := $(shell uname)

ifeq ($(PLATFORM),-DTILERA)
	GCC:=tile-gcc
	LIBS:=-lrt -lpthread -ltmc
else
ifeq ($(UNAME), Linux)
	GCC:=gcc
	LIBS := -lrt -lpthread -lnuma
endif
endif
ifeq ($(UNAME), SunOS)
	GCC:=/opt/csw/bin/gcc
	LIBS := -lrt -lpthread
	COMPILE_FLAGS+= -m64 -mcpu=v9 -mtune=v9
endif

ifndef LOCK_VERSION
  # LOCK_VERSION=-DUSE_HCLH_LOCKS
  # LOCK_VERSION=-DUSE_TTAS_LOCKS
  LOCK_VERSION=-DUSE_SPINLOCK_LOCKS
  # LOCK_VERSION=-DUSE_MCS_LOCKS
  # LOCK_VERSION=-DUSE_ARRAY_LOCKS
  # LOCK_VERSION=-DUSE_RW_LOCKS
  # LOCK_VERSION=-DUSE_CLH_LOCKS
  # LOCK_VERSION=-DUSE_TICKET_LOCKS
  # LOCK_VERSION=-DUSE_MUTEX_LOCKS
  # LOCK_VERSION=-DUSE_HTICKET_LOCKS
endif

ifndef PRIMITIVE
PRIMITIVE=-DTEST_CAS
endif
#ACCOUNT_PADDING=-DPAD_ACCOUNTS

TOP := $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))

SRCPATH := $(TOP)/src
MAININCLUDE := $(TOP)/include

INCLUDES := -I$(MAININCLUDE)
OBJ_FILES :=  mcs.o clh.o ttas.o spinlock.o rw_ttas.o ticket.o alock.o hclh.o gl_lock.o htlock.o


all:  bank bank_one bank_simple test_array_alloc test_trylock sample_generic sample_mcs test_correctness stress_one stress_test stress_latency atomic_bench individual_ops uncontended  htlock_test measure_contention libsync.a
	@echo "############### Used: " $(LOCK_VERSION) " on " $(PLATFORM) " with " $(OPTIMIZE)

libsync.a: ttas.o rw_ttas.o ticket.o clh.o mcs.o hclh.o alock.o htlock.o include/atomic_ops.h include/utils.h include/lock_if.h
	ar -r libsync.a ttas.o rw_ttas.o ticket.o clh.o mcs.o alock.o hclh.o htlock.o spinlock.o include/atomic_ops.h include/utils.h

ttas.o: src/ttas.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ttas.c $(LIBS)

spinlock.o: src/spinlock.c
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/spinlock.c $(LIBS)

rw_ttas.o: src/rw_ttas.c
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/rw_ttas.c $(LIBS)

ticket.o: src/ticket.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ticket.c $(LIBS)

ticket_contention.o: src/ticket.c 
	$(GCC) -D_GNU_SOURCE -DMEASURE_CONTENTION $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/ticket.c -o ticket_contention.o $(LIBS)

gl_lock.o: src/gl_lock.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/gl_lock.c $(LIBS)

mcs.o: src/mcs.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/mcs.c $(LIBS)

clh.o: src/clh.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/clh.c $(LIBS)

hclh.o: src/hclh.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/hclh.c $(LIBS)

alock.o: src/alock.c 
	$(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/alock.c $(LIBS)

htlock.o: src/htlock.c include/htlock.h
	 $(GCC) -D_GNU_SOURCE $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) -c src/htlock.c $(LIBS) 
bank: bmarks/bank_th.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(ACCOUNT_PADDING) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/bank_th.c -o bank $(LIBS)

bank_one: bmarks/bank_one.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(ACCOUNT_PADDING) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/bank_one.c -o bank_one $(LIBS)


bank_simple: bmarks/bank_simple.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/bank_simple.c -o bank_simple $(LIBS)

stress_test: bmarks/stress_test.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/stress_test.c -o stress_test $(LIBS)

measure_contention: bmarks/measure_contention.c $(OBJ_FILES) ticket_contention.o Makefile
	$(GCC) -DUSE_TICKET_LOCKS $(ALTERNATE_SOCKETS) $(NO_DELAYS) -DMEASURE_CONTENTION -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) ticket_contention.o bmarks/measure_contention.c -o measure_contention $(LIBS)

stress_one: bmarks/stress_one.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/stress_one.c -o stress_one $(LIBS)

test_correctness: bmarks/test_correctness.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/test_correctness.c -o test_correctness $(LIBS)

sample_generic: samples/sample_generic.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) samples/sample_generic.c -o sample_generic $(LIBS)

sample_mcs: samples/sample_mcs.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) samples/sample_mcs.c -o sample_mcs $(LIBS)


test_trylock: bmarks/test_trylock.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/test_trylock.c -o test_trylock $(LIBS)


test_array_alloc: bmarks/test_array_alloc.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/test_array_alloc.c -o test_array_alloc $(LIBS)


stress_latency: bmarks/stress_latency.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/stress_latency.c -o stress_latency $(LIBS)

individual_ops: bmarks/individual_ops.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/individual_ops.c -o individual_ops $(LIBS)

uncontended: bmarks/uncontended.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/uncontended.c -o uncontended $(LIBS)

atomic_bench: bmarks/atomic_bench.c Makefile
	$(GCC) $(ALTERNATE_SOCKETS) $(PRIMITIVE) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) bmarks/atomic_bench.c -o atomic_bench $(LIBS)

htlock_test: htlock.o bmarks/htlock_test.c Makefile
	$(GCC) -O0 -D_GNU_SOURCE $(COMPILE_FLAGS) $(PLATFORM) $(DEBUG_FLAGS) $(INCLUDES) bmarks/htlock_test.c -o htlock_test htlock.o $(LIBS)

clean:
	rm -f *.o locks mcs_test hclh_test bank_one bank_simple bank* stress_latency* test_array_alloc test_trylock sample_generic test_correctness stress_one stress_test*  atomic_bench uncontended individual_ops trylock_test htlock_test measure_contention libsync.a
