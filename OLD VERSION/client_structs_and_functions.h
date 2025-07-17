/**
 * @file client_functions.h
 * @author Jacopo Rizzuto (Jacoporizzuto04@gmail.com)
 * @brief File that contains function definitions used specifically by the client
 * @version 0.1
 * @date 2025-05-26
 * 
 * @copyright Copyright (c) 2025
 * 
 */

#include "settings.h"
#include "struct_definitions_and_methods.h"
#include <stddef.h>
#include <cstring>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */
/*                               AUTHENTICATION                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief The authentication function logins a given user into the relative account by communicating with the server.
 * The function takes the "username" and "password" strings, encrypts them and sends them to the server. 
 * if the server accepts the authentication then the relative authentication data is kept in the auth_info. 
 * 
 * @param authentication_pid 
 */
static inline ERROR_CODE authentication(char* username, char* password)
{
    
    printf("Username: ");
    char* username;
    fscanf(STDIN_FILENO, "%ms\n", &username);

    /* ---- We disable terminal echoing so password insertion is more secure ---- */

    struct termios oldt, newt;

    // Get current terminal attributes
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // Disable echo
    newt.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    /* We read direclty from /dev/tty (terminal) so our program cannot be fooled by stdin redirection for password insertion */
    FILE *tty = fopen("/dev/tty", "r+"); /* Opening tty */
    if (tty == NULL)
    {
        DEBUG_PRINT("Error opening /dev/tty");
        fclose(tty);
        return(TTY_ERROR);
    }

    /** if (fwrite("Password:", sizeof(char), strlen("Password:"), tty) < strlen("Password:")) */
    const char *msg = "Password:";
    size_t len = strlen(msg);
    if (fwrite(msg, 1, len, tty) != len) /* We write into tty */
    {
        DEBUG_PRINT("Error printing to /dev/tty");
        fclose(tty);
        return(TTY_ERROR);
    }
    
    char password[128];
    fgets(password, sizeof(password), tty);
    
    /* -------------------------- authentication logic -------------------------- */

    /* we are missing the logic to authenticate and repeat the request in case the password or the username are wrong */
    
    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("\n");
    fclose(tty);
    return(NO_ERROR);
};

/* -------------------------------------------------------------------------- */
/*                                 CLIENT LOOP                                */
/* -------------------------------------------------------------------------- */

static inline void function_selection(){};
static inline void message(){};

static inline ERROR_CODE main_client_loop(){
    size_t authentication_pid;
    authentication();
    function_selection();
}