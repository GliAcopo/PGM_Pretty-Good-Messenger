# ---------------------------------------------------------------
#                       COMPILATION PRESETS
# ---------------------------------------------------------------

#  COMPILER
CC       := gcc

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
CFLAGS   := -g -O0 -Wall -Wextra -pedantic \
            -Wshadow -Wconversion -Wdouble-promotion \
            -Wformat=2 -Wstrict-overflow=5 -Wundef \
            -Werror=return-type -Wuninitialized -Wmaybe-uninitialized \
            -fstack-protector-strong \
            -fsanitize=address,undefined,leak,coverage \
            -MMD -MP                # auto‑generate .d dependency files

# EXECUTABLES DIRECTORY
EXEC_DIR := executables

SRCS_SHARED := 3-Global-Variables-and-Functions.c

SERVER_SRCS := 1-Server.c $(SRCS_SHARED)
CLIENT_SRCS := 2-Client.c $(SRCS_SHARED)

SERVER_OBJS := $(SERVER_SRCS:.c=.o)
CLIENT_OBJS := $(CLIENT_SRCS:.c=.o)

SERVER_BIN  := $(EXEC_DIR)/server
CLIENT_BIN  := $(EXEC_DIR)/client

.PHONY: all server client clean dirs

# Default ‑ build the server.
all: server

# ----------------------------------------------------------------
# Targets
# ----------------------------------------------------------------
server: dirs $(SERVER_BIN)
client: dirs $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(CLIENT_BIN): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

# ----------------------------------------------------------------
# Pattern rule: compile .c → .o (headers handled via -MMD/-MP)
# ----------------------------------------------------------------
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ----------------------------------------------------------------
# House‑keeping
# ----------------------------------------------------------------
dirs:
	@mkdir -p $(EXEC_DIR)

clean:
	$(RM) $(SERVER_OBJS) $(CLIENT_OBJS) $(SERVER_BIN) $(CLIENT_BIN) *.d

# ----------------------------------------------------------------
# Include auto‑generated dependency files if they exist
# ----------------------------------------------------------------
-include $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)

