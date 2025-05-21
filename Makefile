# Compiler
CC = gcc

# ------------------------------ Compiler Flags ------------------------------ #
# -Wall                : Enable all the common warning messages
# -Wextra              : Enable extra warning messages not covered by -Wall
# -O3                  : Optimize the code for maximum speed
# -pedantic            : Enforce strict ANSI/ISO C compliance
# -Wshadow             : Warn if a variable declaration shadows another variable
# -Wconversion         : Warn about implicit type conversions that may change a value
# -Wdouble-promotion   : Warn when float is promoted to double unnecessarily
# -Wformat=2           : Stricter format checking for printf/scanf
# -Wstrict-overflow=5  : Warn if the compiler assumes overflow does not happen (level 5 = most strict)
# -flto                : Enable Link Time Optimization for further performance improvements
# -Werror=return-type     : Treat missing return statements as errors
# -Wuninitialized         : Warn about uninitialized variables
# -Wmaybe-uninitialized   : Warn about possibly uninitialized variables (GCC-specific)
# -fstack-protector-strong: Adds stack overflow protection for local buffers
# -fsanitize=address       : Detect heap, stack, and global buffer overflows and use-after-free bugs
# -fsanitize=undefined     : Detect undefined behavior (e.g., signed integer overflow, null deref)
CFLAGS = -g -O0 -Wall -Wextra -pedantic \
               -Wshadow -Wconversion -Wdouble-promotion \
               -Wformat=2 -Wstrict-overflow=5 -Wundef \
               -Werror=return-type -Wuninitialized -Wmaybe-uninitialized \
               -fstack-protector-strong \
               -fsanitize=address -fsanitize=undefined

# ------------------------------- Source files ------------------------------- #
# wildcard = find all files matching the given pattern
SRCS = $(wildcard *.c)

# Convert each .c filename in SRCS to a corresponding .o filename
OBJS = $(SRCS:.c=.o)

# ------------------------ Rule to build object files ------------------------ #
# How to compile each .c file into a .o file
# $< = first dependency (i.e., the .c file)
# $@ = target file (i.e., the .o file)
%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

# ---------------------------------------------------------------------------- #
#                              COMPILATION PRESETS                             #
# ---------------------------------------------------------------------------- #

#OLD ALL PRESET
#all: $(TARGET)
# If OBJS change, recompile and link them to make the final executable
# $@ = name of the target (i.e., $(TARGET))
# $^ = all dependencies (i.e., $(OBJS))
#$(TARGET): $(OBJS) $(CC) $(CFLAGS) $(SRCS) -o $@ $^

EXEC_DIR = executables
TARGET   = $(EXEC_DIR)/main

# Sice the test files have each a main file we have to filter it out during normal building
SRCS_APP = $(filter-out test_*.c,$(SRCS))
OBJS_APP = $(SRCS_APP:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS_APP)
	mkdir -p $(EXEC_DIR)
	$(CC) $(CFLAGS) $^ -o $@

TEST_SRCS    = $(wildcard TEST_*.c)
TEST_TARGETS = $(patsubst %.c,$(EXEC_DIR)/%,$(TEST_SRCS))

test: $(TEST_TARGETS)

$(EXEC_DIR)/%: %.c
	mkdir -p $(EXEC_DIR)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: all test clean

clean:
	rm -rf $(EXEC_DIR) *.o

