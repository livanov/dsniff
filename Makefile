# the compiler: gcc for C program, define as g++ for C++
CC = gcc

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall
OBJECTS = $(patsubst %.c, %.so, $(wildcard modules/*.c))


all: program ${OBJECTS}

modules/%.so: modules/%.c
	${CC} -shared -fPIC -o $@ $< 

%.o: %.h
	${CC} -o $@ $<

program: main.c sniffwrap.c
	${CC} -ldl -lpopt -lpcap -pthread -g -o $@ $^

test: all

clean:
	rm -rf *.o modules/*.so program
