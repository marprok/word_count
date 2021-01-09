CCFLAGS = -Wall -Wextra
LDLIBS = -lpthread
OUTFILE = it21901
OBJFILE = it21901.o
CC = gcc

it21901: it21901.o
	$(CC) -o $(OUTFILE) $(OBJFILE) $(LDLIBS)

it21901.o: it21901.c
	$(CC) $(CCFLAGS) -c it21901.c -o $(OBJFILE)

clean:
	rm $(OUTFILE) $(OBJFILE)
