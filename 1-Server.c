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
                        // WARNING: apparently this must be defined before any other inclusion, (Learned the hard way, (do not remove this line))
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
#include <string.h>     // strcmp, strncmp, strlen, memcpy, strstr
#include <errno.h>      // errno, EINTR, ETIMEDOUT
#include <signal.h>     // sigset_t, sigwait, SIGINT, SIGTERM
// #include <linux/if_link.h> // IFLA_ADDRESS


// SIMPLE PRINT STATEMENT ON STDOUT
#define P(fmt, ...) do{fprintf(stdout,"[SRV]>>> " fmt "\n", ##__VA_ARGS__);}while(0);

// Max number of pending connections in the socket listen queue
// Also used as the maximum number of logged in users / worker threads active at a time
#define MAX_BACKLOG 10

// CURRENT LOGGED IN USERS HANDLING
static char *current_loggedin_users[MAX_BACKLOG] = {0}; // Array of pointers to the usernames of the currently logged in users, if a slot is NULL, then it is free, otherwise it contains the username of the logged in user. WARNING: the index of the slot is NOT used to identify the user in other arrays (thread_id_array and connections_array)
static unsigned int current_loggedin_users_bitmap = 0;  // Which slots in the current_loggedin_users array are used, bit i is 1 if current_loggedin_users[i] contains the name of a loggedin user
static sem_t current_loggedin_users_semaphore;


// SHUTDOWN HANDLING
static size_t number_of_current_logged_in_users = 0;
// The thread id array in which we store the thread ids for joining when shutting down
static pthread_t thread_id_array[MAX_BACKLOG] = {0};
// The array of the connection file descriptors to close all the connections
static int connections_array[MAX_BACKLOG] = {0};
// Semaphore for the THREE above:
static sem_t shutdown_arrays_semaphore;

// ierror, an internal debug substitute to errno when needed
ERROR_CODE ierrno = NO_ERROR;

// Default port to use in the server (can be overridden by argv or PGM_SERVER_PORT)
static const int32_t DEFAULT_PORT_NUMBER = 6666; // log_2(65535) = 16 bits, 16-1= 15 (sign) so (to be sure) I decided to use a 32 bit 
static const char *server_port_env = "PGM_SERVER_PORT";

// Variable to shut down the server when needed, 0 false 1 true
static volatile sig_atomic_t shutdown_now = 0;


/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                          HELPER FUNCTIONS                                                     */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

static void exit_thread_and_update_thread_id_array(int thread_index)
{
    if (thread_index < 0 || thread_index >= MAX_BACKLOG)
    {
        P("Invalid thread index for exit_thread_and_update_thread_id_array: %d", thread_index);
        pthread_exit(NULL);
    }

    unsigned int max_retries = 5;
    while(unlikely(sem_wait(&shutdown_arrays_semaphore) < 0)) // All of these functions return 0 on success
    {
        if (unlikely(!max_retries--))
        {
            P("sem_wait() failed to aquire semaphore after max_retries retries");
            pthread_exit(NULL);
        }
        PSE("sem_wait() failed to aquire semaphore");
        sched_yield(); // LINUX MAN: sched_yield() causes the calling thread to relinquish the CPU.  The thread is moved to the end of the queue for its static priority and a new thread gets to run.
        continue; // Try again
    }

    if(!number_of_current_logged_in_users--) // Decrement the count of logged in users since this thread is exiting
    {
        P("Warning: number_of_current_logged_in_users is already 0 when exiting thread, this should not happen");
        number_of_current_logged_in_users = 0;
    }
    thread_id_array[thread_index] = 0;  // Clear the thread id from the array
    connections_array[thread_index] = 0; // Clear the connection fd from the array

    while(unlikely(sem_post(&shutdown_arrays_semaphore) < 0))
    {
        PSE("sem_post() failed to release semaphore");
        if (unlikely(!max_retries--))
        {
            P("sem_post() failed to release semaphore after max_retries retries");
            pthread_exit(NULL);
        }
        sched_yield(); // LINUX MAN: sched_yield() causes the calling thread to relinquish the CPU.  The thread is moved to the end of the queue for its static priority and a new thread gets to run.
        continue; // Try again
    }
    pthread_exit(NULL);
}

/**
 * @brief The function will parse a string (port) and convert it into a number (also doing validation checks).
 *
 * @param string_port    NULL-terminated string that contains the port in base 10. If NULL or empty, the function will returns @p port_desired_fallback
 * @param port_desired_fallback Port that gets returned when @p string_port does not satisfy requirements
 * @return The parsed port number as an int32_t, or @p port_desired_fallback
 */
static int32_t parse_port_string(const char *string_port, int32_t port_desired_fallback)
{
    if (unlikely(string_port == NULL || string_port[0] == '\0')) // If the string is empty or a null pointer
    {
        return port_desired_fallback;
    }

    long PORT_MIN = 0, PORT_MAX = 65535, RESERVED_MAX = 1023;

    char *endptr = NULL;
    errno = 0; // reset errno so no false alarms

    long port_long = strtol(string_port, &endptr, 10); // convert string port to long

    // First check if the string was actually read and then check if the port is in the valid range of ports
    if (unlikely(endptr == string_port || (endptr && *endptr != '\0') || errno != 0 || port_long < PORT_MIN || port_long > PORT_MAX))
    {
        P("Invalid port string_port [%s], using fallback: %d", string_port, port_desired_fallback);
        return port_desired_fallback;
    }

    if (unlikely(port_long > 0 && port_long <= RESERVED_MAX)) // If the chosen port is reserved
    {
        P(" The Port %ld is probably a reserved port on UNIX systems, defaulting to ephemeral", port_long);
        return 0;
    }

    return (int32_t)port_long; // Conversion to silence gcc
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
        if (likely(current_loggedin_users[i] != NULL))
            P("\t[%d]: %s", i, current_loggedin_users[i]); // \t tabulates output, so that the user sees pretty output
    }
}

