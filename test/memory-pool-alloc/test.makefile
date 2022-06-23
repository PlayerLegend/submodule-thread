test/thread-memory-pool-alloc: LDLIBS += -lpthread
test/thread-memory-pool-alloc: \
	src/thread/memory-pool.o \
	src/thread/test/memory-pool-alloc/test.o \
	src/window/alloc.o \
	src/log/log.o

C_PROGRAMS += test/thread-memory-pool-alloc

thread-tests: test/thread-memory-pool-alloc
tests: thread-tests
