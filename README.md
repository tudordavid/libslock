Libslock
=======

This repository provides:
- libslock, a cross-platform interface to atomic operations and other common operations 
- implementations of a number of well-known locking algorithms 
- benchmarks testing the performance of atomic operations and various locking algorithms

The package has been tested on x86_64 Intel and AMD machines, Tilera and Sparc architectures.

A version of this code was used in the paper **"Everything you always wanted to know about synchronization but were afraid to ask"** (accessible here: http://dl.acm.org/citation.cfm?doid=2517349.2522714).

Makefile parameters:

Locking Algorithm
-----------------
Can be passed using `LOCK_VERSION` to the Makefile. `LOCK_VERSION` can take one of the following values:

- `USE_TTAS_LOCKS` - use test-and-test-and-set locks
- `USE_SPINLOCK_LOCKS` - use a test-and-set spinlokc
- `USE_TICKET_LOCKS` - use ticket locks
- `USE_HTICKET_LOCKS` - use hierarchical ticket locks
- `USE_MCS_LOCKS` - use MCS locks
- `USE_CLH_LOCKS` - use CLH locks
- `USE_HCLH_LOCKS` - use HCLH locks
- `USE_ARRAY_LOCKS` - use array locks
- `USE_RW_LOCKS` - use read-write locks (not used in paper, not optimized)
- `USE_MUTEX_LOCKS` - use the phtread mutex


Platform
--------
Can be passed using `PLATFORM` to the Makefile; the settings are specific to the platforms we were using (topology, etc.); for other platforms the characteristics can be defined in `platform_defs.h`. The pre-defined platforms are: 

- `XEON` - 8 x 10-core Intel sever
- `OPTERON` - 8 x 6-core AMD server
- `NIGARA` - 8-core SparcT2 machine
- `TILERA` - 36-core Tilera machine

Detailed descriptions of these platforms can be found in the paper.

The `OPTERON_OPTIMIZE` option uses some of the Opteron-specific optimizations mentioned in the paper.
Atomic operation to be tested
-----------------------------
For the benchmarks testing atomic operations, this parameter selects the desired operation. Can be passed to the Makefile using `PRIMITIVE`:

- `TEST_FAI` - fetch-and-increment
- `TEST_TAS` - test-and-set
- `TEST_CAS` - compare-and-swap
- `TEST_SWAP` - atomic swap
- `TEST_CAS_FAI` - fetch-and-increment implemented using compare-and-swap

`ALTERNATE_SOCKETS` is used for thread placement on the Niagara; if not set, hardware threads begin by being assinged to the same core; if set threads are disitributed evenly among the cores
