# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall -Wextra -O3 -pedantic -Wshadow -Wconversion -Wdouble-promotion -Wformat=2 -Wstrict-overflow=5 -flto

# Source files
SRCS = $(wildcard *.c)

OBJS = $(SRCS:.c=.o)

all: $(TARGET)
$(TARGET): $(OBJS) $(CC) $(CFLAGS) $(SRCS) -o $@ $^

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@