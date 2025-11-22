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
#include <sys/socket.h> // socket, bind, listen, accept
#include <netinet/in.h> // struct sockaddr_in, INADDR_ANY
#include <arpa/inet.h>	// inet_ntoa
#include <ifaddrs.h>	// struct ifaddrs, getifaddrs, freeifaddrs
#include <netdb.h>		// NI_MAXHOST, getnameinfo
#include <linux/if_link.h> // IFLA_ADDRESS

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
/** @deprecated 
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
*/

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                                   MAIN LOOP                                                   */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

int main(int argc, char** argv)
{
    printf("Welcome to PGM client!\n");
    P("Printing ascii art and name");
    fprintf(stdout, "%s", ascii_art);
    fprintf(stdout, "Program name: %s\n", program_name);




	/* -------------------------------------------------------------------------- */
	/*                            GET THE USERNAME                                */
	/* -------------------------------------------------------------------------- */
	LOGIN_SESSION_ENVIRONMENT env;

	P(">>> argc = %d", argc);

	if(argc >= 2) // If username passed as parameter
	{
		P(">>> Username passed as parameter: %s", argv[1]);
		snprintf(env.sender, USERNAME_SIZE_CHARS, "%s", argv[1]);
	}
	else // Ask for username
	{
		unsigned char redo = 'y';	// fgetc() and getchar() return the character read as an unsigned char cast to an int or EOF on end of file or error.
		do
		{
			printf(">>>Input your username:\n>");
			fflush(stdout);

			char *ret;
			do
			{
				if (unlikely((ret = fgets(env.sender, USERNAME_SIZE_CHARS, stdin)) == NULL))
				{
					PSE(">>> Got null string, retry");
				}
			} while (ret == NULL);

			env.sender[strcspn(env.sender, "\r\n")] = '\0'; // remove newline for network send

			P(">>> Read name: %s\nWould you like to continue the login with this username? [Y/n]\n", env.sender);
			fflush(stdout);

			redo = (unsigned char)(fgetc(stdin));
			int c;
			while ((c = getchar()) != '\n' && c != EOF) { } // flush trailing input
		} while (redo == 'n' || redo == 'N');
	}



	/* -------------------------------------------------------------------------- */
	/*                              CONNECT TO SOCKET                             */
	/* -------------------------------------------------------------------------- */
	// open IPv4 socket, ask server IP, send username and wait for a response
	
		int sockfd = -1;
		struct sockaddr_in srv = {0};
		char server_ip[INET_ADDRSTRLEN];

		P("[%s] >>> Enter server IPv4 address (e.g. 192.168.1.10):", env.sender);
		fflush(stdout);
		if(argc >= 3) // If server ip passed as parameter
		{
			P(">>> Server IP passed as parameter: %s", argv[2]);
			snprintf(server_ip, sizeof(server_ip), "%s", argv[2]);
		}
		else		  // Else ask the user
		{
			if (unlikely(fgets(server_ip, sizeof(server_ip), stdin) == NULL))
			{
				PSE("[%s] >>> Failed to read server IP", env.sender);
				return (1);
			}
		}
		/* strip newline */
		server_ip[strcspn(server_ip, "\r\n")] = '\0';


		// Ask for the server port for the user
		char port_input[16] = {0};
		printf("Enter server port (1-65535):\n>");
		fflush(stdout);
		if(argc >= 4)
		{
			P(">>> Server port passed as parameter: %s", argv[3]);
			snprintf(port_input, sizeof(port_input), "%s", argv[3]);
		}
		else
		{
			if (unlikely(fgets(port_input, sizeof(port_input), stdin) == NULL)) {
				PSE("[%s] >>> Failed to read server port", env.sender);
				return(1);
			}
		}
		
		char *endptr = NULL;
		long port_long = strtol(port_input, &endptr, 10);
		if (endptr == port_input || port_long <= 0 || port_long > 65535) {
			P("[%s] >>> Invalid port number", env.sender);
			return(1);
		}
		const uint16_t SERVER_PORT = (uint16_t)port_long;

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (unlikely(sockfd < 0)) {
			PSE("[%s] >>> socket()", env.sender);
			return(1);
		}

		srv.sin_family = AF_INET;
		srv.sin_port = htons(SERVER_PORT);
		if (unlikely(inet_pton(AF_INET, server_ip, &srv.sin_addr) <= 0)) {
			PSE("[%s] >>> Invalid IPv4 address", env.sender);
			close(sockfd);
			return(1);
		}

		P("[%s] >>> Connecting to %s:%u ...", env.sender, server_ip, SERVER_PORT);
		if (unlikely(connect(sockfd, (struct sockaddr *)&srv, sizeof(srv)) < 0)) {
			PSE("[%s] >>> connect()", env.sender);
			close(sockfd);
			return(1);
		}
	




		/* -------------------------------------------------------------------------- */
		/*                            SERVER AUTHENTICATION                           */
		/* -------------------------------------------------------------------------- */
	{
		/* prepare username and send */
		char* username = &env.sender[0];
		username[USERNAME_SIZE_CHARS - 1] = '\0';
		username[strcspn(username, "\r\n")] = '\0';

		P("[%s] >>> Sending username: %s", env.sender, username);

		ssize_t sent = send(sockfd, username, strlen(username) + 1, 0); /* also send the null byte */
		if (unlikely(sent < 0)) {
			PSE("[%s] >>> send()", env.sender);
			close(sockfd);
			return(1);
		}

		/* wait for a response from server (ERROR_CODE) */
		ERROR_CODE server_code = ERROR;
		ssize_t recvd = recv(sockfd, &server_code, sizeof(server_code), MSG_WAITALL); /* MSG_WAITALL On  SOCK_STREAM  sockets this requests that the function block until the full amount of data can be returned. The function may return the smaller amount of data if the socket is a message-based socket, if a signal is caught, if the connection is  termi-nated, if MSG_PEEK was specified, or if an error is pending for the socket. */
		if (unlikely(recvd != sizeof(server_code))) {
			PSE("[%s] >>> Failed to receive initial server code (got %zd bytes)", env.sender, recvd);
			close(sockfd);
			return(1);
		}
		P("[%s] >>> Initial server response: %s", env.sender, convert_error_code_to_string(server_code));

		// Depending on the server response code, either register or authenticate
		if (unlikely(server_code == START_REGISTRATION)) { // registration path
			/* registration path */
			char password[PASSWORD_SIZE_CHARS];
			printf("User not found, please register.\nInsert new password:\n>");
			fflush(stdout);

			if(argc >= 5)
			{
				P("[%s] >>> <password> passed as parameter: %s", env.sender, argv[4]);
				snprintf(password, sizeof(password), "%s", argv[4]);
			}
			else
			{
				if (unlikely(fgets(password, sizeof(password), stdin) == NULL)) {
					PSE("[%s] >>> Failed to read password", env.sender);
					close(sockfd);
					return(1);
				}
			}
			password[strcspn(password, "\r\n")] = '\0';

			sent = send(sockfd, password, strlen(password) + 1, 0);
			if (unlikely(sent < 0)) {
				PSE("[%s] >>> Failed to send registration password", env.sender);
				close(sockfd);
				return(1);
			}

			recvd = recv(sockfd, &server_code, sizeof(server_code), MSG_WAITALL);
			if (unlikely(recvd != sizeof(server_code))) {
				PSE("[%s] >>> Failed to receive registration confirmation (got %zd bytes)", env.sender, recvd);
				close(sockfd);
				return(1);
			}
			if (unlikely(server_code != NO_ERROR)) {
				P("[%s] >>> Registration failed: %s", env.sender, convert_error_code_to_string(server_code));
				close(sockfd);
				return(1);
			}
			P("[%s] >>> Registration successful", env.sender);

		} else if (server_code == NO_ERROR) { // authentication path
			/* existing user, perform login */
			// const int MAX_PASSWORD_ATTEMPTS = 3; --- DEFINED IN 3-Global-Variables-and-Functions.h ---

			int attempt = 0;
			int authenticated = 0;
			char password[PASSWORD_SIZE_CHARS];

			while (attempt < MAX_PASSWORD_ATTEMPTS && !authenticated) {
				printf("Insert password:\n>");
				fflush(stdout);

				if (argc >= 5)
				{
					P("[%s] >>> <password> passed as parameter: %s", env.sender, argv[4]);
					snprintf(password, sizeof(password), "%s", argv[4]);
				}
				else
				{
					if (unlikely(fgets(password, sizeof(password), stdin) == NULL))
					{
						PSE("[%s] >>> Failed to read password", env.sender);
						close(sockfd);
						return (1);
					}
				}
				password[strcspn(password, "\r\n")] = '\0';

				sent = send(sockfd, password, strlen(password) + 1, 0);
				if (unlikely(sent < 0)) {
					PSE("[%s] >>> Failed to send password attempt", env.sender);
					close(sockfd);
					return(1);
				}

				recvd = recv(sockfd, &server_code, sizeof(server_code), MSG_WAITALL);
				if (unlikely(recvd != sizeof(server_code))) {
					PSE("[%s] >>> Failed to receive auth response (got %zd bytes)", env.sender, recvd);
					close(sockfd);
					return(1);
				}

				if (server_code == NO_ERROR) {
					P("[%s] >>> Authentication successful", env.sender);
					authenticated = 1;
				} else {
					attempt++;
					P("[%s] >>> Authentication failed: %s (%d/%d)", env.sender, convert_error_code_to_string(server_code), attempt, MAX_PASSWORD_ATTEMPTS);
					if (attempt >= MAX_PASSWORD_ATTEMPTS) {
						P("[%s] >>> Maximum attempts reached, closing.", env.sender);
						close(sockfd);
						return(1);
					}
				}
			}
		} else {
			P("[%s] >>> Unexpected server response: %s", env.sender, convert_error_code_to_string(server_code));
			close(sockfd);
			return(1);
		}

		/* close socket (or keep it open for further communication) */
		close(sockfd);
	}



	/* -------------------------------------------------------------------------- */
	/*                         MESSAGE SENDING AND READING                        */
	/* -------------------------------------------------------------------------- */
    
    

    P("Exiting program");
    return(0);
}

/** @deprecated
 * // VERIFY THE PRESENCE OF A USER FILE IF NOT CREATE IT
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
 */
