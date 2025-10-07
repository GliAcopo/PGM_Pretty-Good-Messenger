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
#include <stdio.h>      // printf, fprintf
#include <stdlib.h>     // exit, EXIT_FAILURE, EXIT_SUCCESS
#include <sys/socket.h> // socket, bind, listen, accept
#include <netinet/in.h> // struct sockaddr_in, INADDR_ANY
#include <arpa/inet.h>  // inet_ntoa

// SIMPLE PRINT STATEMENT ON STDOUT
#define P(fmt, ...) do{fprintf(stdout,"[SRV]>>> " fmt "\n", ##__VA_ARGS__);}while(0);

// Max number of pending connections in the socket listen queue
#define MAX_BACKLOG 10

// ierror, an internal debug substitute to errno
ERROR_CODE ierrno = NO_ERROR;

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                                   MAIN LOOP                                                   */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
int main(int argc, char** argv)
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
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Accept connections from any IP address
    address.sin_port = 0;                 // Ephimeral port since we do not care, let the OS choose the first free one, actually a good thing since we do not want to worry about clashing port numbers
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
    do{ // We use a do while loop to keep the stack clean
        char temp_addr_str[] = "ddd.ddd.ddd.ddd"; // (String copied from linux man) Max length of an IPv4 address in string format is 15 characters + null terminator
                                            // If this string is printed we now there was an error somewhere
        if (unlikely(inet_ntop(AF_INET, &address.sin_addr, temp_addr_str, sizeof(address)) == NULL)) // AF_INET = IPV4 ; "src  points  to  a  struct  in_addr" ;
        {
            PSE("inet_ntop returned an error");
            E();
        }
        P("Server listening on IP: %s, Port: %d", temp_addr_str, ntohs(address.sin_port));
    }while (0);

    if (unlikely(listen(skt_fd, MAX_BACKLOG) < 0)) // The second parameter is the backlog, the number of connections that can be waiting while the process is handling a particular connection, 3 is a good value for now
    {
        PSE("Socket listen failed");
        E();
    }
    P("Socket listening successfully! Max backlog: %d", MAX_BACKLOG);

    // Now we accept connections in loop, each connection will be handled by a different thread
    while(1)
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
        // TODO
        // close(new_connection); It's the thread's resposibility to close the socket when done
    }

    printf("Exiting program\n");
    return 0;
}
