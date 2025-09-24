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


// SIMPLE PRINT STATEMENT ON STDOUT
#define P(fmt, ...) do{fprintf(stdout,"[CL]>>> " fmt "\n", ##__VA_ARGS__);}while(0);

// ierror, an internal debug substitute to errno
ERROR_CODE ierrno = NO_ERROR;

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              MESSAGE STRUCT CREATION                                          */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

/*
 * @brief: This fuction takes a message structure pointer and makes it ready to send
 * 	first it compiles the sender and receiver fields according to the session environment
 * 	then it asks the user to write the message on terminal
 * 	then it calls the encrypt function to encrypt the message
 * @note: initialize MESSAGE structure on stack before function call
 * */
inline ERROR_CODE create_message(LOGIN_SESSION_ENVIRONMENT* login_env, MESSAGE* message)
{
	// unlikely macro comes from the "Global variables and function.h" file and uses __builtin_expect() gcc compiler macro
	// Unlikely means that the condition probably evaluates to false
	if(unlikely(login_env == NULL || message == NULL)){
		ierrno = NULL_PARAMETERS;
		PIE("");
		return(NULL_PARAMETERS);
	}
	
	message->sender = login_env->sender;
	// Reads 64 bytes or less then stops
	printf("sender: %.64s\n", message->sender);
	message->receiver = login_env->receiver;	
	printf("receiver: %.64s\n", message->receiver);

	// @todo: WRITE MESSAGE BY GETTING IT FROM TERMINAL
	char confirm = 'Y';
	do
	{
		printf("Now write the message, press ENTER to confirm, \
			the message can be %u ascii chars long, the remaining chars will be truncated:\n>", MESSAGE_SIZE_CHARS - 1);
		ssize_t n = read(STDIN_FILENO, message->message, (MESSAGE_SIZE_CHARS - 1));
		if(unlikely(n < 0))
		{
			PSE("");
			return(SYSCALL_ERROR);
		}
		// append string null terminator
		message->message[n] = '\0';

		// ASK IF THEY WANT TO MODIFY
		printf("The message was: \n>%s\n\n", message->message);
		printf("Would you like to confirm? [Y/n]");
		if (unlikely((confirm = fgetc(stdin)) == EOF))
		{
			PSE("");
			return(SYSCALL_ERROR);
		}

	} while(confirm == 'n' || confirm == 'N'); // Continue the loop until they do not input 'n'

	// We DO NOT encrypt message right here, another specialized function will then be called to encrypt the message

	return(NO_ERROR);
}


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


    // AUTHENTICATE THE USER 
    LOGIN_SESSION_ENVIRONMENT env;

	char redo;
	do
	{
		printf("Input your username:\n>");
		fflush(stdout);

		char *ret;
		do
		{
			if (unlikely((ret = fgets(&env.sender, USERNAME_SIZE_CHARS, stdin)) == NULL))
			{
				PSE("Got null string, retry");
			}
		} while (ret == NULL);

		P("Read name: %s\nWould you like to continue the login with this username? [Y/n]\n");
		fflush(stdout);

		redo = fgetc(stdin);
	} while (redo != 'n' || redo != 'N');

	// ----------------------------------------------------------------------------------------------------------------------------------------
    // VERIFY THE PRESENCE OF A USER FILE IF NOT CREATE IT
    // Check if there is a PGP key, if not, create it
    // The name of the file will be the username

	FILE *usrfd = NULL; // Initializing file descriptor used to access the two main files
	FILE *keyfd = NULL;
	do
	{

		int usrfds = -1; // "user file descriptor system"
		int keyfds = -1;

		char *cwd = get_current_dir_name(); // We must free the cwd later
		P("Verifiyng presence of user [%s] file in current folder [%s]", env.sender, cwd);

		char suffix1[] = ".pgm";
		char buf1[strlen(env.sender) + strlen(suffix1) + 1];
		if (snprintf(buf1, sizeof(buf1), "%s%s", env.sender, suffix1) > 0)
		{
			PSE(">>> Truncated output");
			P("%s", buf1);
		}

		usrfds = open(buf1, S_IRUSR);
		if (unlikely(usrfds == -1))
		{
			PSE("An error occurred while trying to open the [%s] file, \
				\nnote: if the file simply does not exist then this is normal behaviour",
				buf1);

			printf("Seems like the file %s does not exist, creating it now:\n", buf1);

			if (creat(buf1, S_IRUSR | S_IWUSR) < 0)
			{
				PIE("");
				E();
			}
		}
		close(usrfd);

		// Open user file with messages
		if ((usrfd = fopen(buf1, "r+")) == NULL)
		{
			PSE("");
			E();
		}

		// GET USR PUBKEY FILE
		char suffix2[] = ".pgmkey";
		char buf2[strlen(env.sender) + strlen(suffix2) + 1];
		if (snprintf(buf2, sizeof(buf2), "%s%s", env.sender, suffix2) > 0)
		{
			PSE(">>> Truncated output");
			P("%s", buf2);
		}

		keyfds = open(buf2, S_IRUSR);
		if (unlikely(keyfds == -1))
		{
			PSE("An error occurred while trying to open the [%s] file, \
				\nnote: if the file simply does not exist then this is normal behaviour",
				buf2);

			printf("Seems like the file %s does not exist, creating it now\n", buf2);

			if (create_pubkey_file(buf2) != NO_ERROR)
			{
				PIE("");
				E();
			}
		}
		close(usrfd);

		// Get key fds
		if ((keyfd = fopen(buf2, "r")) == NULL)
		{
			PSE("");
			E();
		}

		free(cwd); // Free library allocated cwd
	} while (0);
	// ----------------------------------------------------------------------------------------------------------------------------------------
    
    MESSAGE message1;
    create_message(&env, &message1);
    P("%s", message1.sender);
    P("%s", message1.message);

    // TODO get the file pubkey
    
    // Ask for the server IP and connect to it
    
    // Once connected, download messages

    // Ask the user if he wants to read or send messages

    p("Exiting program");
    return(0);
}
