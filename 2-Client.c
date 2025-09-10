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

// ierror, an internal debug substitute to errno
ERROR_CODE ierrno = NO_ERROR;

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

    // Check if there is a PGM folder present, if not, create it
	
    // AUTHENTICATE THE USER 
    LOGIN_SESSION_ENVIRONMENT env;
    do{
    	printf("Input your username:\n>");
	fflush(stdout);
    	if(unlikely(fgets(&env.sender, USERNAME_SIZE_CHARS, stdin) == NULL))
    	{
		PSE("");
		exit(EXIT_FAILURE);
    	}
	printf("Read name %s\nWould you like to continue the login with this username?\n[Y/n] ");
	fflush(stdout);
	char redo = fgetc(stdin);        
    }while(redo != n || redo != N);

    // VERIFY THE PRESENCE OF A USER FILE IF NOT CREATE IT
    // Check if there is a PGP key, if not, create it
    // The name of the file will be the username
    do{
	char* cwd = get_current_dir_name(); // We must free the cwd later
	P("Verifiyng presence of user [%s] file in current folder [%s]", env.sender, cwd);
	char buf[strlen(env.seder) + strlen(".pub") + 1];
	strcpy(buf, env.sender);
	strcat(buf, ".pub");
	int fd = open(buf, S_IRUSR);
	if(unlikely(fd == -1))
	{
		PSE("An error occurred while trying to open the [%s] file, \
				\nnote: if the file simply does not exist then this is normal behaviour", buf);
	}
	printf("Seems like the file %s does not exist, creating it now\n", buf);
	
	free(cwd); // Free library allocated cwd
    }while(0);
    
    // Ask for the server IP and connect to it
    
    // Once connected, download messages

    // Ask the user if he wants to read or send messages

    p("Exiting program");
    return(0);
}
