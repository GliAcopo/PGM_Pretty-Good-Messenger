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

#define _GNU_SOURCE     // To get defns of NI_MAXSERV and NI_MAXHOST 
                        // WARNING: apparently this must be defined before any other inclusion, (Learned the hard way, do not remove this line))
#include "3-Global-Variables-and-Functions.h"
#include "1-Server.h"
#include <stdio.h>      // printf, fprintf
#include <stdlib.h>     // exit, EXIT_FAILURE, EXIT_SUCCESS
#include <unistd.h>     // close, access
#include <pthread.h>    // pthread_create, pthread_t, pthread_exit
#include <sys/socket.h> // socket, bind, listen, accept
#include <netinet/in.h> // struct sockaddr_in, INADDR_ANY
#include <arpa/inet.h>  // inet_ntoa
#include <ifaddrs.h>    // struct ifaddrs, getifaddrs, freeifaddrs
#include <netdb.h>      // NI_MAXHOST, getnameinfo
// #include <linux/if_link.h> // IFLA_ADDRESS


// SIMPLE PRINT STATEMENT ON STDOUT
#define P(fmt, ...) do{fprintf(stdout,"[SRV]>>> " fmt "\n", ##__VA_ARGS__);}while(0);

// Max number of pending connections in the socket listen queue
#define MAX_BACKLOG 10

// ierror, an internal debug substitute to errno
ERROR_CODE ierrno = NO_ERROR;

// port number to use in the server, if -1 or > 65535 then an ephemeral is used instead
const int32_t PORT_NUMBER = 666; // log_2(65535) = 16 bits, 16-1= 15 (sign) so (to be sure) I decided to use a 32 bit 

/**
 * @brief Prints all the local IP addresses of the machine
 * @param port The port number to print alongside the IP addresses
 * @return ERROR_CODE NO_ERROR on success, SYSCALL_ERROR on failure
 * @note This function is inspired to the linux man example getifaddrs(3)
 */
ERROR_CODE print_local_ip_addresses(uint16_t port)
{
    // We use getifaddrs to get the list of network interfaces on the machine
    struct ifaddrs *ifaddr;   // why bother allocating two pointers? because ifsddrs is actually a linked list! So we need to maintain a head pointer or else we won't be able to free the memory
    if (getifaddrs(&ifaddr) == -1) // WARNING: allocates the ifaddrs structure by itself, so there is a need to free it later NOT WITH FREE() (man 3 getifaddrs)
    {
        PSE("getifaddrs() error");
        return SYSCALL_ERROR;
    }

    // I just spent like 30 minutes trying to figure out why the loopback address was not being filtered out turns out I was comparing a char array with a uint32_t aaaaaaaaaaaaaaaaaaaaaaaaa
    //    // Loopback address code
    //    uint32_t loopback = 0; // IPv4 is 32 bits according to linux man inet_pton(3), so there should be no problem storing it in a uint32_t... Right?
    //    if(unlikely(inet_pton(AF_INET, "127.0.0.1", &loopback) <= 0)) // Converts the string to binary format, using this to compare the addresses we find with the loopback address @note: pton can also return 0 if the address is invalid, but we know it's valid since it's hardcoded
    //    {
    //        PSE("inet_pton() error");
    //        freeifaddrs(ifaddr); // Edge case in wich we free (forgot it once...)
    //        return SYSCALL_ERROR;
    //    }
    

    P("Local IP addresses found:");
    // Walk through linked list, maintaining head pointer so we can free list later
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr == NULL){
            continue;
        }

        int family = ifa->ifa_addr->sa_family;

        // For an AF_INET* interface address, display the address
        if (family == AF_INET) // AF_INET = IPv4 ; AF_INET6 = IPv6 (We do not use IPv6...)
        {
            char host[NI_MAXHOST];
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (unlikely(s != 0))
            {
                PSE("getnameinfo() failed: %s", gai_strerror(s)); // gai_strerror (copypasted from man page) converts the error code returned by getnameinfo to a human readable string
                freeifaddrs(ifaddr); // Edge case in wich we free (forgot it twice...)
                return SYSCALL_ERROR;
            }
            else if (!strcmp(host, "127.0.0.1")) // We skip the loop if the address is loopback
            {
                continue; 
            } else  // else not strictly needed but whatever
            {
                P("\tInterface: %s\tAddress: %s:%u", ifa->ifa_name, host, port); // \t stands for tab (ordering text )
            }
        }
    }

    freeifaddrs(ifaddr); // Ah, free at last
    return NO_ERROR;
}

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                          COONNECTION HANDLER (THREAD)                                         */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

