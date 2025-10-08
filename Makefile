# ---------------------------------------------------------------
#                       COMPILATION PRESETS
# ---------------------------------------------------------------

# ------------------------------ Compiler Flags ------------------------------ #
# -Wall                                 : Enable all the common warning messages
# -Wextra                               : Enable extra warning messages not covered by -Wall
# -O2                                   : Optimize the code for maximum speed
# -pedantic                             : Enforce strict ANSI/ISO C compliance
# -Wshadow                              : Warn if a variable declaration shadows another variable
# -Wshadow / -Wshadow=local             : Warn on variable‐name shadowing
# -Wconversion                          : Warn about implicit type conversions that may change a value
# -Wconversion / -Wsign-conversion      : Warn on implicit type conversions
# -Wdouble-promotion                    : Warn when float is promoted to double unnecessarily
# -Wformat=2                            : Enforces the strictest compile‑time checks on printf/scanf format strings: ->
#                                       : • Verifies that each format specifier (e.g. %d, %s, %p) matches the type of the corresponding argument
#                                       : • Catches mismatches (like passing a double to %d), missing or extra arguments, and unsafe width/precision uses
#                                       : • Helps prevent format‑string vulnerabilities by rejecting potentially dangerous constructs
#                                        
# -Wstrict-overflow=5                   : Enables the highest‑level warnings about compiler assumptions on signed‐integer overflow: ->
#                                       : • Detects when the optimizer rewrites code under the “undefined‑behavior” rule for signed overflow
#                                       : • Warns you if your code might rely on overflow wrapping or if the compiler transforms your logic assuming no overflow can occur
#                                       : • Level 5 is the most pedantic setting—good for catching subtle UB‑based optimizations that could break your intentions
#                                       
# -flto                                 : Enable Link Time Optimization for further performance improvements
# -Werror=return-type                   : Treat missing return statements as errors
# -Wuninitialized                       : Warn about uninitialized variables
# -Wmaybe-uninitialized                 : Warn about possibly uninitialized variables (GCC-specific)
# -fstack-protector-strong              : Adds stack overflow protection for local buffers
# -fsanitize=address                    : Detect heap, stack, and global buffer overflows and use-after-free bugs
# -fsanitize=undefined                  : Detect undefined behavior (e.g., signed integer overflow, null deref)
# -fsanitize=coverage                   : Gather coverage data for sanitizers
# ------------------------ not applied:
# -g3                                   : Emit maximum debug info (including macros).
# -fno-omit-frame-pointer               : Keep frame pointers for profilers/backtraces
# -fprofile-generate / -fprofile-use    : PGO: generate runtime profiles, then optimize with real usage data
# -fanalyzer                            : Built‑in static code analysis
# --coverage                            : GCC’s gcov instrumentation (-fprofile-arcs -ftest-coverage)
# -Weverything                          : Every warning—then selectively disable the noisy ones

# –g for debug, –O0 to keep the code un‑optimised while debugging
# Lots of warnings + ASan + UBSan
# Top‑level Makefile
# Usage:
#   make server    # build ./bin/server
#   make client    # build ./bin/client
#   make           # build both targets
#   make clean     # remove all build artefacts
#
# Directory layout (auto‑created if missing):
#   ./build    – object (.o) and dependency (.d) files
#   ./bin      – final executables
# -pedantic Damn I hate pedantic warnings, I do not care about the compiler criyng about the old C standard
CC      := gcc
CFLAGS  := -std=c11 -g -O0 -Wall -Wextra \
           -Wshadow -Wconversion -Wdouble-promotion \
           -Wformat=2 -Wstrict-overflow=5 -Wundef \
           -Werror=return-type -Wuninitialized -Wmaybe-uninitialized \
           -fstack-protector-strong \
           -fsanitize=address,undefined,leak --coverage \
           -MMD -MP

SRC_DIR := .
OBJ_DIR := build
BIN_DIR := bin

SERVER_SRCS := 1-Server.c 3-Global-Variables-and-Functions.c
CLIENT_SRCS := 2-Client.c 3-Global-Variables-and-Functions.c

SERVER_OBJS := $(SERVER_SRCS:%.c=$(OBJ_DIR)/%.o)
CLIENT_OBJS := $(CLIENT_SRCS:%.c=$(OBJ_DIR)/%.o)

SERVER_DEPS := $(SERVER_OBJS:.o=.d)
CLIENT_DEPS := $(CLIENT_OBJS:.o=.d)

.PHONY: all server client clean dirs

all: server client

server: $(BIN_DIR)/server
client: $(BIN_DIR)/client

$(BIN_DIR)/server: $(SERVER_OBJS) | dirs
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/client: $(CLIENT_OBJS) | dirs
	$(CC) $(CFLAGS) $^ -o $@

# Pattern rule for every object file.
# The -MF flag ensures the .d file is written next to the .o object inside $(OBJ_DIR).
$(OBJ_DIR)/%.o: %.c | dirs
	$(CC) $(CFLAGS) -c $< -o $@ -MF $(patsubst %.o,%.d,$@)

dirs:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR)

clean:
	@$(RM) -r $(OBJ_DIR) $(BIN_DIR)

# Automatically pull in dependency files if they exist.
-include $(SERVER_DEPS) $(CLIENT_DEPS)


