#pragma once /* only stops a header from being included more than once within the same translation unit, it does nothing to prevent each .c file that does #include "settings.h" from getting its own copy of everything in that header */

#include <stdio.h> /* For stderr */


#define DEBUG 1
#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) \
    fprintf(stderr, "[%s:%s:%d] " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...)
#endif

/**
 * @brief This enum contains all the possible error codes that the software may produce during execution, if an error occurs it will be documented here
 * 
 */
typedef enum ERROR_CODE
{
    NO_ERROR = 0,
    ERROR = -1,
    STRING_SIZE_INVALID = -2,
    STRING_SIZE_EXCEEDING_MAXIMUM = -3,
    TTY_ERROR = -4,
    SYSCALL_ERROR = -5,
    OPERATION_ABORTED = -6,
    EXIT_PROGRAM = -99, // A return value that asks whoever called the program to explicitly close it, we don't close it here because there may be some unsaved work or other close routines to handle
} ERROR_CODE;