void *thread_routine(void *arg)
{
    int connection_fd = *((int *)arg);
    P("[%d]::: Thread started for connection fd: %d", connection_fd, connection_fd);

    // const int MAX_PASSWORD_ATTEMPTS = 3; // Of course, if the user fails more than this number of times, we close the connection
    // This is now defined in 3-Global-Variables-and-Functions.h

    // Handle the connection
    LOGIN_SESSION_ENVIRONMENT login_env = {0};      // @note: initialized to zero, not really needed but I do it for best practice
    ERROR_CODE response_code = NO_ERROR;
    char stored_password[PASSWORD_SIZE_CHARS] = {0};
    char client_password[PASSWORD_SIZE_CHARS] = {0};

    // Login / registration handshake
    P("[%d]::: Handling login...", connection_fd);

    //  1) Receive username 
    ssize_t received = recv(connection_fd, &login_env.sender, sizeof(login_env.sender), 0);
    if (unlikely(received <= 0))
    {
        PSE("::: Error receiving username for connection fd: %d", connection_fd);
        goto cleanup;
    }
    login_env.sender[USERNAME_SIZE_CHARS - 1] = '\0';                     // Defensive null-termination
    login_env.sender[strcspn(login_env.sender, "\r\n")] = '\0';           // Strip newline if present @note: \r is for Windows compatibility. I do not remember if the professor told us only for UNIX systems or not
    P("[%d]::: Read username [%s]", connection_fd, login_env.sender);

    //  2) Decide whether to register or authenticate
    char user_file_path[USERNAME_SIZE_CHARS + 4] = {0}; // room for simple suffix if i want, like ".txt"
    if (unlikely(snprintf(user_file_path, sizeof(user_file_path), "%s", login_env.sender) < 0))
    {
        PSE("::: Failed to build user file path for username: %s", login_env.sender);
        goto cleanup;
    }

    FILE *user_file = fopen(user_file_path, "r");
    // Now we start this massive if-else block, depending on whether the user file is found or not we REGISTER or AUTHENTICATE
    if (user_file == NULL)  // If user file is not found, then start registration
    {
        // User not found -> ask client to register (using code START_REGISTRATION)
        response_code = START_REGISTRATION;
        if (unlikely(send(connection_fd, &response_code, sizeof(response_code), 0) <= 0))
        {
            PSE("::: Failed to send START_REGISTRATION to client on fd: %d", connection_fd);
            goto cleanup;
        }
        P("[%d]::: Sent START_REGISTRATION to [%s]", connection_fd, login_env.sender);

        // Receive password chosen by the client
        received = recv(connection_fd, client_password, sizeof(client_password), 0);
        if (unlikely(received <= 0))
        {
            PSE("::: Failed to receive registration password for [%s]", login_env.sender);
            goto cleanup;
        }
        client_password[PASSWORD_SIZE_CHARS - 1] = '\0';           // Add null-termination just in case
        client_password[strcspn(client_password, "\r\n")] = '\0';  // Strip newline if present, also \r for Windows compatibility... yada yada 

        // Create user file and store password as the first line
        user_file = fopen(user_file_path, "w");
        if (unlikely(user_file == NULL))
        {
            PSE("::: Failed to create user file for [%s]", login_env.sender);
            goto cleanup;
        }
        if (unlikely(fprintf(user_file, "%s\n", client_password) < 0))
        {
            PSE("::: Failed to write password for new user [%s]", login_env.sender);
            fclose(user_file);
            goto cleanup;
        }
        fclose(user_file);

        response_code = NO_ERROR;
        if (unlikely(send(connection_fd, &response_code, sizeof(response_code), 0) <= 0))
        {
            PSE("::: Failed to confirm registration for [%s]", login_env.sender);
            goto cleanup;
        }
        P("[%d]::: Registered new user [%s]!!!", connection_fd, login_env.sender);
    }
    else // If user file is found, then proceed with authentication
    {
        // Read the password from the user file
        if (unlikely(fgets(stored_password, sizeof(stored_password), user_file) == NULL))
        {
            PSE("::: Failed to read stored password for user [%s]", login_env.sender);
            fclose(user_file);
            goto cleanup;
        }
        fclose(user_file);
        stored_password[strcspn(stored_password, "\r\n")] = '\0';

        // Notify client that the user exists and we expect a password
        response_code = NO_ERROR;
        if (unlikely(send(connection_fd, &response_code, sizeof(response_code), 0) <= 0))
        {
            PSE("::: Failed to send login-ready code to [%s]", login_env.sender);
            goto cleanup;
        }

        int attempts = 0;
        int authenticated = 0;  // boolean flag, 0 = false, 1 = true
        while (attempts < MAX_PASSWORD_ATTEMPTS && !authenticated)
        {
            received = recv(connection_fd, client_password, sizeof(client_password), 0);
            if (unlikely(received <= 0))
            {
                PSE("::: Failed to receive password attempt %d for [%s]", attempts + 1, login_env.sender);
                goto cleanup;
            }
            client_password[PASSWORD_SIZE_CHARS - 1] = '\0';
            client_password[strcspn(client_password, "\r\n")] = '\0';

            if (strcmp(client_password, stored_password) == 0)  // Passwords match case
            {
                authenticated = 1;
                response_code = NO_ERROR;
                P("[%d]::: User [%s] authenticated", connection_fd, login_env.sender);
            }
            else                                                // Passwords do not match case
            {
                attempts++;
                response_code = WRONG_PASSWORD;
                P("[%d]::: Wrong password for [%s] (attempt %d/%d)", connection_fd, login_env.sender, attempts, MAX_PASSWORD_ATTEMPTS);
            }

            if (unlikely(send(connection_fd, &response_code, sizeof(response_code), 0) <= 0))
            {
                PSE("::: Failed to send authentication result to [%s]", login_env.sender);
                goto cleanup;
            }

            if (!authenticated && attempts >= MAX_PASSWORD_ATTEMPTS)
            {
                P("[%d]::: Max password attempts reached for [%s]", connection_fd, login_env.sender);
                goto cleanup;
            }
        }
    }

    P("[%d]::: Login handled successfully for [%s]", connection_fd, login_env.sender);

cleanup:
    // Closing the connection before exiting the thread
    P("[%d]::: Closing connection fd: %d", connection_fd, connection_fd);
    if(unlikely(close(connection_fd) < 0))
    {
        PSE("Error closing connection fd: %d", connection_fd);
        E();
    }
    P("[%d]:::Connection fd: %d closed, thread exiting", connection_fd, connection_fd);
    pthread_exit(NULL);
    return NULL; // Defensive, should not be reached
}


