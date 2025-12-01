CC      = gcc
CFLAGS  = -Wall -Wextra 
PROGS   = minget minls

SRC_COMMON   = minfs_common.c
SRC_MINGET   = minget.c
SRC_MINLS    = minls.c

OBJ_COMMON   = $(SRC_COMMON:.c=.o)
OBJ_MINGET   = $(SRC_MINGET:.c=.o)
OBJ_MINLS    = $(SRC_MINLS:.c=.o)

HEADERS = min.h minfs_common.h

# Default target: build both binaries
all: $(PROGS)

# --- Link rules ---

minget: $(OBJ_MINGET) $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

minls: $(OBJ_MINLS) $(OBJ_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

# --- Compile rules ---

# Generic rule to build a .o from a .c
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $<

# --- Housekeeping ---

.PHONY: all clean

clean:
	rm -f $(PROGS) *.o
