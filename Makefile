CCFLAGS = -Wall -Wextra
LDLIBS = -lpthread
OUTFILE = main
OBJFILE = main.o
CC = gcc

main: main.o
	$(CC) -o $(OUTFILE) $(OBJFILE) $(LDLIBS)

main.o: main.c
	$(CC) $(CCFLAGS) -c main.c -o $(OBJFILE)

clean:
	rm $(OUTFILE) $(OBJFILE)
