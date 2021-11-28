CCFLAGS = -Wall -Wextra
LDLIBS = -lpthread
OUTFILE = word_count
OBJFILE = word_count.o
CC = gcc

$(OUTFILE): $(OBJFILE)
	$(CC) -o $(OUTFILE) $(OBJFILE) $(LDLIBS)

$(OBJFILE): word_count.c
	$(CC) $(CCFLAGS) -c word_count.c -o $(OBJFILE)

clean:
	rm $(OUTFILE) $(OBJFILE)
