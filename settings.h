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
    /**   */
    STRING_SIZE_INVALID = -2,
    STRING_SIZE_EXCEEDING_MAXIMUM = -3,
} ERROR_CODE;
