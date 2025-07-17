# ---------------------------------------------------------------
#                       COMPILATION PRESETS
# ---------------------------------------------------------------

#  COMPILER
CC       := gcc

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

# –g for debug, –O0 to keep the code un‑optimised while debugging
# Lots of warnings + ASan + UBSan
CFLAGS   := -g -O0 -Wall -Wextra -pedantic \
            -Wshadow -Wconversion -Wdouble-promotion \
            -Wformat=2 -Wstrict-overflow=5 -Wundef \
            -Werror=return-type -Wuninitialized -Wmaybe-uninitialized \
            -fstack-protector-strong \
            -fsanitize=address -fsanitize=undefined \
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

