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
#include <fcntl.h>      // open
#include <pthread.h>    // pthread_create, pthread_t, pthread_exit
#include <sys/socket.h> // socket, bind, listen, accept
#include <netinet/in.h> // struct sockaddr_in, INADDR_ANY
#include <arpa/inet.h>  // inet_ntoa
#include <ifaddrs.h>    // struct ifaddrs, getifaddrs, freeifaddrs
#include <netdb.h>      // NI_MAXHOST, getnameinfo
#include <sys/stat.h>   // stat, mkdir
#include <dirent.h>     // opendir, readdir, closedir
#include <semaphore.h>  // sem_t, sem_init, sem_timedwait, sem_post
#include <time.h>       // clock_gettime
#include <stdint.h>     // uint32_t
#include <stddef.h>     // offsetof
// #include <linux/if_link.h> // IFLA_ADDRESS


// SIMPLE PRINT STATEMENT ON STDOUT
#define P(fmt, ...) do{fprintf(stdout,"[SRV]>>> " fmt "\n", ##__VA_ARGS__);}while(0);

// Max number of pending connections in the socket listen queue
#define MAX_BACKLOG 10

static char *current_loggedin_users[MAX_BACKLOG] = {0};
static unsigned int current_loggedin_users_bitmap = 0;
static sem_t current_loggedin_users_semaphore;

// ierror, an internal debug substitute to errno
ERROR_CODE ierrno = NO_ERROR;

// Default port to use in the server (can be overridden by argv or PGM_SERVER_PORT)
const int32_t DEFAULT_PORT_NUMBER = 6666; // log_2(65535) = 16 bits, 16-1= 15 (sign) so (to be sure) I decided to use a 32 bit 
const char *server_port_env = "PGM_SERVER_PORT";


/**
 * @brief Parse a decimal port string and return a validated port number.
 *
 * @param value    NUL-terminated string containing the port in base 10. If NULL or empty, the function returns @p fallback.
 * @param fallback Port to return when @p value is NULL/empty or invalid.
 * @param source   Short textual description used in diagnostic messages emitted via P(...).
 * @return The parsed port number as an int32_t, or @p fallback on error.
 *  - Calls strtol(value, &endptr, 10) with errno cleared beforehand.
 *  - If parsing fails (entire string not consumed, out of range, or other error), emits a diagnostic and returns @p fallback.
 *  - If the parsed port is in the privileged range (1..1023), returns 0
 *  - Otherwise returns the parsed port as an int32_t
 */
static int32_t parse_port_string(const char *value, int32_t fallback, const char *source)
{
    if (value == NULL || value[0] == '\0')
    {
        return fallback;
    }

    char *endptr = NULL;
    errno = 0;
    long port_long = strtol(value, &endptr, 10);
    if (endptr == value || *endptr != '\0' || errno != 0 || port_long < 0 || port_long > 65535)
    {
        P("Invalid %s port value [%s], using %d", source, value, fallback);
        return fallback;
    }
    if (port_long > 0 && port_long < 1024)
    {
        P("Port %ld from %s requires elevated privileges, using ephemeral port", port_long, source);
        return 0;
    }
    return (int32_t)port_long;
}

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

static void dump_loggedin_users(void)
{
    P("Logged in users bitmap: 0x%08X", current_loggedin_users_bitmap);
    for (int i = 0; i < MAX_BACKLOG; i++)
    {
        if (current_loggedin_users[i] != NULL)
        {
            P("\t[%d]: %s", i, current_loggedin_users[i]);
        }
    }
}

static void lock_loggedin_users_or_exit(void)
{
    int attempts = 0;
    while (attempts < MAX_AQUIRE_SEMAPHORE_RETRY)
    {
        struct timespec ts;
        if (unlikely(clock_gettime(CLOCK_REALTIME, &ts) == -1))
        {
            PSE("clock_gettime() failed while locking loggedin users");
#ifdef DEBUG
            dump_loggedin_users();
#endif
            E();
        }
        ts.tv_sec += 5;

        int rc;
        do
        {
            rc = sem_timedwait(&current_loggedin_users_semaphore, &ts);
        } while (rc == -1 && errno == EINTR);

        if (rc == 0)
        {
            return;
        }
        if (errno == ETIMEDOUT)
        {
            attempts++;
            P("Timed out waiting for current_loggedin_users_semaphore (%d/%d)", attempts, MAX_AQUIRE_SEMAPHORE_RETRY);
            continue;
        }
        PSE("sem_timedwait() failed for current_loggedin_users_semaphore");
        break;
    }

    P("Unable to acquire current_loggedin_users_semaphore, exiting");
#ifdef DEBUG
    dump_loggedin_users();
#endif
    E();
}

