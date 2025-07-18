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
            -fsanitize=address,undefined,leak --coverage \
            -MMD -MP                # auto‑generate .d dependency files

# /////////////////// Custom BUILD directory flags (So that build objects stay out of my way) /////////////////// #
# EXECUTABLES DIRECTORY
EXEC_DIR := executables
# Builds
BUILD_DIR    := build
OBJ_DIR      := $(BUILD_DIR)/objs       # .o   .d   .gcno files
GCOV_DIR     := $(BUILD_DIR)/gcovdata   # final executables
BIN_DIR      := $(BUILD_DIR)/bin        # .gcda runtime output
# We add flags to the compiler and the linker so that they know where to store the files
# Coverage flags (compile & link)
COVFLAGS  := --coverage -fprofile-dir=$(GCOV_DIR)
CFLAGS   += $(COVFLAGS)
LDFLAGS  += $(COVFLAGS)

# ////////////////////////////////////////////// SOURCE FILE LISTS ////////////////////////////////////////////// #

SRCS_SHARED := 3-Global-Variables-and-Functions.c

SERVER_SRCS := 1-Server.c $(SRCS_SHARED)
CLIENT_SRCS := 2-Client.c $(SRCS_SHARED)

# Objects live in OBJ_DIR
SERVER_OBJS  := $(patsubst %.c,$(OBJ_DIR)/%.o,$(SERVER_SRCS))
CLIENT_OBJS  := $(patsubst %.c,$(OBJ_DIR)/%.o,$(CLIENT_SRCS))

SERVER_BIN  := $(EXEC_DIR)/server
CLIENT_BIN  := $(EXEC_DIR)/client

# Dependency‑file names mirror the objects (foo.o → foo.d)
DEPS         := $(SERVER_OBJS:.o=.d) $(CLIENT_OBJS:.o=.d)

.PHONY: all server client clean dirs



# ///////////////////////////////////////// Default ‑ build the server. ///////////////////////////////////////// #
all: server

# ----------------------------------------------------------------
# Link targets
# ----------------------------------------------------------------
server: $(SERVER_BIN)
client: $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@

$(CLIENT_BIN): $(CLIENT_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $^ -o $@

# ----------------------------------------------------------------
# Pattern rule: compile .c → .o (headers handled via -MMD/-MP) (object (+ .d file))
# ----------------------------------------------------------------
# Make sure the three build directories exist
$(BIN_DIR) $(GCOV_DIR):
	@mkdir -p $@

$(OBJ_DIR)/%.o : %.c
	@mkdir -p $(dir $@) $(GCOV_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# ----------------------------------------------------------------
# House‑keeping
# ----------------------------------------------------------------
dirs:
	@mkdir -p $(EXEC_DIR)

#clean:
#	$(RM) $(SERVER_OBJS) $(CLIENT_OBJS) $(SERVER_BIN) $(CLIENT_BIN) *.d

clean: # since we keep everything in the build dir to clean we can just remove it
	@rm -rf $(BUILD_DIR)
	@echo "Build directory removed."

# //////////////////////////////////////// Automatic header dependencies //////////////////////////////////////// #
-include $(DEPS)
