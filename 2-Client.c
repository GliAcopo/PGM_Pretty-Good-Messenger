/**
 * @file 2-Client.c
 * @author Jacopo Rizzuto (jacoporizzuto04@gmail.com)
 * @brief .c file for the client process
 * @version 0.1
 * @date 2025-07-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "3-Global-Variables-and-Functions.h"
#include "2-Client.h"

#include <sys/stat.h> /* For mkdir */
#include <error.h>    /* For error handling and printing (strerror)*/
#include <errno.h>    /* For errno */
#include <stdio.h>
#include <stdlib.h> // to get the home environment name
#include <string.h> // String concatenation
#include <unistd.h> // for read

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                            PGP ENCYPTION FUNCTIONS                                            */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
// #region PGP



/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                                   MAIN LOOP                                                   */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

int main(int argc, char** argv)
{
    printf("Welcome to PGM client!\n");
    p("Testing p macro");
    pe("Testing pe macro");
    DEBUG_PRINT("Testing DEBUG_PRINT macro");
    p("Printing ascii art and name");
    fprintf(stdout, "%s", ascii_art);
    fprintf(stdout, "Program name: %s\n", program_name);
    p("Exiting program");
    return(0);
}