static void unlock_loggedin_users_or_exit(void)
{
    if (unlikely(sem_post(&current_loggedin_users_semaphore) == -1))
    {
        PSE("sem_post() failed for current_loggedin_users_semaphore");
#ifdef DEBUG
        dump_loggedin_users();
#endif
        E();
    }
}

static ERROR_CODE add_loggedin_user(const char *username, int *out_index)
{
    if (unlikely(username == NULL || out_index == NULL))
    {
        return NULL_PARAMETERS;
    }

    lock_loggedin_users_or_exit();
#ifdef DEBUG
    P("current_loggedin_users_bitmap before login: 0x%08X", current_loggedin_users_bitmap);
#endif
    for (int i = 0; i < MAX_BACKLOG; i++)
    {
        if (current_loggedin_users[i] != NULL && strcmp(current_loggedin_users[i], username) == 0)
        {
            unlock_loggedin_users_or_exit();
            return ERROR;
        }
    }

    char *name_copy = calloc(USERNAME_SIZE_CHARS, sizeof(char));
    if (unlikely(name_copy == NULL))
    {
        unlock_loggedin_users_or_exit();
        return SYSCALL_ERROR;
    }
    snprintf(name_copy, USERNAME_SIZE_CHARS, "%s", username);

    int slot = -1;
    for (int i = 0; i < MAX_BACKLOG; i++)
    {
        if ((current_loggedin_users_bitmap & (1u << i)) == 0)
        {
            slot = i;
            break;
        }
    }
    if (slot < 0)
    {
        free(name_copy);
        P("No free slot in current_loggedin_users_bitmap");
#ifdef DEBUG
        dump_loggedin_users();
#endif
        E();
    }

    current_loggedin_users[slot] = name_copy;
    current_loggedin_users_bitmap |= (1u << slot);
    *out_index = slot;
#ifdef DEBUG
    P("current_loggedin_users_bitmap after login: 0x%08X", current_loggedin_users_bitmap);
#endif
    unlock_loggedin_users_or_exit();

    return NO_ERROR;
}

static void remove_loggedin_user(int index)
{
    if (index < 0 || index >= MAX_BACKLOG)
    {
        return;
    }

    lock_loggedin_users_or_exit();
#ifdef DEBUG
    P("current_loggedin_users_bitmap before logout: 0x%08X", current_loggedin_users_bitmap);
#endif
    if (current_loggedin_users[index] != NULL)
    {
        free(current_loggedin_users[index]);
        current_loggedin_users[index] = NULL;
    }
    current_loggedin_users_bitmap &= ~(1u << index);
#ifdef DEBUG
    P("current_loggedin_users_bitmap after logout: 0x%08X", current_loggedin_users_bitmap);
#endif
    unlock_loggedin_users_or_exit();
}

