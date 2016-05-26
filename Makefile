# the compiler: gcc for C program, define as g++ for C++
CC = gcc

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g 
#CFLAGS  = -g -Wall
OBJECTS = $(patsubst %.c, %.so, $(wildcard src_modules/*.c))


all: clean worker.out master.out ${OBJECTS}

src_modules/%.so: src_modules/%.c
	${CC} ${CFLAGS} $< -shared -fPIC -o $@ 

%.o: %.h
	${CC} $< -o $@ 

worker.out: worker.c sockwrap.c utilities.c sniffwrap.c
	${CC} ${CFLAGS} $^ -pthread -lpcap -ldl -o $@
	                            
master.out:	master.c sockwrap.c utilities.c
	${CC} ${CFLAGS} $^ -pthread -lm -o $@

test: all

clean:
	rm -rf *.o src_modules/*.so *.out