/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                                   MAIN LOOP                                                   */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
int main(int, char**) // Unused parameters argc argv
{
    printf("Welcome to PGM server!\n");
    PIE("Testing PIE macro");
    PSE("Testing PSE macro");
    printf("Printing ascii art and name\n");
    fprintf(stdout, "%s", ascii_art);
    fprintf(stdout, "Program name: %s\n", program_name);

    // START SOCKET SERVER HERE
    // Creating a socket for internet communication using domain:IPv4 ; type:TCP ; protocol: default(0) 
    P("Creating socket...");
    int skt_fd;
    if(unlikely((skt_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)) // theoretically socket() can return 0, but it's very unlikely, and won't happen in this situation (0 is used for stdin) and it is not a problem anyway
    {
        PSE("Socket creation failed");
        E();
    }
    P("Socket created successfully, socket file descriptor: %d", skt_fd);
    
    struct sockaddr_in address = {0}; // Initializing the strct to 0
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP address (“accept connections on whatever IPs this host currently has.”)
    if (PORT_NUMBER > 0 && PORT_NUMBER < 65536)
    {
        address.sin_port = htons(PORT_NUMBER); // htons converts the port number from host byte order to network byte order (big endian)
    }
    else
    {
        P("Using ephemeral port");
        address.sin_port = 0; // Ephimeral port since we do not care, let the OS choose the first free one, actually a good thing since we do not want to worry about clashing port numbers
    }
    // Binding the socket to the specified IP address and port number
    if (unlikely(bind(skt_fd, (struct sockaddr *)&address, sizeof(address)) < 0))
    {
        PSE("Socket bind failed");
        E();
    }
    P("Socket binded successfully!");

    // Now get the port number and the IP address of the server
    socklen_t addrlen = sizeof(address);
    if (unlikely(getsockname(skt_fd, (struct sockaddr *)&address, &addrlen) == -1)) // We reuse the address struct to get the info out of the OS
    {
        PSE("getsockname returned an error");
        E();
    }
    // NOW WE PRINT THE IP ADDRESS AND PORT NUMBER
    // NB: inet_ntoa gets the IP address in string format from a struct in_addr (NOT THREAD SAFE, but we are not using threads right now)
    // But the man page says that the function is obsolete anyway and we should use inet_ntop()
        // inet_ntop() binary -> text
        // inet_pton() text -> binary
    // ntohs converts the port number from network byte order (big endian) to host byte order (little endian or whatever)
    
    uint16_t port = ntohs(address.sin_port);
    do{ // We use a do while loop to keep the stack clean
        char temp_addr_str[] = "ddd.ddd.ddd.ddd";                                                    // (String copied from linux man) Max length of an IPv4 address in string format is 15 characters + null terminator
                                                                                                     // If this string is printed we now there was an error somewhere
        if (unlikely(inet_ntop(AF_INET, &address.sin_addr, temp_addr_str, sizeof(address)) == NULL)) // AF_INET = IPV4 ; "src  points  to  a  struct  in_addr" ;
        {
            PSE("inet_ntop returned an error");
            E();
        }
        P("Server listening on IP: %s, Port: %u", temp_addr_str, port);
    }while (0);

    P("Now discovering machine IP addresses...");
    while(unlikely(print_local_ip_addresses(port) != NO_ERROR))
    {
        PSE("print_local_ip_addresses() returned an error, press any key to retry...");
        getchar();
    }
    P("You can copypaste any of these IP addresses on the client machine to connect to the server");

    if (unlikely(listen(skt_fd, MAX_BACKLOG) < 0)) // The second parameter is the backlog, the number of connections that can be waiting while the process is handling a particular connection, 3 is a good value for now
    {
        PSE("Socket listen failed");
        E();
    }
    P("Socket listening successfully! Max backlog: %d", MAX_BACKLOG);

    // Now we accept connections in loop, each connection will be handled by a different thread
    int thread_args_connections[MAX_BACKLOG]; // A circular array containing the fds of the connections to pass to the thread
    uint8_t thread_args_connections_index = 0;// The index of the array to keep track where to write
                                              // But won't the elements of the array be overwritten by the next connections?
                                              // No, since the size of the array is the size of the max backlog, so if an element is being overwritten then the thread assigned to that connetion had already closed said connection
    pthread_t thread_id_array[MAX_BACKLOG];   // The thread id array in which we store the thread ids
                                              // @note: we are not joining the threads anywhere, so this is not exactly useful, but may be in future implementations or for debugging
    while (1)
    {
        P("Waiting for incoming connections...");
        // We necessarily need to create this variables on stack since the accept function uses pointers to them
        int new_connection;
        struct sockaddr_in client_address;
        socklen_t client_addrlen = sizeof(client_address);
        if (unlikely((new_connection = accept(skt_fd, (struct sockaddr *)&client_address, &client_addrlen)) < 0))
        {
            PSE("Socket accept failed");
            continue; // We do not exit the program, we just continue to accept new connections
        }
        P("Connection accepted from IP: %s, Port: %d, New socket file descriptor: %d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), new_connection);
        // CREATE NEW THREAD HANDLING THE CONNECTION:
        thread_args_connections[thread_args_connections_index] = new_connection; // We store the new connection fd in the circular array
        if (unlikely(pthread_create(&thread_id_array[thread_args_connections_index], NULL, thread_routine, (void *)&thread_args_connections[thread_args_connections_index]) < 0))
        {
            PSE("Failed to create thread for connection fd: %d", new_connection);
            // We close the connection since we cannot handle it
            close(new_connection);
        }
        else{
            // Successfully created thread, increment the index in a circular manner
            thread_args_connections_index = (uint8_t)((thread_args_connections_index + 1) % MAX_BACKLOG); // Conversion to silence gcc lamenting
            P("Thread [%lu] created successfully for connection fd: %d\n", (unsigned long)thread_id_array[thread_args_connections_index], new_connection);
        }
        // close(new_connection); It's the thread's resposibility to close the socket when done
    }

    printf("Exiting program\n");
    return 0;
}