/**
 * @brief: function that tries to aquire the semaphore to the loggedin users array before triying to modify it (e.g. another thread needs to add a user that logged in)
 */
static void lock_loggedin_users_or_exit(void)
{
    for (int attempt = 1; attempt <= MAX_AQUIRE_SEMAPHORE_RETRY; attempt++) // MAX_AQUIRE_SEMAPHORE_RETRY Defined in 1-Server.h
    {
        /** From the linux man page for clock_gettime
         int clock_gettime(clockid_t clockid, struct timespec *tp);
         */
        // From the linux man page for timespec
        //  struct timespec {
        //     time_t     tv_sec;   /* Seconds */
        //    /* ... */  tv_nsec;  /* Nanoseconds [0, 999'999'999] */
        //   };
        
        struct timespec ts;

        if (unlikely(clock_gettime(CLOCK_REALTIME, &ts) == -1))
        {
            PSE("clock_gettime() failed while locking loggedin users");
            #ifdef DEBUG
                dump_loggedin_users();
            #endif
            E();
        }

        ts.tv_sec += MAX_AQUIRE_SEMAPHORE_TIME_WAIT_SECONDS; // Defined in 1-Server.h

        for (;;) //for-ever muahahaha
        {
            if (likely(sem_timedwait(&current_loggedin_users_semaphore, &ts) == 0))
                return; // No error return

            if (likely(errno == EINTR)) // Interruption because of signal, this code should be unreachable since the signal are masked, but I do not want to delete this check because I wrote it
                continue;
            else{
                // Something gone wrong
                PSE("sem_timedwait() failed :(");
            }
            break;
        }

        if (unlikely(errno == ETIMEDOUT)) // ETIMEDOUT: (sem_timedwait()) The call timed out before the semaphore could be locked.
        {
            P("Timed out waiting for current_loggedin_users_semaphore (%d/%d)", attempt, MAX_AQUIRE_SEMAPHORE_RETRY);
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

/** 
 * @brief: this function assures that all data that needs to be sent is sent through the socket, if some data is left, then  */
static int send_all(int fd, const void *buffer, size_t length)
{
    const char *p = (const char *)buffer;
    size_t left = length;

    while (left)
    {
        const ssize_t n = send(fd, p, left, 0);

        if (likely(n > 0))
        {
            p += n;
            left -= (size_t)n;
            continue;
        }

        if (n == 0)
            return -1;

        if (errno == EINTR)
            continue;

        return -1;
    }

    return 0;
}

static int recv_all(int fd, void *buffer, size_t length)
{
    char *p = (char *)buffer;
    size_t left = length;

    while (left)
    {
        const ssize_t n = recv(fd, p, left, 0);

        if (likely(n > 0))
        {
            p += n;
            left -= (size_t)n;
            continue;
        }

        if (n == 0)
            return 0;

        if (errno == EINTR)
            continue;

        return -1;
    }

    return 1;
}

/** 
 * @brief super easy helper function to check if a string ends with a certain suffix
 * It ain't much but keeps things clean
 * @return boolean
 */
static int ends_with(const char *value, const char *suffix)
{
    // value_len = strlen(value);
    // suffix_len = strlen(suffix);
    if (strlen(suffix) > strlen(value))
    {
        return 0;
    }
    return strcmp(value + (strlen(value) - strlen(suffix)), suffix) == 0;
}

/** 
 * @brief super easy helper function to check if a string starts with a certain prefix
 * Like said above
 * @return boolean
 */
static int starts_with(const char *value, const char *prefix)
{
    // value_len is the strlen(value);
    // prefix_len is the strlen(prefix);
    if (strlen(prefix) > strlen(value))
    {
        return 0;
    }
    return strncmp(value, prefix, strlen(prefix)) == 0;
}

/**
 * @brief function that builds the list of REGISTERED (not online) users by scanning the current directory for folders that end with the folder suffix defined in 3-Global-Variables-and-Functions.h (e.g. "_user")
 * 
 * @param out_len 
 * @return char* 
 */
static char *build_list_of_registered_users(size_t *output_length) // We use a parameter to tell the reader the lenght of the buffer we return
{
    if (output_length == NULL)
    {
        return NULL;
    }

    /* The  opendir() function opens a directory stream corresponding to the directory name, and returns a pointer to
       the directory stream.  The stream is positioned at the first entry in the directory.
       The opendir() and fdopendir() functions return a pointer to the directory stream.  On error, NULL is returned,
       and errno is set to indicate the error.
    */
    DIR *directory = opendir("."); // Open the dir the application is running in
    if (unlikely(directory == NULL))
    {
        PSE("opendir() failed");
        return NULL;
    }

    size_t bytes_used = 0; // Bytes already used to store names in the user_list_buffer buffer
    size_t buffer_capacity = 0; // The current max capacity of the user_list_buffer buffer
    char *user_list_buffer = NULL; // Buffer that will hold the list of registered users
    struct dirent *directory_entry = NULL; // Points to the current directory entry
    size_t folder_suffix_length = strlen(folder_suffix_user); // calculate only once
    /* The  readdir()  function  returns a pointer to a dirent structure representing the next directory entry in the
       directory stream pointed to by dirp.  It returns NULL on reaching the end of the directory stream or if an er-
       ror occurred.

       In the glibc implementation, the dirent structure is defined as follows:

           struct dirent {
               ino_t          d_ino;       // /* Inode number *
               off_t          d_off;       // /* Not an offset; see below *
               unsigned short d_reclen;    // /* Length of this record *
               unsigned char  d_type;      // /* Type of file; not supported
                                           //   by all filesystem types *
               char           d_name[256]; // /* Null-terminated filename *
           };
    */
    while ((directory_entry = readdir(directory)) != NULL) // While there are still entries to read within the directory we check if they are user folders
    {
        if (!ends_with(directory_entry->d_name, folder_suffix_user)) // If it is not a user we do not care
        {
            continue;
        }
        //else

        // check if the entry is actually a user DIRECTORY and not just a file that ends with the user suffix because the application would explode if that would be the case
        /*
        struct stat entry_stat = {0};
        if (stat(directory_entry->d_name, &entry_stat) != 0)
        {
            continue;
        }
        if (!S_ISDIR(entry_stat.st_mode))
        {
            continue;
        }
        */

        // Extract username from the folder name
        size_t username_length = strlen(directory_entry->d_name) - folder_suffix_length;
        size_t bytes_needed = username_length + 1;
        
        // Check if current buffer has enough space for: used bytes + new username + newline + null terminator, if not increase size
        if (bytes_used + bytes_needed + 1 > buffer_capacity)
        {
            size_t next_capacity;
            
            // For the first time start with 128 bytes
            if (buffer_capacity == 0)
            {
                next_capacity = 128;
            }
            else
            {
                // Increase capacity since it is too small to hold the names
                next_capacity = buffer_capacity + 128;
            }
            
            
            while (bytes_used + bytes_needed + 1 > next_capacity) // Increase capacity until it's large enough to hold all data
            {
                next_capacity += 128; // Increase by chunks of 128 bytes 
            }
            
            // Resize the buffer to the new capacity using another variable because if the allocation fails we also lose the handle to free the old buffer
            char *reallocated_list = realloc(user_list_buffer, next_capacity);
            if (unlikely(reallocated_list == NULL))
            {
                PSE("Failed to allocate memory for user list buffer");
                free(user_list_buffer);
                closedir(directory);
                return NULL;
            }
            
            // Update the pointer
            user_list_buffer = reallocated_list;
            buffer_capacity = next_capacity; // Keep track of new capacity
        }

        // Copy the username (without the suffix) into the buffer at the current position
        // void *memcpy(void dest[restrict .n], const void src[restrict .n], size_t n);
        memcpy(&user_list_buffer[bytes_used], directory_entry->d_name, username_length);
        
        // Advance the bytes_used counter by the username length
        bytes_used += username_length;
        
        // Add a newline character after the username and increment bytes_used
        user_list_buffer[bytes_used++] = '\n';
    }

    closedir(directory);

    // No users found so return an empty string
    if (user_list_buffer == NULL)
    {
        user_list_buffer = malloc(1); // Malloc to return something to the reader (null terminator)
        if (unlikely(user_list_buffer == NULL))
        {
            PSE("Failed to allocate memory for empty user list buffer");
            return NULL;
        }
        user_list_buffer[0] = '\0';
        *output_length = 1;
        return user_list_buffer;
    }

    if (bytes_used + 1 > buffer_capacity)
    {
        PSE("NO ROOM FOR NULL TERMINATOR!");
        
        char *reallocated_list = realloc(user_list_buffer, bytes_used + 1);
        if (reallocated_list == NULL)
        {
            free(user_list_buffer);
            return NULL;
        }
        user_list_buffer = reallocated_list;
        
    }
    else
    {
        user_list_buffer[bytes_used++] = '\0';
        *output_length = bytes_used;
    }
    
    return user_list_buffer;
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

static int sanitize_filename(const char *value)
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
    if (!ends_with(value, file_suffix_user_data))
    {
        return 0;
    }
    return 1;
}


static int recv_cstring(int fd, char *buffer, size_t max_len)
{
    if (buffer == NULL || max_len == 0)
    {
        return -1;
    }
    size_t used = 0;
    while (used + 1 < max_len)
    {
        char ch = '\0';
        ssize_t recvd = recv(fd, &ch, 1, 0);
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
        buffer[used++] = ch;
        if (ch == '\0')
        {
            return 1;
        }
    }
    buffer[max_len - 1] = '\0';
    return -1;
}

/**
 * @brief Function needed for the qsort call in collect_message_files, it compares two strings in descending order putting newest files first since the name of the files are based on timestamps
 * 
 * @param a 
 * @param b 
 * @return int 
 */
static int compare_strings_desc(const void *a, const void *b) // Compare strings descending
{
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcmp(sb, sa);
}

/**
 * @brief Collects all the timestamps of the messages for the requesting user, if only_unread_messages is set to 1, then it collects only the messages that are unread (those that start with "UNREAD"), the function returns an array of strings that are the filenames of the messages, the number of files collected is returned through the output_file_count parameter
 * 
 * @param user_directory_path 
 * @param only_unread_messages 
 * @param output_file_count 
 * @return char** 
 */
static char **collect_message_files(const char *user_directory_path, int only_unread_messages, size_t *output_file_count)
{
    if (unlikely(output_file_count == NULL || user_directory_path == NULL)) // input check
    {
        ierrno = NULL_PARAMETERS;
        PIE("Invalid parameters for collect_message_files");
        return NULL;
    }

    DIR *directory_stream = opendir(user_directory_path);
    if (unlikely(directory_stream == NULL))
    {
        PSE("Failed to open user directory: %s", user_directory_path);
        return NULL;
    }

    size_t collected_file_count = 0;
    size_t files_array_capacity = 0;
    char **collected_message_files = NULL;
    struct dirent *directory_entry = NULL;

    while ((directory_entry = readdir(directory_stream)) != NULL)
    {
        
        if (!ends_with(directory_entry->d_name, file_suffix_user_data))
        { // if it is not a user data file we do not care, this should also filters out the password file
            continue;
        }
        else if (strcmp(directory_entry->d_name, password_filename) == 0 || strcmp(directory_entry->d_name, data_filename) == 0)
        { // Skip password and data files (double check)
            continue;
        }
        if (only_unread_messages && !starts_with(directory_entry->d_name, "UNREAD"))
        { // if we only want unread messages and the file does not start with UNREAD, then we skip it
            continue;
        }

        size_t filename_length = strlen(directory_entry->d_name);
        char *filename_copy = malloc(filename_length + 1);
        if (filename_copy == NULL)
        {
            PSE("Failed to allocate memory for filename copy");
            continue; // not handled as fatal error
        }
        memcpy(filename_copy, directory_entry->d_name, filename_length + 1);

        if (collected_file_count + 1 > files_array_capacity)
        {
            size_t next_array_capacity;
            if (files_array_capacity == 0) next_array_capacity = 5;
            else next_array_capacity = files_array_capacity + 5; // Increase capacity by chunks of 5 pointers

            // Resize the array to hold more pointers
            char **reallocated_files_array = realloc(collected_message_files, next_array_capacity * sizeof(char *));
            
            // If realloc failed, free the filename copy and skip this file
            if (reallocated_files_array == NULL)
            {
                PSE("Failed to allocate memory for collected message files array");
                free(filename_copy);
                continue; // still not fatal
            }
            
            // Update the main pointer to the resized array
            collected_message_files = reallocated_files_array;
            
            // Track the new capacity
            files_array_capacity = next_array_capacity;
        }
        collected_message_files[collected_file_count++] = filename_copy;
    }

    closedir(directory_stream);

    if (collected_message_files == NULL)
    {   // no messages for the user
        *output_file_count = 0;
        return NULL;
    }

    /* qsort, qsort_r - sort an array
    void qsort(void base[.size * .nmemb], size_t nmemb, size_t size, int (*compar)(const void [.size], const void [.size]));
    */
    qsort(collected_message_files, collected_file_count, sizeof(char *), compare_strings_desc);

    *output_file_count = collected_file_count;
    return collected_message_files;
}

/**
 * @brief helper function to free the memory allocated for an array of message file names
 * 
 * @param files 
 * @param count 
 */
static void free_message_files(char **files, size_t count)
{
    if (files == NULL)
    {
        return;
    }
    for (size_t i = 0; i < count; i++)
    {
        free(files[i]);
    }
    free(files);
}

static char *build_list_from_files(char **files, size_t count, size_t *out_len)
{
    if (out_len == NULL)
    {
        return NULL;
    }

    size_t used = 0;
    size_t cap = 0;
    char *list = NULL;

    for (size_t i = 0; i < count; i++)
    {
        size_t name_len = strlen(files[i]);
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
                return NULL;
            }
            list = new_list;
            cap = next_cap;
        }
        memcpy(list + used, files[i], name_len);
        used += name_len;
        list[used++] = '\n';
    }

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

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                          SIGNAL HANDLER (THREAD)                                              */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

void *signal_handler_thread(void *arg)
{
    sigset_t *set = (sigset_t *)arg;
    int sig;

    while (1)
    {
        if (sigwait(set, &sig) != 0)
        {
            PSE("sigwait() failed in signal handler thread");
            continue;
        }

        if (sig == SIGINT || sig == SIGTERM)
        {
            P("Received shutdown signal, shutting down server...");
            shutdown_now = 1;
            break;
        }
    }

    return NULL;
}



/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                          COONNECTION HANDLER (THREAD)                                         */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

void *thread_routine(void *arg)
{
    thread_args_t *thread_args = (thread_args_t *)arg;
    int connection_fd = thread_args->connection_fd;
    int thread_index = thread_args->thread_index;
    free(thread_args); // Free the allocated memory for thread arguments

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

    /* -------------------------------------------------------------------------- */
    /*                            LOGIN / REGISTRATION                            */
    /* -------------------------------------------------------------------------- */

    // Login / registration
    P("[%d]::: Handling login...", connection_fd);

    //  1) Receive username
    ssize_t received = recv(connection_fd, &login_env.sender, sizeof(login_env.sender), 0);
    if (unlikely(received <= 0))
    {
        PSE("::: Error receiving username for connection fd: %d", connection_fd);
        if (shutdown_now) {
            P("Shutdown flag is set, closing thread...");
            goto cleanup;
        }
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
            if (shutdown_now) {
                P("Shutdown flag is set, closing thread...");
                goto cleanup;
            }
            goto cleanup;
        }
        P("[%d]::: Sent START_REGISTRATION to [%s]", connection_fd, login_env.sender);

        // Receive password chosen by the client
        received = recv(connection_fd, client_password, sizeof(client_password), 0);
        if (unlikely(received <= 0))
        {
            PSE("::: Failed to receive registration password for [%s]", login_env.sender);
            if (shutdown_now) {
                P("Shutdown flag is set, closing thread...");
                goto cleanup;
            }
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
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
            }
            goto cleanup;
        }

        response_code = NO_ERROR;
        if (unlikely(send(connection_fd, &response_code, sizeof(response_code), 0) <= 0))
        {
            PSE("::: Failed to confirm registration for [%s]", login_env.sender);
            if (shutdown_now) {
                P("Shutdown flag is set, closing thread...");
                goto cleanup;
            }
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
            if (shutdown_now) {
                P("Shutdown flag is set, closing thread...");
                goto cleanup;
            }
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
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
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
                        if (shutdown_now) {
                            P("Shutdown flag is set, closing thread...");
                            goto cleanup;
                        }
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
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
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
            if (shutdown_now) {
                P("Shutdown flag is set, closing thread...");
                goto cleanup;
            }
            break;
        }
        if (recvd < 0)
        {
            PSE("::: Failed to receive MESSAGE_CODE for [%s]", login_env.sender);
            if (shutdown_now) {
                P("Shutdown flag is set, closing thread...");
                goto cleanup;
            }
            goto cleanup;
        }
        if (recvd != sizeof(request_code))
        {
            P("[%d]::: Incomplete MESSAGE_CODE received (%zd bytes)", connection_fd, recvd);
            if (shutdown_now) {
                P("Shutdown flag is set, closing thread...");
                goto cleanup;
            }
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
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
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
                    if (shutdown_now) {
                        P("Shutdown flag is set, closing thread...");
                        goto cleanup;
                    }
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
                    if (shutdown_now) {
                        P("Shutdown flag is set, closing thread...");
                        goto cleanup;
                    }
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
                    if (shutdown_now) {
                        P("Shutdown flag is set, closing thread...");
                        goto cleanup;
                    }
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
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
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
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
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
            char *list = build_list_of_registered_users(&list_len);
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
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                goto cleanup;
            }

            ERROR_CODE ack = ERROR;
            if (unlikely(recv_all(connection_fd, &ack, sizeof(ack)) <= 0))
            {
                PSE("::: Failed to receive list ack from [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
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
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
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
        {
            P("[%d]::: REQUEST_LOAD_MESSAGE received", connection_fd);
            size_t file_count = 0;
            char **files = collect_message_files(user_dir_path, 0, &file_count);
            char *list = NULL;
            size_t list_len = 0;

            if (file_count == 0)
            {
                list = malloc(1);
                if (list == NULL)
                {
                    PSE("::: Failed to allocate empty list");
                    goto cleanup;
                }
                list[0] = '\0';
                list_len = 1;
            }
            else
            {
                list = build_list_from_files(files, file_count, &list_len);
                if (list == NULL)
                {
                    PSE("::: Failed to build message list");
                    free_message_files(files, file_count);
                    goto cleanup;
                }
            }

            if (list_len > UINT32_MAX)
            {
                PSE("::: Message list too large");
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }

            uint32_t list_len_net = htonl((uint32_t)list_len);
            if (unlikely(send_all(connection_fd, &list_len_net, sizeof(list_len_net)) < 0))
            {
                PSE("::: Failed to send message list length to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }

            ERROR_CODE ack = ERROR;
            if (unlikely(recv_all(connection_fd, &ack, sizeof(ack)) <= 0))
            {
                PSE("::: Failed to receive message list ack from [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }
            if (ack != NO_ERROR)
            {
                P("[%d]::: Client aborted message list", connection_fd);
                free(list);
                free_message_files(files, file_count);
                handled = 1;
                break;
            }

            if (unlikely(send_all(connection_fd, list, list_len) < 0))
            {
                PSE("::: Failed to send message list to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }
            free(list);
            free_message_files(files, file_count);

            MESSAGE_CODE next_code = MESSAGE_ERROR;
            if (unlikely(recv_all(connection_fd, &next_code, sizeof(next_code)) <= 0))
            {
                PSE("::: Failed to receive message selection code from [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                goto cleanup;
            }

            if (next_code == MESSAGE_OPERATION_ABORTED)
            {
                P("[%d]::: Client aborted message load", connection_fd);
                handled = 1;
                break;
            }
            if (next_code != REQUEST_LOAD_SPECIFIC_MESSAGE)
            {
                P("[%d]::: Unexpected code after message list: %d", connection_fd, next_code);
                handled = 1;
                break;
            }

            char filename[512] = {0};
            int filename_recv = recv_cstring(connection_fd, filename, sizeof(filename));
            if (filename_recv <= 0)
            {
                PSE("::: Failed to receive message filename from [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                goto cleanup;
            }

            if (!sanitize_filename(filename))
            {
                MESSAGE_CODE not_found = MESSAGE_NOT_FOUND;
                if (unlikely(send_all(connection_fd, &not_found, sizeof(not_found)) < 0))
                {
                    PSE("::: Failed to send MESSAGE_NOT_FOUND to [%s]", login_env.sender);
                    if (shutdown_now) {
                        P("Shutdown flag is set, closing thread...");
                        goto cleanup;
                    }
                }
                handled = 1;
                break;
            }

            size_t path_len = strlen(user_dir_path) + 1 + strlen(filename) + 1;
            char *full_path = calloc(path_len, sizeof(char));
            if (full_path == NULL)
            {
                PSE("::: Failed to allocate full path for message");
                goto cleanup;
            }
            snprintf(full_path, path_len, "%s/%s", user_dir_path, filename);

            int msg_fd = open(full_path, O_RDONLY);
            if (msg_fd < 0)
            {
                MESSAGE_CODE not_found = MESSAGE_NOT_FOUND;
                if (unlikely(send_all(connection_fd, &not_found, sizeof(not_found)) < 0))
                {
                    PSE("::: Failed to send MESSAGE_NOT_FOUND to [%s]", login_env.sender);
                    if (shutdown_now) {
                        P("Shutdown flag is set, closing thread...");
                        goto cleanup;
                    }
                }
                free(full_path);
                handled = 1;
                break;
            }

            size_t header_size = offsetof(MESSAGE, message);
            MESSAGE *header = calloc(1, header_size);
            if (header == NULL)
            {
                PSE("::: Failed to allocate message header");
                close(msg_fd);
                free(full_path);
                goto cleanup;
            }
            ssize_t header_read = read(msg_fd, header, header_size);
            if (header_read != (ssize_t)header_size)
            {
                PSE("::: Failed to read message header");
                close(msg_fd);
                free(header);
                free(full_path);
                goto cleanup;
            }

            uint32_t body_len = ntohl(header->message_length);
            if (body_len == 0 || body_len > MESSAGE_SIZE_CHARS)
            {
                PSE("::: Invalid message length in file");
                close(msg_fd);
                free(header);
                free(full_path);
                goto cleanup;
            }

            char *body = calloc(body_len, sizeof(char));
            if (body == NULL)
            {
                PSE("::: Failed to allocate message body");
                close(msg_fd);
                free(header);
                free(full_path);
                goto cleanup;
            }
            ssize_t body_read = read(msg_fd, body, body_len);
            if (body_read != (ssize_t)body_len)
            {
                PSE("::: Failed to read message body");
                close(msg_fd);
                free(body);
                free(header);
                free(full_path);
                goto cleanup;
            }
            close(msg_fd);

            ERROR_CODE ok = NO_ERROR;
            if (unlikely(send_all(connection_fd, &ok, sizeof(ok)) < 0))
            {
                PSE("::: Failed to send NO_ERROR to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(body);
                free(header);
                free(full_path);
                goto cleanup;
            }

            if (unlikely(send_all(connection_fd, header, header_size) < 0))
            {
                PSE("::: Failed to send message header to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(body);
                free(header);
                free(full_path);
                goto cleanup;
            }
            if (unlikely(send_all(connection_fd, body, body_len) < 0))
            {
                PSE("::: Failed to send message body to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(body);
                free(header);
                free(full_path);
                goto cleanup;
            }

            if (starts_with(filename, "UNREAD"))
            {
                const char *new_name = filename + strlen("UNREAD");
                if (new_name[0] != '\0')
                {
                    size_t new_path_len = strlen(user_dir_path) + 1 + strlen(new_name) + 1;
                    char *new_path = calloc(new_path_len, sizeof(char));
                    if (new_path != NULL)
                    {
                        snprintf(new_path, new_path_len, "%s/%s", user_dir_path, new_name);
                        rename(full_path, new_path);
                        free(new_path);
                    }
                }
            }

            free(body);
            free(header);
            free(full_path);
            handled = 1;
            break;
        }
        case REQUEST_LOAD_SPECIFIC_MESSAGE:
            P("[%d]::: REQUEST_LOAD_SPECIFIC_MESSAGE received (not implemented)", connection_fd);
            handled = 1;
            break;
        case REQUEST_LOAD_UNREAD_MESSAGES:
        {
            P("[%d]::: REQUEST_LOAD_UNREAD_MESSAGES received", connection_fd);
            size_t file_count = 0;
            char **files = collect_message_files(user_dir_path, 1, &file_count);
            char *list = NULL;
            size_t list_len = 0;

            if (file_count == 0)
            {
                list = malloc(1);
                if (list == NULL)
                {
                    PSE("::: Failed to allocate empty unread list");
                    goto cleanup;
                }
                list[0] = '\0';
                list_len = 1;
            }
            else
            {
                list = build_list_from_files(files, file_count, &list_len);
                if (list == NULL)
                {
                    PSE("::: Failed to build unread message list");
                    free_message_files(files, file_count);
                    goto cleanup;
                }
            }

            if (list_len > UINT32_MAX)
            {
                PSE("::: Unread message list too large");
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }

            uint32_t list_len_net = htonl((uint32_t)list_len);
            if (unlikely(send_all(connection_fd, &list_len_net, sizeof(list_len_net)) < 0))
            {
                PSE("::: Failed to send unread list length to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }

            ERROR_CODE ack = ERROR;
            if (unlikely(recv_all(connection_fd, &ack, sizeof(ack)) <= 0))
            {
                PSE("::: Failed to receive unread list ack from [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }
            if (ack != NO_ERROR)
            {
                P("[%d]::: Client aborted unread list", connection_fd);
                free(list);
                free_message_files(files, file_count);
                handled = 1;
                break;
            }

            if (unlikely(send_all(connection_fd, list, list_len) < 0))
            {
                PSE("::: Failed to send unread list to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }

            free(list);
            free_message_files(files, file_count);
            handled = 1;
            break;
        }
        case REQUEST_DELETE_MESSAGE:
        {
            P("[%d]::: REQUEST_DELETE_MESSAGE received", connection_fd);
            size_t file_count = 0;
            char **files = collect_message_files(user_dir_path, 0, &file_count);
            char *list = NULL;
            size_t list_len = 0;

            if (file_count == 0)
            {
                list = malloc(1);
                if (list == NULL)
                {
                    PSE("::: Failed to allocate empty delete list");
                    goto cleanup;
                }
                list[0] = '\0';
                list_len = 1;
            }
            else
            {
                list = build_list_from_files(files, file_count, &list_len);
                if (list == NULL)
                {
                    PSE("::: Failed to build delete message list");
                    free_message_files(files, file_count);
                    goto cleanup;
                }
            }

            if (list_len > UINT32_MAX)
            {
                PSE("::: Delete message list too large");
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }

            uint32_t list_len_net = htonl((uint32_t)list_len);
            if (unlikely(send_all(connection_fd, &list_len_net, sizeof(list_len_net)) < 0))
            {
                PSE("::: Failed to send delete list length to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }

            ERROR_CODE ack = ERROR;
            if (unlikely(recv_all(connection_fd, &ack, sizeof(ack)) <= 0))
            {
                PSE("::: Failed to receive delete list ack from [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }
            if (ack != NO_ERROR)
            {
                P("[%d]::: Client aborted delete list", connection_fd);
                free(list);
                free_message_files(files, file_count);
                handled = 1;
                break;
            }

            if (unlikely(send_all(connection_fd, list, list_len) < 0))
            {
                PSE("::: Failed to send delete list to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(list);
                free_message_files(files, file_count);
                goto cleanup;
            }
            free(list);
            free_message_files(files, file_count);

            MESSAGE_CODE next_code = MESSAGE_ERROR;
            if (unlikely(recv_all(connection_fd, &next_code, sizeof(next_code)) <= 0))
            {
                PSE("::: Failed to receive delete selection code from [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                goto cleanup;
            }

            if (next_code == MESSAGE_OPERATION_ABORTED)
            {
                P("[%d]::: Client aborted delete operation", connection_fd);
                handled = 1;
                break;
            }
            if (next_code != REQUEST_LOAD_SPECIFIC_MESSAGE)
            {
                P("[%d]::: Unexpected code after delete list: %d", connection_fd, next_code);
                handled = 1;
                break;
            }

            char filename[512] = {0};
            int filename_recv = recv_cstring(connection_fd, filename, sizeof(filename));
            if (filename_recv <= 0)
            {
                PSE("::: Failed to receive delete filename from [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                goto cleanup;
            }

            if (!sanitize_filename(filename))
            {
                MESSAGE_CODE not_found = MESSAGE_NOT_FOUND;
                if (unlikely(send_all(connection_fd, &not_found, sizeof(not_found)) < 0))
                {
                    PSE("::: Failed to send MESSAGE_NOT_FOUND to [%s]", login_env.sender);
                    if (shutdown_now) {
                        P("Shutdown flag is set, closing thread...");
                        goto cleanup;
                    }
                }
                handled = 1;
                break;
            }

            size_t path_len = strlen(user_dir_path) + 1 + strlen(filename) + 1;
            char *full_path = calloc(path_len, sizeof(char));
            if (full_path == NULL)
            {
                PSE("::: Failed to allocate delete path");
                goto cleanup;
            }
            snprintf(full_path, path_len, "%s/%s", user_dir_path, filename);

            int delete_response = NO_ERROR;
            if (unlink(full_path) != 0)
            {
                delete_response = MESSAGE_NOT_FOUND;
            }
            if (unlikely(send_all(connection_fd, &delete_response, sizeof(delete_response)) < 0))
            {
                PSE("::: Failed to send delete response to [%s]", login_env.sender);
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
                free(full_path);
                goto cleanup;
            }

            free(full_path);
            handled = 1;
            break;
        }
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
                if (shutdown_now) {
                    P("Shutdown flag is set, closing thread...");
                    goto cleanup;
                }
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
    exit_thread_and_update_thread_id_array(thread_index);
    pthread_exit(NULL); // Should not be reached
    return NULL; // Should not be reached
}


/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                                   MAIN                                                   */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
int main(int argc, char** argv)
{
    printf("Welcome to PGM server!\n");
    PIE("Testing PIE macro");
    PSE("Testing PSE macro");
    printf("Printing ascii art and name\n");
    fprintf(stdout, "%s", ascii_art);
    fprintf(stdout, "Program name: %s\n", program_name);

    /* -------------------------------------------------------------------------- */
    /*                             SEMAPHORE HANDLING                             */
    /* -------------------------------------------------------------------------- */

    if (unlikely(sem_init(&current_loggedin_users_semaphore, 0, 1) == -1)) // LUX MAN: int sem_init(sem_t *sem, int pshared, unsigned int value) initializes the unnamed semaphore at the address pointed to by sem.  The value argument specifies the initial value for the semaphore.
    {
        PSE("sem_init() failed for current_loggedin_users_semaphore");
        E();
    }
    if (unlikely(sem_init(&shutdown_arrays_semaphore, 0, 1) == -1))
    {
        PSE("sem_init() failed for shutdown_arrays_semaphore");
        E();
    }

    /* -------------------------------------------------------------------------- */
    /*                           MAIN ARGUMENT HANDLING                           */
    /* -------------------------------------------------------------------------- */
    int32_t port_number = DEFAULT_PORT_NUMBER;
    const char *env_port = getenv(server_port_env);
    if (env_port != NULL)
    {
        P("Port from environment: %s", env_port);
        port_number = parse_port_string(env_port, port_number);
    }
    if (argc >= 2)
    {
        P("Port from argument: %s", argv[1]);
        port_number = parse_port_string(argv[1], port_number);
    }

    /* -------------------------------------------------------------------------- */
    /*                               SOCKET HANDLING                              */
    /* -------------------------------------------------------------------------- */

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

    /* -------------------------------------------------------------------------- */
    /*                               SIGNAL HANDLING                              */
    /* -------------------------------------------------------------------------- */

    /* No forks were made so we do not worry about childs
    // Prevent my childs from becoming zombies...
    // Source - https://stackoverflow.com/a/17015831
    struct sigaction sigchld_action = {
        .sa_handler = SIG_DFL, // Default signal handler for SIGCHLD
        .sa_flags = SA_NOCLDWAIT // If  signum  is SIGCHLD, do not transform children into zombies when they terminate.
    };
    sigaction(SIGCHLD, &sigchld_action, NULL);
    */
    // Block signals and make them be handled by the signal thread
    pthread_t signal_thread_id;
    sigset_t set; // signal mask
    // Block both signals
    sigaddset(&set, SIGINT); // MAN sigaddset() and sigdelset() add and delete respectively signal signum from set. -NOT-> sigfillset() initializes set to full, including all signals.
    sigaddset(&set, SIGTERM);
    if (unlikely(pthread_sigmask(SIG_BLOCK, &set, NULL) != 0)) // LINUX MAN: A new thread inherits a copy of its creator's signal mask.
    {
        PSE("Failed to block signals in main thread");
        E();
    }
    if (unlikely(pthread_create(&signal_thread_id, NULL, signal_handler_thread, (void *)&set) != 0))
    {
        PSE("Failed to create signal handler thread");
        E();
    }

    /* -------------------------------------------------------------------------- */
    /*                                  MAIN LOOP                                 */
    /* -------------------------------------------------------------------------- */

    // Now we accept connections in loop, each connection will be handled by a different thread
    size_t thread_args_connections_index = 0;// The index of the array to keep track where to write
    // Why does this not overwrite other thread ids? because we set the size of the thread id array to MAX_BACKLOG, which is the maximum number of connections that can be waiting at the same time, so we are sure that by the time we overwrite a thread id, the corresponding thread will have finished and its id will not be useful anymore (since we are not joining the threads, we do not care about their ids after they finish)
    // From [https://blog.clusterweb.com.br/?p=4854] "To summarize, if the TCP implementation in Linux receives the ACK packet of the 3-way handshake and the accept queue is full, it will basically ignore that packet."
    // MOVED TO START OF THE DOCUMENT FOR GLOBAL ACCESS   // The thread id array in which we store the thread ids
                                              // @note: we are not joining the threads anywhere, so this is not exactly useful, but may be in future implementations or for debugging
                                              // In the end it was useful indeed
    while (1)
    {
        if (shutdown_now)
        {
            P("Shutdown variable set, stopping main thread server...");
            break;
        }
        P("Waiting for incoming connections...");
        // We necessarily need to create this variables on stack since the accept function uses pointers to them
        int new_connection;
        struct sockaddr_in client_address;
        socklen_t client_addrlen = sizeof(client_address);
        if (unlikely((new_connection = accept(skt_fd, (struct sockaddr *)&client_address, &client_addrlen)) < 0))
        {
            PSE("Socket accept failed");
            if (shutdown_now)
            {
                P("Shutdown variable set, stopping main thread server...");
                break;
            }
            continue; // We do not exit the program, we just continue to accept new connections
        }
        P("Connection accepted from IP: %s, Port: %d, New socket file descriptor: %d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port), new_connection);
        
        // Close connection if the max number of logged in users is reached
        if (number_of_current_logged_in_users >= MAX_BACKLOG)
        {
            P("Maximum number of logged in users reached, rejecting connection from IP: %s, Port: %d", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));
            close(new_connection);
            continue;
        }

        // CHECK IF SLOT IS FREE: ---------------------------------------
        while (thread_id_array[thread_args_connections_index] != 0)
        {
            PSE("Thread id array index %zu is not 0, a thread is already running on this index", thread_args_connections_index);
            thread_args_connections_index = ((thread_args_connections_index + 1) % MAX_BACKLOG);
            if (shutdown_now)
            {
                P("Shutdown variable set, stopping main thread server...");
                break;
            }
            sched_yield(); // Yield the CPU to let threads run and finish
            /*
            close(new_connection);
            free(thread_args);
            continue;
            */
        }

        // CREATE NEW THREAD HANDLING THE CONNECTION: ---------------------------------------
        thread_args_t *thread_args = malloc(sizeof(thread_args_t));
        if (thread_args == NULL)
        {
            PSE("Failed to allocate memory for thread arguments");
            close(new_connection);
            continue;
        }
        thread_args->connection_fd = new_connection;
        thread_args->thread_index = thread_args_connections_index;
        connections_array[thread_args_connections_index] = new_connection;
        
        if (shutdown_now)
        {
            P("Shutdown variable set, stopping main thread server...");
            break;
        }
        if (unlikely(pthread_create(&thread_id_array[thread_args_connections_index], NULL, thread_routine, (void *)thread_args) != 0))
        {
            PSE("Failed to create thread for connection fd: %d", new_connection);
            // We close the connection since we cannot handle it
            close(new_connection);
            connections_array[thread_args_connections_index] = 0; // Clear the connection fd from the array
            free(thread_args);
        }
        else
        {
            // Successfully created thread, increment the index in a circular manner
            P("Thread [%lu] created successfully for connection fd: %d\n", (unsigned long)thread_id_array[thread_args_connections_index], new_connection);
            // Add new connection to the connections array for later closing when shutting down the server
            connections_array[thread_args_connections_index] = new_connection;
            // Increment index
            thread_args_connections_index = ((thread_args_connections_index + 1) % MAX_BACKLOG); // Conversion to silence gcc -> actually gcc was right
            // Add another user to the count of logged in users, we do this after creating the thread to be sure that the thread will actually handle the connection and increment the count of logged in users, otherwise we may end up with a situation where we have a thread that is not handling any connection but is still counted as a logged in user, which would prevent new connections from being accepted
            number_of_current_logged_in_users++;

        }
        // close(new_connection); It's the thread's resposibility to close the socket when done
    }

    /* -------------------------------------------------------------------------- */
    /*                               CLOSING SERVER:                              */
    /* -------------------------------------------------------------------------- */

    // I need to close the listening socket to unblock the accept() call in case it is blocking, so that the server can shutdown gracefully
    P("\t------------------------------------------------------------------");
    P("\tClosing server NOW!");
    P("\t------------------------------------------------------------------");
    
    // Copy the thread id array and the connections so that we still know wich threads were active when we started shutting down, since the original arrays will be modified by the threads when they finish and set their id to 0 and their connection to 0
    pthread_t thread_id_array_copy[MAX_BACKLOG] = {0};
    int connections_array_copy[MAX_BACKLOG] = {0};

    // AQUIRING SEMAPHORE
    unsigned int max_retries = 5;
    while(unlikely(sem_wait(&shutdown_arrays_semaphore) < 0)) // All of these functions return 0 on success
    {
        if (unlikely(!max_retries--))
        {
            P("sem_wait() failed to aquire semaphore after max_retries retries");
            pthread_exit(NULL);
        }
        PSE("sem_wait() failed to aquire semaphore");
        sched_yield(); // LINUX MAN: sched_yield() causes the calling thread to relinquish the CPU.  The thread is moved to the end of the queue for its static priority and a new thread gets to run.
        continue; // Try again
    }

    // Copy the thread id array and the connections so that we still know wich threads were active when we started shutting down, since the original arrays will be modified by the threads when they finish and set their id to 0 and their connection to 0
    memcpy(thread_id_array_copy, thread_id_array, sizeof(thread_id_array));
    memcpy(connections_array_copy, connections_array, sizeof(connections_array));

    while(unlikely(sem_post(&shutdown_arrays_semaphore) < 0))
    {
        PSE("sem_post() failed to release semaphore");
        if (unlikely(!max_retries--))
        {
            P("sem_post() failed to release semaphore after max_retries retries");
            pthread_exit(NULL);
        }
        sched_yield(); // LINUX MAN: sched_yield() causes the calling thread to relinquish the CPU.  The thread is moved to the end of the queue for its static priority and a new thread gets to run.
        continue; // Try again
    }

    for (unsigned int i = 0; i < MAX_BACKLOG; i++) 
    {
        if (connections_array_copy[i] > 0)  
        {
            if (unlikely(shutdown(connections_array_copy[i], SHUT_RDWR) < 0))
            {
                PSE("Failed to shutdown connection fd: %d", connections_array_copy[i]);
                thread_id_array_copy[i] = 0; // We set the thread id to 0 to not try to join it later, since it may be blocked on the socket and we won't be able to join it, but we do not want to leave it hanging there, so we just ignore it and let it finish on its own when it detects that the socket is closed
            }

        }
    }  

    // Wait for other threads to finish before shutting down the server, so that unsaved work gets saved
    for(unsigned int i = 0; i < MAX_BACKLOG; i++)
    {
        if (thread_id_array_copy[i] != 0)
        {
            P("Joining thread [%lu]", (unsigned long)thread_id_array_copy[i]); // Just to print
            pthread_join(thread_id_array_copy[i], NULL); // MAN: If retval is not NULL, then pthread_join() copies the exit status of the target thread (i.e., the value that the target thread supplied to  pthread_exit(3))  into  the location pointed to by retval.
        }
    }
    
    printf("Exiting program!\n");
    return 0;
}
