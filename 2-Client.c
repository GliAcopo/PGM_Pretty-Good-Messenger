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
#include <stdint.h>	// uint32_t
#include <stddef.h>	// offsetof

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
	if (unlikely(login_env == NULL || message == NULL))
	{
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
	// PHASE 0:
	// Print a startup banner so the user immediately knows the client process has started correctly.
    printf("Welcome to PGM client!\n");
    P("Printing ascii art and name");
    fprintf(stdout, "%s", ascii_art);
    fprintf(stdout, "Program name: %s\n", program_name);




	/* -------------------------------------------------------------------------- */
	/*                            GET THE USERNAME                                */
	/* -------------------------------------------------------------------------- */
	LOGIN_SESSION_ENVIRONMENT env = {0};

	P(">>> argc = %d", argc);

	if (likely(argc >= 2)) // If username passed as parameter, skip interactive prompt so scripts/tests can run non-interactively
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
			// Why fflush here:
			// this prompt ends with '>' and not with '\n', so line-buffered stdout may keep it in memory.
			// We flush manually to guarantee the prompt is visible before waiting for user input.
			fflush(stdout);

			char *ret;
			do
			{
				if (unlikely((ret = fgets(env.sender, USERNAME_SIZE_CHARS, stdin)) == NULL))
				{
					PSE(">>> Got null string, retry");
				}
			} while (ret == NULL);

			env.sender[strcspn(env.sender, "\n")] = '\0'; // remove newline for network send

			P(">>> Read name: %s\nWould you like to continue the login with this username? [Y/n]", env.sender);
			// Same rationale as above: ensure the confirmation prompt is printed before we block on fgetc().
			fflush(stdout);

			redo = (unsigned char)(fgetc(stdin));
			int c;
			// Consume any extra characters left on stdin so the next fgets() starts from a clean line.
			while ((c = getchar()) != '\n' && c != EOF) { }
		} while (redo == 'n' || redo == 'N');
	}



	/* -------------------------------------------------------------------------- */
	/*                              CONNECT TO SOCKET                             */
	/* -------------------------------------------------------------------------- */
	// PHASE 2:
	// Collect server endpoint information (IP + port), create the TCP socket, then establish the connection.
	
		int sockfd = -1;
		struct sockaddr_in srv = {0};
		char server_ip[INET_ADDRSTRLEN];

		P("[%s] >>> Enter server IPv4 address (default is local 0.0.0.0) (e.g. 192.168.1.10):", env.sender);
		// Why fflush here:
		// the prompt has no final newline, so it may stay buffered and invisible to the user.
		// Flushing now guarantees the user sees exactly what is being asked.
		fflush(stdout);
		if (likely(argc >= 3)) // If server ip passed as parameter
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
		// fgets() keeps the trailing newline when there is space in the buffer.
		// We remove it because parsing/validation/network code expects a clean C string token.
		server_ip[strcspn(server_ip, "\n")] = '\0';
		if (unlikely(strlen(server_ip) == 0)) // default to local
		{
			P("[%s] >>> Defaulting to local server IP", env.sender);
			snprintf(server_ip, sizeof(server_ip), "0.0.0.0");
		}


		// Ask for the server port for the user
		char port_input[16] = {0};
		printf("Enter server port (default is 6666) (1-65535):\n>");
		// Same prompt-visibility reason: flush before blocking on stdin.
		fflush(stdout);
		if (likely(argc >= 4))
		{
			P(">>> Server port passed as parameter: %s", argv[3]);
			snprintf(port_input, sizeof(port_input), "%s", argv[3]);
		}
		else
		{
			if (unlikely(fgets(port_input, sizeof(port_input), stdin) == NULL))
			{
				PSE("[%s] >>> Failed to read server port", env.sender);
				return(1);
			}
		}
		port_input[strcspn(port_input, "\n")] = '\0';
		if (unlikely(strlen(port_input) <= 0)) // Newline was stripped from input (before was giving problems)
		{ // default to 6666
			snprintf(port_input, sizeof(port_input), "6666");
			P("[%s] >>> Defaulting to port 6666", env.sender);
		}
		P("[%s] >>> Server port input: %s", env.sender, port_input);

		char *endptr = NULL;
		// strtol() is used instead of atoi() so we can validate:
		// 1) if no number was parsed, and 2) if the value is outside valid port range.
		long port_long = strtol(port_input, &endptr, 10);
		if (unlikely(endptr == port_input || port_long <= 0 || port_long > 65535))
		{
			P("[%s] >>> Invalid port number port_input:port_long[%s]:[%ld]", env.sender, port_input, port_long);
			return(1);
		}
		const uint16_t SERVER_PORT = (uint16_t)port_long;

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (unlikely(sockfd < 0))
		{
			PSE("[%s] >>> socket()", env.sender);
			return(1);
		}

		srv.sin_family = AF_INET; // This socket speaks IPv4.
		// Port in sockaddr_in must be stored in network byte order, not host byte order.
		srv.sin_port = htons(SERVER_PORT);
		if (unlikely(inet_pton(AF_INET, server_ip, &srv.sin_addr) <= 0))
		{
			PSE("[%s] >>> Invalid IPv4 address", env.sender);
			close(sockfd);
			return(1);
		}

		P("[%s] >>> Connecting to %s:%u ...", env.sender, server_ip, SERVER_PORT);
		if (unlikely(connect(sockfd, (struct sockaddr *)&srv, sizeof(srv)) < 0))
		{
			PSE("[%s] >>> connect()", env.sender);
			close(sockfd);
			return(1);
		}
	




		/* -------------------------------------------------------------------------- */
		/*                            SERVER AUTHENTICATION                           */
		/* -------------------------------------------------------------------------- */
	{
		// PHASE 3:
		// Login handshake with the server.
		// First we send username, then we follow registration or authentication based on server response.
		char* username = &env.sender[0];
		// Hard safety terminator to avoid accidental non-null-terminated strings in edge cases.
		username[USERNAME_SIZE_CHARS - 1] = '\0';
		// If username came from fgets(), remove trailing newline so server receives just the name.
		username[strcspn(username, "\n")] = '\0';

		P("[%s] >>> Sending username: %s", env.sender, username);

		// Server login path expects fixed-size buffers for username/password.
		// For this handshake only, we send a constant-size zero-padded buffer.
		char username_fixed[USERNAME_SIZE_CHARS] = {0};
		snprintf(username_fixed, sizeof(username_fixed), "%s", username);
		if (unlikely(send_all(sockfd, username_fixed, sizeof(username_fixed)) < 0))
		{
			PSE("[%s] >>> Failed to send fixed-size username", env.sender);
			close(sockfd);
			return(1);
		}

		// Wait for the first server decision code:
		// - START_REGISTRATION => user does not exist and must register
		// - NO_ERROR          => user exists, continue with password authentication
		ERROR_CODE server_code = ERROR;
		// We expect a fixed-size ERROR_CODE packet here.
		// MSG_WAITALL asks recv() to wait until all bytes of that packet arrive (or until error/close).
		ssize_t recvd = recv(sockfd, &server_code, sizeof(server_code), MSG_WAITALL);
		if (unlikely(recvd != sizeof(server_code)))
		{
			PSE("[%s] >>> Failed to receive initial server code (got %zd bytes)", env.sender, recvd);
			close(sockfd);
			return(1);
		}
		P("[%s] >>> Initial server response: %s", env.sender, convert_error_code_to_string(server_code));

		// Depending on the server response code, either register or authenticate
		if (unlikely(server_code == START_REGISTRATION)) // registration path
		{
			// Registration path: server did not find this user and asks for initial password setup.
			char password[PASSWORD_SIZE_CHARS] = {0};
			printf("User not found, please register.\nInsert new password:\n>");
			// Prompt ends with '>' (no newline), so flush manually before reading input.
			fflush(stdout);

			if (likely(argc >= 5))
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
					return(1);
				}
			}
			password[strcspn(password, "\n")] = '\0';

			// Server expects a fixed-size password buffer during registration handshake.
			if (unlikely(send_all(sockfd, password, sizeof(password)) < 0))
			{
				PSE("[%s] >>> Failed to send fixed-size registration password", env.sender);
				close(sockfd);
				return(1);
			}

			recvd = recv(sockfd, &server_code, sizeof(server_code), MSG_WAITALL);
			if (unlikely(recvd != sizeof(server_code)))
			{
				PSE("[%s] >>> Failed to receive registration confirmation (got %zd bytes)", env.sender, recvd);
				close(sockfd);
				return(1);
			}
			if (unlikely(server_code != NO_ERROR))
			{
				P("[%s] >>> Registration failed: %s", env.sender, convert_error_code_to_string(server_code));
				close(sockfd);
				return(1);
			}
			P("[%s] >>> Registration successful", env.sender);

		}
		else if (likely(server_code == NO_ERROR)) // authentication path
		{
			// Existing-user path: user exists, so run password authentication loop.
			// const int MAX_PASSWORD_ATTEMPTS = 3; --- DEFINED IN 3-Global-Variables-and-Functions.h ---

			int attempt = 0;
			int authenticated = 0;
			char password[PASSWORD_SIZE_CHARS] = {0};

			while (attempt < MAX_PASSWORD_ATTEMPTS && !authenticated) {
				memset(password, 0, sizeof(password));
				printf("Insert password:\n>");
				// Flush prompt before blocking on stdin, for the same reason explained above.
				fflush(stdout);

				if (likely(argc >= 5))
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
				password[strcspn(password, "\n")] = '\0';

				// Same login/auth handshake rule: fixed-size zero-padded password buffer.
				if (unlikely(send_all(sockfd, password, sizeof(password)) < 0))
				{
					PSE("[%s] >>> Failed to send fixed-size password attempt", env.sender);
					close(sockfd);
					return(1);
				}

				recvd = recv(sockfd, &server_code, sizeof(server_code), MSG_WAITALL);
				if (unlikely(recvd != sizeof(server_code)))
				{
					PSE("[%s] >>> Failed to receive auth response (got %zd bytes)", env.sender, recvd);
					close(sockfd);
					return(1);
				}

				if (likely(server_code == NO_ERROR))
				{
					P("[%s] >>> Authentication successful", env.sender);
					authenticated = 1;
				}
				else
				{
					// Increase attempts only on failed authentication responses.
					// This keeps MAX_PASSWORD_ATTEMPTS semantics clear and predictable.
					attempt++;
					P("[%s] >>> Authentication failed: %s (%d/%d)", env.sender, convert_error_code_to_string(server_code), attempt, MAX_PASSWORD_ATTEMPTS);
					if (unlikely(attempt >= MAX_PASSWORD_ATTEMPTS))
					{
						P("[%s] >>> Maximum attempts reached, closing.", env.sender);
						close(sockfd);
						return(1);
					}
				}
			}
		}
		else
		{
			P("[%s] >>> Unexpected server response: %s", env.sender, convert_error_code_to_string(server_code));
			close(sockfd);
			return(1);
		}

	}



	/* -------------------------------------------------------------------------- */
	/*                         MESSAGE SENDING AND READING                        */
	/* -------------------------------------------------------------------------- */
	int running = 1;
	while (running)
	{
		// PHASE 4A:
		// Show the operations menu and convert user choice into an internal MESSAGE_CODE request.
		printf("\nSelect operation:\n");
		printf("  [1] Send message\n");
		printf("  [2] List registered users\n");
		printf("  [3] Load message\n");
		printf("  [4] Load unread messages list\n");
		printf("  [5] Delete message\n");
		printf("  [q] Quit\n> ");
		// Menu prompt ends with '>' and no newline, so flush now to make it visible immediately.
		fflush(stdout);

		// Input strategy:
		// read the whole line first, then pick the first non-whitespace character.
		// This is tolerant to accidental spaces or extra trailing characters.
		char choice_buf[16] = {0};
		if (unlikely(fgets(choice_buf, sizeof(choice_buf), stdin) == NULL))
		{
			PSE("[%s] >>> Failed to read menu choice", env.sender);
			break;
		}
		P("[%s] >>> Read choice: %s", env.sender, choice_buf);

		// Extract first non-whitespace character as command selector.
		// Examples that map correctly to the same choice: "1", "1\n", "   1", "1 anything".
		char choice = '\0';
		for (size_t i = 0; choice_buf[i] != '\0'; i++)
		{
			if (likely(choice_buf[i] != ' ' && choice_buf[i] != '\t' && choice_buf[i] != '\n' && choice_buf[i] != '\r'))
			{
				choice = choice_buf[i];
				break;
			}
		}

		// Map choice to MESSAGE_CODE
		MESSAGE_CODE request_code = MESSAGE_ERROR;
		switch (choice)
		{
		case '1': // Accept both numeric and mnemonic aliases, so menu usage is faster and more forgiving.
		case 's':
		case 'S':
			request_code = REQUEST_SEND_MESSAGE;
			break;
		case '2':
		case 'l':
		case 'L':
			request_code = REQUEST_LIST_REGISTERED_USERS;
			break;
		case '3':
		case 'm':
		case 'M':
			request_code = REQUEST_LOAD_MESSAGE;
			break;
		case '4':
		case 'u':
		case 'U':
			request_code = REQUEST_LOAD_UNREAD_MESSAGES;
			break;
		case '5':
		case 'd':
		case 'D':
			request_code = REQUEST_DELETE_MESSAGE;
			break;
		case 'q':
		case 'Q':
			request_code = LOGOUT;
			break;
		default: // invalid choice
			P("[%s] >>> Invalid choice", env.sender);
			continue;
		}

			switch (request_code)
			{
			case REQUEST_SEND_MESSAGE:
			{
				// PHASE 4B:
				// Send message flow:
				// 1) gather input, 2) send operation code + header, 3) wait server validation, 4) send body.
				char recipient[USERNAME_SIZE_CHARS] = {0};
				printf("Insert recipient username:\n>");
				// Prompt has no trailing newline, therefore explicit flush is needed before reading.
				fflush(stdout);
				if (unlikely(fgets(recipient, sizeof(recipient), stdin) == NULL))
				{
					PSE("[%s] >>> Failed to read recipient", env.sender);
					running = 0;
					break;
				}
				recipient[strcspn(recipient, "\n")] = '\0';

				char subject[SUBJECT_SIZE_CHARS] = {0};
				printf("Insert message subject (max %u chars):\n>", SUBJECT_SIZE_CHARS - 1);
				// Same reason: force prompt visibility before blocking on fgets().
				fflush(stdout);
				if (unlikely(fgets(subject, sizeof(subject), stdin) == NULL))
				{
					PSE("[%s] >>> Failed to read message subject", env.sender);
					running = 0;
					break;
				}
				subject[strcspn(subject, "\n")] = '\0';
				if (unlikely(subject[0] == '\0'))
				{
					P("[%s] >>> Empty subject is not allowed", env.sender);
					break;
				}

				char message_buf[MESSAGE_SIZE_CHARS + 1] = {0};
				printf("Insert message body (max %u chars):\n>", MESSAGE_SIZE_CHARS);
				// Same reason again: do not rely on automatic line-buffer flush here.
				fflush(stdout);
				if (unlikely(fgets(message_buf, sizeof(message_buf), stdin) == NULL))
				{
					PSE("[%s] >>> Failed to read message body", env.sender);
					running = 0;
					break;
				}
				message_buf[strcspn(message_buf, "\n")] = '\0';

				// Protocol step 1:
				// tell the server which operation is requested before sending any operation-specific data.
				if (unlikely(send_all(sockfd, &request_code, sizeof(request_code)) < 0))
				{
					PSE("[%s] >>> Failed to send MESSAGE_CODE", env.sender);
					running = 0;
					break;
				}

				// We send only the fixed-size MESSAGE header first.
				// The variable-size body is sent later, but only if server accepts metadata/recipient checks.
				size_t header_size = offsetof(MESSAGE, message);
				MESSAGE *header = calloc(1, header_size);
				if (unlikely(header == NULL))
				{
					PSE("[%s] >>> Failed to allocate MESSAGE header", env.sender);
					running = 0;
					break;
				}
				snprintf(header->sender, sizeof(header->sender), "%s", env.sender);
				snprintf(header->recipient, sizeof(header->recipient), "%s", recipient);
				snprintf(header->subject, sizeof(header->subject), "%s", subject);
				size_t body_len = strlen(message_buf);
				// message_length is part of network payload, so convert host byte order -> network byte order.
				header->message_length = htonl((uint32_t)body_len);

				if (unlikely(send_all(sockfd, header, header_size) < 0))
				{
					PSE("[%s] >>> Failed to send MESSAGE header", env.sender);
					free(header);
					running = 0;
					break;
				}

				// Wait for server pre-check result before sending body.
				// This avoids sending large payload data when recipient/metadata is invalid.
				ERROR_CODE server_code = ERROR;
				if (unlikely(recv_all(sockfd, &server_code, sizeof(server_code)) <= 0))
				{
					PSE("[%s] >>> Failed to receive send-message response", env.sender);
					free(header);
					running = 0;
					break;
				}
				if (unlikely(server_code != NO_ERROR))
				{
					P("[%s] >>> Send message failed: %s", env.sender, convert_error_code_to_string(server_code));
					free(header);
					break;
				}

				// Body is sent as raw bytes without '\0': receiver already knows exact length from header->message_length.
				if (likely(body_len > 0))
				{
					if (unlikely(send_all(sockfd, message_buf, body_len) < 0))
					{
						PSE("[%s] >>> Failed to send message body", env.sender);
						free(header);
						running = 0;
						break;
					}
				}

				free(header);
				P("[%s] >>> Message sent", env.sender);
				break;
			}
		case REQUEST_LIST_REGISTERED_USERS:
		{
			// PHASE 4C:
			// Request the registered users list.
			// Protocol pattern is: receive length prefix -> send ack -> receive variable-size payload.
			if (unlikely(send_all(sockfd, &request_code, sizeof(request_code)) < 0))
			{
				PSE("[%s] >>> Failed to send MESSAGE_CODE", env.sender);
				running = 0;
				break;
			}

			uint32_t list_len_net = 0;
			if (unlikely(recv_all(sockfd, &list_len_net, sizeof(list_len_net)) <= 0))
			{
				PSE("[%s] >>> Failed to receive users list length", env.sender);
				running = 0;
				break;
			}
			// Convert list length from network byte order to host byte order before using it.
			uint32_t list_len = ntohl(list_len_net);

			// Send explicit ack so server knows client is ready for the variable-size list payload.
			ERROR_CODE ack = NO_ERROR;
			if (unlikely(send_all(sockfd, &ack, sizeof(ack)) < 0))
			{
				PSE("[%s] >>> Failed to send list ack", env.sender);
				running = 0;
				break;
			}

			char *list = calloc(list_len == 0 ? 1 : list_len, sizeof(char));
			if (unlikely(list == NULL))
			{
				PSE("[%s] >>> Failed to allocate users list buffer", env.sender);
				running = 0;
				break;
			}
			if (likely(list_len > 0))
			{
				if (unlikely(recv_all(sockfd, list, list_len) <= 0))
				{
					PSE("[%s] >>> Failed to receive users list", env.sender);
					free(list);
					running = 0;
					break;
				}
			}

			printf("\nRegistered users:\n%s\n", list);
			free(list);
			break;
		}
		case REQUEST_LOAD_MESSAGE:
		{
			// PHASE 4D:
			// Ask server for available message filenames, let user choose one, then download and print that message.
			if (unlikely(send_all(sockfd, &request_code, sizeof(request_code)) < 0))
			{
				PSE("[%s] >>> Failed to send MESSAGE_CODE", env.sender);
				running = 0;
				break;
			}

			uint32_t list_len_net = 0;
			if (unlikely(recv_all(sockfd, &list_len_net, sizeof(list_len_net)) <= 0))
			{
				PSE("[%s] >>> Failed to receive message list length", env.sender);
				running = 0;
				break;
			}
			// Length prefix is sent over the network, so convert it back to host byte order.
			uint32_t list_len = ntohl(list_len_net);

			ERROR_CODE ack = NO_ERROR;
			if (unlikely(send_all(sockfd, &ack, sizeof(ack)) < 0))
			{
				PSE("[%s] >>> Failed to send message list ack", env.sender);
				running = 0;
				break;
			}

			char *list = calloc(list_len == 0 ? 1 : list_len, sizeof(char));
			if (unlikely(list == NULL))
			{
				PSE("[%s] >>> Failed to allocate message list buffer", env.sender);
				running = 0;
				break;
			}
			if (likely(list_len > 0))
			{
				if (unlikely(recv_all(sockfd, list, list_len) <= 0))
				{
					PSE("[%s] >>> Failed to receive message list", env.sender);
					free(list);
					running = 0;
					break;
				}
			}

			if (unlikely(list[0] == '\0'))
			{
				P("[%s] >>> No messages available", env.sender);
				// Tell the server we are aborting this operation so it does not wait for a file selection code/name.
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(list);
				break;
			}

			size_t entry_count = 0;
			for (size_t i = 0; list[i] != '\0'; i++)
			{
				// Server sends list entries separated by '\n', so counting newlines gives number of entries.
				if (unlikely(list[i] == '\n'))
				{
					entry_count++;
				}
			}

			char **entries = calloc(entry_count, sizeof(char *));
			if (unlikely(entries == NULL))
			{
				PSE("[%s] >>> Failed to allocate entries array", env.sender);
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(list);
				running = 0;
				break;
			}

			size_t idx = 0;
			char *cursor = list;
			for (size_t i = 0; list[i] != '\0'; i++)
			{
				if (unlikely(list[i] == '\n'))
				{
					// Replace separator with '\0' and reuse existing buffer (in-place tokenization, no extra copies).
					list[i] = '\0';
					if (likely(idx < entry_count))
					{
						entries[idx++] = cursor;
					}
					cursor = &list[i + 1];
				}
			}

			printf("\nMessages:\n");
			for (size_t i = 0; i < entry_count; i++)
			{
				printf("  [%zu] %s\n", i, entries[i]);
			}
			printf("Select message number or 'q' to cancel:\n>");
			// Prompt has no trailing newline, flush now so it is visible before fgets().
			fflush(stdout);

			char choice_line[32] = {0};
			if (unlikely(fgets(choice_line, sizeof(choice_line), stdin) == NULL))
			{
				PSE("[%s] >>> Failed to read selection", env.sender);
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(entries);
				free(list);
				running = 0;
				break;
			}

			if (unlikely(choice_line[0] == 'q' || choice_line[0] == 'Q'))
			{
				// User canceled from client side: send explicit abort code so server can close this request cleanly.
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(entries);
				free(list);
				break;
			}

			char *select_endptr = NULL;
			long selection = strtol(choice_line, &select_endptr, 10);
			if (unlikely(select_endptr == choice_line || selection < 0 || (size_t)selection >= entry_count))
			{
				P("[%s] >>> Invalid selection", env.sender);
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(entries);
				free(list);
				break;
			}

			// After list stage, server expects a second operation code indicating we selected one specific message.
			MESSAGE_CODE select_code = REQUEST_LOAD_SPECIFIC_MESSAGE;
			if (unlikely(send_all(sockfd, &select_code, sizeof(select_code)) < 0))
			{
				PSE("[%s] >>> Failed to send REQUEST_LOAD_SPECIFIC_MESSAGE", env.sender);
				free(entries);
				free(list);
				running = 0;
				break;
			}

			const char *filename = entries[selection];
			size_t filename_len = strlen(filename) + 1;
			if (unlikely(send_all(sockfd, filename, filename_len) < 0))
			{
				PSE("[%s] >>> Failed to send filename", env.sender);
				free(entries);
				free(list);
				running = 0;
				break;
			}

			ERROR_CODE response = ERROR;
			if (unlikely(recv_all(sockfd, &response, sizeof(response)) <= 0))
			{
				PSE("[%s] >>> Failed to receive load response", env.sender);
				free(entries);
				free(list);
				running = 0;
				break;
			}
			if (unlikely(response != NO_ERROR))
			{
				P("[%s] >>> Load message failed: %d", env.sender, response);
				free(entries);
				free(list);
				break;
			}

				size_t header_size = offsetof(MESSAGE, message);
				MESSAGE *header = calloc(1, header_size);
				if (unlikely(header == NULL))
				{
					PSE("[%s] >>> Failed to allocate MESSAGE header", env.sender);
					free(entries);
					free(list);
					running = 0;
					break;
				}
				if (unlikely(recv_all(sockfd, header, header_size) <= 0))
				{
					PSE("[%s] >>> Failed to receive MESSAGE header", env.sender);
					free(header);
					free(entries);
					free(list);
					running = 0;
					break;
				}
				header->sender[USERNAME_SIZE_CHARS - 1] = '\0';
				header->recipient[USERNAME_SIZE_CHARS - 1] = '\0';
				header->subject[SUBJECT_SIZE_CHARS - 1] = '\0';
				header->sender[strcspn(header->sender, "\n")] = '\0';
				header->recipient[strcspn(header->recipient, "\n")] = '\0';
				header->subject[strcspn(header->subject, "\n")] = '\0';

				// message_length in the header is serialized in network byte order, convert before using it.
				uint32_t body_len = ntohl(header->message_length);
				if (unlikely(body_len == 0 || body_len > MESSAGE_SIZE_CHARS))
				{
					P("[%s] >>> Invalid message length received", env.sender);
					free(header);
					free(entries);
					free(list);
					break;
				}

				char *body = calloc(body_len + 1, sizeof(char));
				if (unlikely(body == NULL))
				{
					PSE("[%s] >>> Failed to allocate message body", env.sender);
					free(header);
					free(entries);
					free(list);
					running = 0;
					break;
				}
				if (unlikely(recv_all(sockfd, body, body_len) <= 0))
				{
					PSE("[%s] >>> Failed to receive message body", env.sender);
					free(body);
					free(header);
					free(entries);
					free(list);
					running = 0;
					break;
				}
				body[body_len] = '\0';

				// Final step of this operation: render loaded message in human-readable terminal format.
				printf("\nMessage loaded:\n");
				printf("  From: %s\n", header->sender);
				printf("  To: %s\n", header->recipient);
				printf("  Subject: %s\n", header->subject);
				printf("  Body: %s\n", body);

			free(body);
			free(header);
			free(entries);
			free(list);
			break;
		}
		case REQUEST_LOAD_UNREAD_MESSAGES:
		{
			// PHASE 4E:
			// Load unread messages list using the same length-prefix + ack handshake pattern.
			if (unlikely(send_all(sockfd, &request_code, sizeof(request_code)) < 0))
			{
				PSE("[%s] >>> Failed to send MESSAGE_CODE", env.sender);
				running = 0;
				break;
			}

			uint32_t list_len_net = 0;
			if (unlikely(recv_all(sockfd, &list_len_net, sizeof(list_len_net)) <= 0))
			{
				PSE("[%s] >>> Failed to receive unread list length", env.sender);
				running = 0;
				break;
			}
			// Convert payload length from network order to host order before allocation/read.
			uint32_t list_len = ntohl(list_len_net);

			ERROR_CODE ack = NO_ERROR;
			if (unlikely(send_all(sockfd, &ack, sizeof(ack)) < 0))
			{
				PSE("[%s] >>> Failed to send unread list ack", env.sender);
				running = 0;
				break;
			}

			char *list = calloc(list_len == 0 ? 1 : list_len, sizeof(char));
			if (unlikely(list == NULL))
			{
				PSE("[%s] >>> Failed to allocate unread list buffer", env.sender);
				running = 0;
				break;
			}
			if (likely(list_len > 0))
			{
				if (unlikely(recv_all(sockfd, list, list_len) <= 0))
				{
					PSE("[%s] >>> Failed to receive unread list", env.sender);
					free(list);
					running = 0;
					break;
				}
			}

			printf("\nUnread messages:\n%s\n", list);
			free(list);
			break;
		}
		case REQUEST_DELETE_MESSAGE:
		{
			// PHASE 4F:
			// Delete flow is similar to load-message flow:
			// get list -> choose entry -> send selected filename -> receive delete result.
			if (unlikely(send_all(sockfd, &request_code, sizeof(request_code)) < 0))
			{
				PSE("[%s] >>> Failed to send MESSAGE_CODE", env.sender);
				running = 0;
				break;
			}

			uint32_t list_len_net = 0;
			if (unlikely(recv_all(sockfd, &list_len_net, sizeof(list_len_net)) <= 0))
			{
				PSE("[%s] >>> Failed to receive delete list length", env.sender);
				running = 0;
				break;
			}
			// Convert payload length from network order to host order before allocation/read.
			uint32_t list_len = ntohl(list_len_net);

			ERROR_CODE ack = NO_ERROR;
			if (unlikely(send_all(sockfd, &ack, sizeof(ack)) < 0))
			{
				PSE("[%s] >>> Failed to send delete list ack", env.sender);
				running = 0;
				break;
			}

			char *list = calloc(list_len == 0 ? 1 : list_len, sizeof(char));
			if (unlikely(list == NULL))
			{
				PSE("[%s] >>> Failed to allocate delete list buffer", env.sender);
				running = 0;
				break;
			}
			if (likely(list_len > 0))
			{
				if (unlikely(recv_all(sockfd, list, list_len) <= 0))
				{
					PSE("[%s] >>> Failed to receive delete list", env.sender);
					free(list);
					running = 0;
					break;
				}
			}

			if (unlikely(list[0] == '\0'))
			{
				P("[%s] >>> No messages available", env.sender);
				// Tell server this request is intentionally aborted, so it does not wait for a selection.
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(list);
				break;
			}

			size_t entry_count = 0;
			for (size_t i = 0; list[i] != '\0'; i++)
			{
				if (unlikely(list[i] == '\n'))
				{
					entry_count++;
				}
			}

			char **entries = calloc(entry_count, sizeof(char *));
			if (unlikely(entries == NULL))
			{
				PSE("[%s] >>> Failed to allocate delete entries array", env.sender);
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(list);
				running = 0;
				break;
			}

			size_t idx = 0;
			char *cursor = list;
			for (size_t i = 0; list[i] != '\0'; i++)
			{
				if (unlikely(list[i] == '\n'))
				{
					// Reuse list buffer by replacing '\n' separators with '\0' terminators.
					list[i] = '\0';
					if (likely(idx < entry_count))
					{
						entries[idx++] = cursor;
					}
					cursor = &list[i + 1];
				}
			}

			printf("\nMessages:\n");
			for (size_t i = 0; i < entry_count; i++)
			{
				printf("  [%zu] %s\n", i, entries[i]);
			}
			printf("Select message number to delete or 'q' to cancel:\n>");
			// Prompt has no trailing newline, so flush to keep interaction immediate and clear.
			fflush(stdout);

			char choice_line[32] = {0};
			if (unlikely(fgets(choice_line, sizeof(choice_line), stdin) == NULL))
			{
				PSE("[%s] >>> Failed to read delete selection", env.sender);
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(entries);
				free(list);
				running = 0;
				break;
			}

			if (unlikely(choice_line[0] == 'q' || choice_line[0] == 'Q'))
			{
				// User canceled delete operation from menu: notify server with explicit abort code.
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(entries);
				free(list);
				break;
			}

			char *select_endptr = NULL;
			long selection = strtol(choice_line, &select_endptr, 10);
			if (unlikely(select_endptr == choice_line || selection < 0 || (size_t)selection >= entry_count))
			{
				P("[%s] >>> Invalid selection", env.sender);
				MESSAGE_CODE abort_code = MESSAGE_OPERATION_ABORTED;
				send_all(sockfd, &abort_code, sizeof(abort_code));
				free(entries);
				free(list);
				break;
			}

			// After list exchange, server expects this code to indicate "one concrete entry was selected".
			MESSAGE_CODE select_code = REQUEST_LOAD_SPECIFIC_MESSAGE;
			if (unlikely(send_all(sockfd, &select_code, sizeof(select_code)) < 0))
			{
				PSE("[%s] >>> Failed to send REQUEST_LOAD_SPECIFIC_MESSAGE", env.sender);
				free(entries);
				free(list);
				running = 0;
				break;
			}

			const char *filename = entries[selection];
			size_t filename_len = strlen(filename) + 1;
			if (unlikely(send_all(sockfd, filename, filename_len) < 0))
			{
				PSE("[%s] >>> Failed to send filename", env.sender);
				free(entries);
				free(list);
				running = 0;
				break;
			}

			ERROR_CODE response = ERROR;
			if (unlikely(recv_all(sockfd, &response, sizeof(response)) <= 0))
			{
				PSE("[%s] >>> Failed to receive delete response", env.sender);
				free(entries);
				free(list);
				running = 0;
				break;
			}

			if (likely(response == NO_ERROR))
			{
				P("[%s] >>> Message deleted", env.sender);
			}
			else
			{
				P("[%s] >>> Delete failed: %d", env.sender, response);
			}

			free(entries);
			free(list);
			break;
		}
		case LOGOUT:
			// PHASE 4G:
			// Graceful termination path: notify server with LOGOUT code, then break local loop.
			if (unlikely(send_all(sockfd, &request_code, sizeof(request_code)) < 0))
			{
				PSE("[%s] >>> Failed to send LOGOUT", env.sender);
			}
			running = 0;
			break;
		default:
		{
			if (unlikely(send_all(sockfd, &request_code, sizeof(request_code)) < 0))
			{
				PSE("[%s] >>> Failed to send MESSAGE_CODE", env.sender);
				running = 0;
				break;
			}
			MESSAGE_CODE server_msg_code = MESSAGE_ERROR;
			ssize_t recvd = recv(sockfd, &server_msg_code, sizeof(server_msg_code), MSG_WAITALL);
			if (unlikely(recvd != sizeof(server_msg_code)))
			{
				PSE("[%s] >>> Failed to receive MESSAGE_CODE response (got %zd bytes)", env.sender, recvd);
				running = 0;
				break;
			}
			P("[%s] >>> Server MESSAGE_CODE response: %d", env.sender, server_msg_code);
			break;
		}
		}
	}

	/* -------------------------------------------------------------------------- */
	/*                             END OF CLIENT LOOP                             */
	/* -------------------------------------------------------------------------- */
	close(sockfd);
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
		if (unlikely(snprintf(buf1, sizeof(buf1), "%s%s", env.sender, suffix1) > 0))
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

			if (unlikely(creat(buf1, S_IRUSR | S_IWUSR) < 0))
			{
				PIE("");
				E();
			}
		}
		close(usrfd);

		// Open user file with messages
		if (unlikely((usrfd = fopen(buf1, "r+")) == NULL))
		{
			PSE("");
			E();
		}

		// GET USR PUBKEY FILE
		char suffix2[] = ".pgmkey";
		char buf2[strlen(env.sender) + strlen(suffix2) + 1];
		if (unlikely(snprintf(buf2, sizeof(buf2), "%s%s", env.sender, suffix2) > 0))
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

			if (unlikely(create_pubkey_file(buf2) != NO_ERROR))
			{
				PIE("");
				E();
			}
		}
		close(usrfd);

		// Get key fds
		if (unlikely((keyfd = fopen(buf2, "r")) == NULL))
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
