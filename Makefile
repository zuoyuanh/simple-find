CC = gcc
CFLAGS = -Wall -ansi -pedantic
MAIN = simple_find
OBJS = simple_find.o limit_fork.o
all : $(MAIN)

$(MAIN) : $(OBJS) limit_fork.h
	$(CC) $(CFLAGS) -o $(MAIN) $(OBJS)

simple_find.o : simple_find.c limit_fork.h
	$(CC) $(CFLAGS) -c simple_find.c

limit_fork.o : limit_fork.c limit_fork.h
	$(CC) $(CFLAGS) -c limit_fork.c

clean :
	rm *.o $(MAIN)
