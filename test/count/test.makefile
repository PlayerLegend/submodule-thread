test/thread-count: LDLIBS += -lpthread -lm
test/thread-count: \
	src/thread/thread-pool.o \
	src/thread/memory-pool.o \
	src/thread/test/count/test.o \
	src/window/alloc.o \
	src/log/log.o \
	src/range/alloc.o \

C_PROGRAMS += test/thread-count

thread-tests: test/thread-count
tests: thread-tests
