# the compiler: gcc for C program, define as g++ for C++
CC = gcc

# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall
OBJECTS = $(patsubst %.c, %.so, $(wildcard modules/*.c))


all: server.out client.out ${OBJECTS}

modules/%.so: modules/%.c
	${CC} $< -shared -fPIC -o $@ 

%.o: %.h
	${CC} $< -o $@ 

server.out: server.c sniffwrap.c sockwrap.c
	${CC} $^ -pthread -lpopt -lpcap -o $@
	
client.out: client.c sockwrap.c
	${CC} $^ -pthread -ldl -o $@ 

test: all

clean:
	rm -rf *.o modules/*.so *.out
