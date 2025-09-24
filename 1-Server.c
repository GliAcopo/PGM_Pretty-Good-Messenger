/**
 * @file 1-Server.c
 * @author Jacopo Rizzuto (jacoporizzuto04@gmail.com)
 * @brief .c file for the server process
 * @version 7539510
 * @date 2025-07-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "3-Global-Variables-and-Functions.h"
#include "1-Server.h"

// SIMPLE PRINT STATEMENT ON STDOUT
#define P(fmt, ...) do{fprintf(stdout,"[SR]>>> " fmt "\n", ##__VA_ARGS__);}while(0);

// ierror, an internal debug substitute to errno
ERROR_CODE ierrno = NO_ERROR;

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                                   MAIN LOOP                                                   */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
int main(int argc, char** argv)
{
    printf("Welcome to PGM server!\n");
    p("Testing p macro");
    pe("Testing pe macro");
    DEBUG_PRINT("Testing DEBUG_PRINT macro");
    p("Printing ascii art and name");
    fprintf(stdout, "%s", ascii_art);
    fprintf(stdout, "Program name: %s\n", program_name);
    p("Exiting program");
    return 0;
}