static int send_all(int fd, const void *buffer, size_t length)
{
    const char *cursor = buffer;
    size_t remaining = length;
    while (remaining > 0)
    {
        ssize_t sent = send(fd, cursor, remaining, 0);
        if (sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (sent == 0)
        {
            return -1;
        }
        cursor += sent;
        remaining -= (size_t)sent;
    }
    return 0;
}

static int recv_all(int fd, void *buffer, size_t length)
{
    char *cursor = buffer;
    size_t remaining = length;
    while (remaining > 0)
    {
        ssize_t recvd = recv(fd, cursor, remaining, 0);
        if (recvd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (recvd == 0)
        {
            return 0;
        }
        cursor += recvd;
        remaining -= (size_t)recvd;
    }
    return 1;
}

static int ends_with(const char *value, const char *suffix)
{
    size_t value_len = strlen(value);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > value_len)
    {
        return 0;
    }
    return strcmp(value + (value_len - suffix_len), suffix) == 0;
}

static char *build_registered_users_list(size_t *out_len)
{
    if (out_len == NULL)
    {
        return NULL;
    }

    DIR *dir = opendir(".");
    if (dir == NULL)
    {
        return NULL;
    }

    size_t used = 0;
    size_t cap = 0;
    char *list = NULL;
    struct dirent *entry = NULL;
    size_t suffix_len = strlen(folder_suffix_user);

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        if (!ends_with(entry->d_name, folder_suffix_user))
        {
            continue;
        }

        struct stat st = {0};
        if (stat(entry->d_name, &st) != 0)
        {
            continue;
        }
        if (!S_ISDIR(st.st_mode))
        {
            continue;
        }

        size_t name_len = strlen(entry->d_name) - suffix_len;
        size_t needed = name_len + 1;
        if (used + needed + 1 > cap)
        {
            size_t next_cap = cap == 0 ? 128 : cap * 2;
            while (used + needed + 1 > next_cap)
            {
                next_cap *= 2;
            }
            char *new_list = realloc(list, next_cap);
            if (new_list == NULL)
            {
                free(list);
                closedir(dir);
                return NULL;
            }
            list = new_list;
            cap = next_cap;
        }

        memcpy(list + used, entry->d_name, name_len);
        used += name_len;
        list[used++] = '\n';
    }

    closedir(dir);

    if (list == NULL)
    {
        list = malloc(1);
        if (list == NULL)
        {
            return NULL;
        }
        list[0] = '\0';
        *out_len = 1;
        return list;
    }

    if (used + 1 > cap)
    {
        char *new_list = realloc(list, used + 1);
        if (new_list == NULL)
        {
            free(list);
            return NULL;
        }
        list = new_list;
    }
    list[used++] = '\0';
    *out_len = used;
    return list;
}

