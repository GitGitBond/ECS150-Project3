# Target library
lib := libfs.a

all: $(lib)

## TODO: Phase 1
CC = gcc

CFLAGS = -Wall -Wextra -Werror

CFILES = disk.c fs.c

OBJFILES = disk.o fs.o

$(lib): $(OBJFILES)
	ar rc $(lib) $(OBJFILES)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean: $(OBJFILES) $(lib)
	rm -f $(OBJFILES)
	rm -f $(lib)
