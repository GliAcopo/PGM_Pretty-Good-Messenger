/**
 * @file helper.c
 * @brief Document that contains helper functions, smaller functions that serve smaller purposes to support the overall execution of the program
 * @date 2025-06-19
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "settings.h"

/**
 * @brief Converts an ERROR_CODE enum value to its corresponding string representation.
 *
 * This function takes an ERROR_CODE value and returns a constant string describing the error.
 *
 * @param code The ERROR_CODE value to convert.
 * @return A constant string representing the error code. If the code is not recognized, "UNKNOWN_ERROR_CODE" is returned.
 */
inline const char *convert_error_code_to_string(ERROR_CODE code)
{
    switch (code)
    {
    case NO_ERROR:
        return "NO_ERROR";
    case ERROR:
        return "ERROR";
    case STRING_SIZE_INVALID:
        return "STRING_SIZE_INVALID";
    case STRING_SIZE_EXCEEDING_MAXIMUM:
        return "STRING_SIZE_EXCEEDING_MAXIMUM";
    case TTY_ERROR:
        return "TTY_ERROR";
    case SYSCALL_ERROR:
        return "SYSCALL_ERROR";
    case OPERATION_ABORTED:
        return "OPERATION_ABORTED";
    case EXIT_PROGRAM:
        return "EXIT_PROGRAM";
    default:
        return "UNKNOWN_ERROR_CODE";
    }
}