static int sanitize_username(const char *value)
{
    if (value == NULL || value[0] == '\0')
    {
        return 0;
    }
    if (strstr(value, "..") != NULL)
    {
        return 0;
    }
    for (const char *cursor = value; *cursor != '\0'; cursor++)
    {
        if (*cursor == '/' || *cursor == '\\')
        {
            return 0;
        }
    }
    return 1;
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
    int current_loggedin_users_used_index = -1;
    char *user_dir_path = NULL;
    char *password_path = NULL;
    char *data_path = NULL;

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
    login_env.sender[strcspn(login_env.sender, "\n")] = '\0';           // Strip newline if present @note: \r for Windows compatibility not needed
    P("[%d]::: Read username [%s]", connection_fd, login_env.sender);

    //  2) Decide whether to register or authenticate
    size_t user_dir_len = strlen(login_env.sender) + strlen(folder_suffix_user) + 1;
    user_dir_path = calloc(user_dir_len, sizeof(char));
    if (unlikely(user_dir_path == NULL))
    {
        PSE("::: Failed to allocate user directory path for username: %s", login_env.sender);
        goto cleanup;
    }
    if (unlikely(snprintf(user_dir_path, user_dir_len, "%s%s", login_env.sender, folder_suffix_user) < 0))
    {
        PSE("::: Failed to build user directory path for username: %s", login_env.sender);
        goto cleanup;
    }

    struct stat user_dir_stat = {0};
    int user_dir_missing = 0;
    if (stat(user_dir_path, &user_dir_stat) == 0) // From the linux man: stat = Get file attributes for FILE and put the in BUFF
    {
        if (!S_ISDIR(user_dir_stat.st_mode))
        {
            PSE("::: User path exists but is not a directory for username: %s", login_env.sender);
            goto cleanup;
        }
    }
    else
    {
        if (errno == ENOENT)
        {
            user_dir_missing = 1;
        }
        else
        {
            PSE("::: Failed to stat user directory for username: %s", login_env.sender);
            goto cleanup;
        }
    }

    size_t password_path_len = strlen(user_dir_path) + 1 + strlen(password_filename) + 1;
    password_path = calloc(password_path_len, sizeof(char));
    if (unlikely(password_path == NULL))
    {
        PSE("::: Failed to allocate password file path for username: %s", login_env.sender);
        goto cleanup;
    }
    if (unlikely(snprintf(password_path, password_path_len, "%s/%s", user_dir_path, password_filename) < 0))
    {
        PSE("::: Failed to build password file path for username: %s", login_env.sender);
        goto cleanup;
    }

    size_t data_path_len = strlen(user_dir_path) + 1 + strlen(data_filename) + 1;
    data_path = calloc(data_path_len, sizeof(char));
    if (unlikely(data_path == NULL))
    {
        PSE("::: Failed to allocate data file path for username: %s", login_env.sender);
        goto cleanup;
    }
    if (unlikely(snprintf(data_path, data_path_len, "%s/%s", user_dir_path, data_filename) < 0))
    {
        PSE("::: Failed to build data file path for username: %s", login_env.sender);
        goto cleanup;
    }

    FILE *password_file = NULL;
    // Now we start this massive if-else block, depending on whether the user folder is found or not we REGISTER or AUTHENTICATE
    if (user_dir_missing)  // If user folder is not found, then start registration
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

        // Create user folder
        if (unlikely(mkdir(user_dir_path, 0700) == -1 && errno != EEXIST))
        {
            PSE("::: Failed to create user folder for [%s]", login_env.sender);
            goto cleanup;
        }
        P("[%d]::: Created user folder [%s] for [%s]", connection_fd, user_dir_path, login_env.sender);

        // Create password file and store password as the first line
        password_file = fopen(password_path, "w");
        if (unlikely(password_file == NULL))
        {
            PSE("::: Failed to create password file for [%s]", login_env.sender);
            goto cleanup;
        }
        P("[%d]::: Created password file [%s] for [%s]", connection_fd, password_path, login_env.sender);
        if (unlikely(fprintf(password_file, "%s\n", client_password) < 0))
        {
            PSE("::: Failed to write password for new user [%s]", login_env.sender);
            fclose(password_file);
            goto cleanup;
        }
        P("[%d]::: Stored password for new user [%s]", connection_fd, login_env.sender);
        fclose(password_file);

        // Create data file and initialize received message count
        FILE *data_file = fopen(data_path, "w");
        if (unlikely(data_file == NULL))
        {
            PSE("::: Failed to create data file for [%s]", login_env.sender);
            goto cleanup;
        }
        P("[%d]::: Created data file [%s] for [%s]", connection_fd, data_path, login_env.sender);
        if (unlikely(fprintf(data_file, "%u\n", 0u) < 0))
        {
            PSE("::: Failed to initialize data file for [%s]", login_env.sender);
            fclose(data_file);
            goto cleanup;
        }
        P("[%d]::: Initialized data file [%s] for new user [%s]", connection_fd, data_path, login_env.sender);
        fclose(data_file);

        ERROR_CODE add_code = add_loggedin_user(login_env.sender, &current_loggedin_users_used_index);
        if (unlikely(add_code != NO_ERROR))
        {
            response_code = add_code;
            if (unlikely(send(connection_fd, &response_code, sizeof(response_code), 0) <= 0))
            {
                PSE("::: Failed to send registration rejection for [%s]", login_env.sender);
            }
            goto cleanup;
        }

        response_code = NO_ERROR;
        if (unlikely(send(connection_fd, &response_code, sizeof(response_code), 0) <= 0))
        {
            PSE("::: Failed to confirm registration for [%s]", login_env.sender);
            goto cleanup;
        }
        P("[%d]::: Registered new user [%s]!!!", connection_fd, login_env.sender);
    }
    else // If user folder is found, then proceed with authentication
    {
        // Read the password from the user password file
        password_file = fopen(password_path, "r");
        if (unlikely(password_file == NULL))
        {
            PSE("::: Failed to open password file for user [%s]", login_env.sender);
            goto cleanup;
        }
        if (unlikely(fgets(stored_password, sizeof(stored_password), password_file) == NULL))
        {
            PSE("::: Failed to read stored password for user [%s]", login_env.sender);
            fclose(password_file);
            goto cleanup;
        }
        fclose(password_file);
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
                ERROR_CODE add_code = add_loggedin_user(login_env.sender, &current_loggedin_users_used_index);
                if (unlikely(add_code != NO_ERROR))
                {
                    response_code = add_code;
                    if (unlikely(send(connection_fd, &response_code, sizeof(response_code), 0) <= 0))
                    {
                        PSE("::: Failed to send login rejection to [%s]", login_env.sender);
                    }
                    goto cleanup;
                }

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

    /* -------------------------------------------------------------------------- */
    /*                          MESSAGE SENDING TO USERS                          */
    /* -------------------------------------------------------------------------- */

    while (1)
    {
        MESSAGE_CODE request_code = MESSAGE_ERROR;
        ssize_t recvd;
        do
        {
            recvd = recv(connection_fd, &request_code, sizeof(request_code), MSG_WAITALL);
        } while (recvd < 0 && errno == EINTR);

        if (recvd == 0)
        {
            P("[%d]::: Client disconnected", connection_fd);
            break;
        }
        if (recvd < 0)
        {
            PSE("::: Failed to receive MESSAGE_CODE for [%s]", login_env.sender);
            goto cleanup;
        }
        if (recvd != sizeof(request_code))
        {
            P("[%d]::: Incomplete MESSAGE_CODE received (%zd bytes)", connection_fd, recvd);
            goto cleanup;
        }

        int handled = 0;
        switch (request_code)
        {
        case REQUEST_SEND_MESSAGE:
        {
            P("[%d]::: REQUEST_SEND_MESSAGE received", connection_fd);

            size_t header_size = offsetof(MESSAGE, message);
            MESSAGE *header = calloc(1, header_size);
            if (header == NULL)
            {
                PSE("::: Failed to allocate message header");
                goto cleanup;
            }
            int header_recv = recv_all(connection_fd, header, header_size);
            if (header_recv <= 0)
            {
                PSE("::: Failed to receive MESSAGE header for [%s]", login_env.sender);
                free(header);
                goto cleanup;
            }

            header->sender[USERNAME_SIZE_CHARS - 1] = '\0';
            header->recipient[USERNAME_SIZE_CHARS - 1] = '\0';
            header->sender[strcspn(header->sender, "\r\n")] = '\0';
            header->recipient[strcspn(header->recipient, "\r\n")] = '\0';
            snprintf(header->sender, sizeof(header->sender), "%s", login_env.sender);

            uint32_t message_length = ntohl(header->message_length);
            if (message_length == 0 || message_length > MESSAGE_SIZE_CHARS)
            {
                ERROR_CODE invalid = STRING_SIZE_INVALID;
                if (unlikely(send_all(connection_fd, &invalid, sizeof(invalid)) < 0))
                {
                    PSE("::: Failed to send STRING_SIZE_INVALID to [%s]", login_env.sender);
                }
                free(header);
                handled = 1;
                break;
            }

            if (!sanitize_username(header->recipient))
            {
                ERROR_CODE not_found = USER_NOT_FOUND;
                if (unlikely(send_all(connection_fd, &not_found, sizeof(not_found)) < 0))
                {
                    PSE("::: Failed to send USER_NOT_FOUND to [%s]", login_env.sender);
                }
                free(header);
                handled = 1;
                break;
            }

            size_t recipient_dir_len = strlen(header->recipient) + strlen(folder_suffix_user) + 1;
            char *recipient_dir = calloc(recipient_dir_len, sizeof(char));
            if (recipient_dir == NULL)
            {
                PSE("::: Failed to allocate recipient directory path for [%s]", header->recipient);
                free(header);
                handled = 1;
                break;
            }
            if (unlikely(snprintf(recipient_dir, recipient_dir_len, "%s%s", header->recipient, folder_suffix_user) < 0))
            {
                PSE("::: Failed to build recipient directory path for [%s]", header->recipient);
                free(recipient_dir);
                free(header);
                goto cleanup;
            }

            struct stat recipient_stat = {0};
            if (stat(recipient_dir, &recipient_stat) != 0 || !S_ISDIR(recipient_stat.st_mode))
            {
                ERROR_CODE not_found = USER_NOT_FOUND;
                if (unlikely(send_all(connection_fd, &not_found, sizeof(not_found)) < 0))
                {
                    PSE("::: Failed to send USER_NOT_FOUND to [%s]", login_env.sender);
                }
                free(recipient_dir);
                free(header);
                handled = 1;
                break;
            }

            ERROR_CODE ok = NO_ERROR;
            if (unlikely(send_all(connection_fd, &ok, sizeof(ok)) < 0))
            {
                PSE("::: Failed to send NO_ERROR to [%s]", login_env.sender);
                free(recipient_dir);
                free(header);
                goto cleanup;
            }

            char *body = calloc(message_length, sizeof(char));
            if (body == NULL)
            {
                PSE("::: Failed to allocate message body");
                free(header);
                goto cleanup;
            }
            int body_recv = recv_all(connection_fd, body, message_length);
            if (body_recv <= 0)
            {
                PSE("::: Failed to receive MESSAGE body for [%s]", login_env.sender);
                free(body);
                free(header);
                goto cleanup;
            }

            time_t now = time(NULL);
            struct tm now_tm = {0};
            if (localtime_r(&now, &now_tm) == NULL)
            {
                PSE("::: Failed to get local time");
                free(recipient_dir);
                free(body);
                free(header);
                goto cleanup;
            }
            char timestamp[16] = {0};
            if (strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", &now_tm) == 0)
            {
                PSE("::: Failed to format timestamp");
                free(recipient_dir);
                free(body);
                free(header);
                goto cleanup;
            }

            int msg_fd = -1;
            char message_path[USERNAME_SIZE_CHARS + 64] = {0};
            for (unsigned int counter = 0; counter < 1000; counter++)
            {
                if (counter == 0)
                {
                    snprintf(message_path, sizeof(message_path), "%s/UNREAD%s%s",
                             recipient_dir, timestamp, file_suffix_user_data);
                }
                else
                {
                    snprintf(message_path, sizeof(message_path), "%s/UNREAD%s%u%s",
                             recipient_dir, timestamp, counter, file_suffix_user_data);
                }

                msg_fd = open(message_path, O_CREAT | O_EXCL | O_WRONLY, 0600);
                if (msg_fd >= 0)
                {
                    break;
                }
                if (errno != EEXIST)
                {
                    PSE("::: Failed to open message file for [%s]", header->recipient);
                    break;
                }
            }

            if (msg_fd < 0)
            {
                PSE("::: Unable to create message file for [%s]", header->recipient);
                free(recipient_dir);
                free(body);
                free(header);
                goto cleanup;
            }

            if (unlikely(write(msg_fd, header, header_size) != (ssize_t)header_size))
            {
                PSE("::: Failed to write message header to file");
                close(msg_fd);
                free(recipient_dir);
                free(body);
                free(header);
                goto cleanup;
            }
            if (unlikely(write(msg_fd, body, message_length) != (ssize_t)message_length))
            {
                PSE("::: Failed to write message body to file");
                close(msg_fd);
                free(recipient_dir);
                free(body);
                free(header);
                goto cleanup;
            }
            close(msg_fd);

            free(recipient_dir);
            free(body);
            free(header);
            handled = 1;
            break;
        }
        case REQUEST_LIST_REGISTERED_USERS:
        {
            P("[%d]::: REQUEST_LIST_REGISTERED_USERS received", connection_fd);
            size_t list_len = 0;
            char *list = build_registered_users_list(&list_len);
            if (list == NULL)
            {
                PSE("::: Failed to build registered users list");
                goto cleanup;
            }
            if (list_len > UINT32_MAX)
            {
                PSE("::: Registered users list too large");
                free(list);
                goto cleanup;
            }
            uint32_t list_len_net = htonl((uint32_t)list_len);
            if (unlikely(send_all(connection_fd, &list_len_net, sizeof(list_len_net)) < 0))
            {
                PSE("::: Failed to send list length to [%s]", login_env.sender);
                free(list);
                goto cleanup;
            }

            ERROR_CODE ack = ERROR;
            if (unlikely(recv_all(connection_fd, &ack, sizeof(ack)) <= 0))
            {
                PSE("::: Failed to receive list ack from [%s]", login_env.sender);
                free(list);
                goto cleanup;
            }
            if (ack != NO_ERROR)
            {
                P("[%d]::: Client aborted list users", connection_fd);
                free(list);
                handled = 1;
                break;
            }

            if (unlikely(send_all(connection_fd, list, list_len) < 0))
            {
                PSE("::: Failed to send users list to [%s]", login_env.sender);
                free(list);
                goto cleanup;
            }
            free(list);
            handled = 1;
            break;
        }
        case REQUEST_LOAD_PREVIOUS_MESSAGES:
            P("[%d]::: REQUEST_LOAD_PREVIOUS_MESSAGES received (not implemented)", connection_fd);
            handled = 1;
            break;
        case REQUEST_LOAD_MESSAGE:
            P("[%d]::: REQUEST_LOAD_MESSAGE received (not implemented)", connection_fd);
            handled = 1;
            break;
        case REQUEST_LOAD_SPECIFIC_MESSAGE:
            P("[%d]::: REQUEST_LOAD_SPECIFIC_MESSAGE received (not implemented)", connection_fd);
            handled = 1;
            break;
        case REQUEST_LOAD_UNREAD_MESSAGES:
            P("[%d]::: REQUEST_LOAD_UNREAD_MESSAGES received (not implemented)", connection_fd);
            handled = 1;
            break;
        case REQUEST_DELETE_MESSAGE:
            P("[%d]::: REQUEST_DELETE_MESSAGE received (not implemented)", connection_fd);
            handled = 1;
            break;
        case LOGOUT:
            P("[%d]::: LOGOUT received", connection_fd);
            handled = 1;
            goto cleanup;
        default:
            P("[%d]::: Unknown MESSAGE_CODE [%d]", connection_fd, request_code);
            break;
        }

        if (!handled)
        {
            MESSAGE_CODE response_code_msg = MESSAGE_ERROR;
            if (unlikely(send(connection_fd, &response_code_msg, sizeof(response_code_msg), 0) <= 0))
            {
                PSE("::: Failed to send MESSAGE_ERROR to [%s]", login_env.sender);
                goto cleanup;
            }
        }
    }

cleanup:
    if (current_loggedin_users_used_index >= 0)
    {
        remove_loggedin_user(current_loggedin_users_used_index);
        current_loggedin_users_used_index = -1;
    }
    if (user_dir_path != NULL)
    {
        free(user_dir_path);
    }
    if (password_path != NULL)
    {
        free(password_path);
    }
    if (data_path != NULL)
    {
        free(data_path);
    }
    // Closing the connection before exiting the thread
    P("[%d]::: Closing connection fd: %d", connection_fd, connection_fd);
    if(unlikely(close(connection_fd) < 0))
    {
        PSE("Error closing connection fd: %d", connection_fd);
        E();
    }
    P("[%d]:::Connection fd: %d closed, thread exiting", connection_fd, connection_fd);
    pthread_exit(NULL);
    return NULL; // Should not be reached
}


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

    if (unlikely(sem_init(&current_loggedin_users_semaphore, 0, 1) == -1))
    {
        PSE("sem_init() failed for current_loggedin_users_semaphore");
        E();
    }

    int32_t port_number = DEFAULT_PORT_NUMBER;
    const char *env_port = getenv(server_port_env);
    if (env_port != NULL)
    {
        port_number = parse_port_string(env_port, port_number, "environment");
    }
    if (argc >= 2)
    {
        port_number = parse_port_string(argv[1], port_number, "argument");
    }

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
    if (port_number > 0 && port_number < 65536)
    {
        address.sin_port = htons((uint16_t)port_number); // htonl converts the port number from host byte order to network byte order (big endian)
                                                         // WARNING: before I used htons here, but it was a mistake since htons is for short (16 bit) and port numbers are 32 bit
                                                         // WARNING 2: actually I tried htonl but this caused the server to bind to unexpected ports??? So I guess that port numbers are 16 bit, but htons expects a uint16_t as input, so we cast it to uint16_t
        P("Using specified port: %d", port_number);
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
