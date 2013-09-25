ifeq ($(DEBUG),1)
  DEBUG_FLAGS=-Wall -ggdb -DDEBUG
  COMPILE_FLAGS=-O0 -DADD_PADDING -fno-inline
else
  DEBUG_FLAGS=-Wall
  COMPILE_FLAGS=-O3 -DADD_PADDING
#COMPILE_FLAGS=-O3 -DADD_PADDING -DALTERNATE_CORES
endif

ifndef PLATFORM
#PLATFORM=-DTILERA
#PLATFORM=-DSPARC
#PLATFORM=-DXEON
PLATFORM=-DOPTERON 
# PLATFORM=-DXEON
endif

#ifeq ($(PLATFORM), -DOPTERON)	#allow OPTERON_OPTIMIZE only for OPTERON platform
#OPTIMIZE=-DOPTERON_OPTIMIZE
#else
#OPTIMIZE=
#endif

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
  # LOCK_VERSION=-DUSE_SPINLOCK_LOCKS
  # LOCK_VERSION=-DUSE_MCS_LOCKS
  # LOCK_VERSION=-DUSE_ARRAY_LOCKS
  # LOCK_VERSION=-DUSE_RW_LOCKS
 # LOCK_VERSION=-DUSE_TTAS_LOCKS
  # LOCK_VERSION=-DUSE_TICKET_LOCKS
  LOCK_VERSION=-DUSE_TICKET_LOCKS
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


all:  bank bank_one bank_simple stress_one stress_test stress_latency atomic_test atomic_latency individual_ops uncontended trylock_test atomic_success htlock_test libsync.a
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

stress_one: bmarks/stress_one.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/stress_one.c -o stress_one $(LIBS)


stress_latency: bmarks/stress_latency.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/stress_latency.c -o stress_latency $(LIBS)

individual_ops: bmarks/individual_ops.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/individual_ops.c -o individual_ops $(LIBS)

uncontended: bmarks/uncontended.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/uncontended.c -o uncontended $(LIBS)

trylock_test: bmarks/trylock_test.c $(OBJ_FILES) Makefile
	$(GCC) $(LOCK_VERSION) $(ALTERNATE_SOCKETS) $(NO_DELAYS) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) $(OBJ_FILES) bmarks/trylock_test.c -o trylock_test $(LIBS)

atomic_test: bmarks/atomic_test.c Makefile
	$(GCC) $(ALTERNATE_SOCKETS) $(PRIMITIVE) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) bmarks/atomic_test.c -o atomic_test $(LIBS)

atomic_latency: bmarks/atomic_latency.c Makefile
	$(GCC) $(ALTERNATE_SOCKETS) $(PRIMITIVE) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) bmarks/atomic_latency.c -o atomic_latency $(LIBS)


atomic_success: bmarks/atomic_success.c Makefile
	$(GCC) $(ALTERNATE_SOCKETS) $(PRIMITIVE) -D_GNU_SOURCE  $(COMPILE_FLAGS) $(DEBUG_FLAGS) $(INCLUDES) bmarks/atomic_success.c -o atomic_success $(LIBS)


htlock_test: htlock.o bmarks/htlock_test.c Makefile
	$(GCC) -O0 -D_GNU_SOURCE $(COMPILE_FLAGS) $(PLATFORM) $(DEBUG_FLAGS) $(INCLUDES) bmarks/htlock_test.c -o htlock_test htlock.o $(LIBS)

clean:
	rm -f *.o locks mcs_test hclh_test bank_one bank_simple bank stress_latency stress_one stress_test atomic_latency atomic_test atomic_success uncontended individual_ops trylock_test htlock_test libsync.a
