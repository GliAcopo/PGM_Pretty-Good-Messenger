#include "settings.h"
#include <sys/stat.h> /* For mkdir */
#include <error.h>    /* For error handling and printing (strerror)*/
#include <errno.h>    /* For errno */
#include <stdio.h>
#include <stdlib.h> // to get the home environment name
#include <string.h> // String concatenation
#include <unistd.h> // for read

inline ERROR_CODE create_home_folder(void)
{
    const char *HOME_dir = NULL;
    HOME_dir = getenv("HOME"); /* The  getenv() function searches the environment list to find the environment variable name, and returns a pointer to the corresponding value string.*/
    /** @note: BE CAREFUL NOT TO MODIFY THE ENV VARIABLE -> this will result in modification of the TRUE anv variable for the program! */
    if (HOME_dir == NULL)
    {
        DEBUG_PRINT("ERROR: Couldn't get home environment from env variables. The folder creation is not possible \n");
        return (SYSCALL_ERROR);
        /** @todo handle the error in the main program, if the folder already exist you may decide to not override it and go on, else you may want to close the program */
    }

    char full_path_name[4096]; /** @note The maximum number of characters that can be included in any path name is 4096 on Unix-based platforms */
    snprintf(full_path_name, sizeof(full_path_name), "%s/%s", HOME_dir, program_name);

    /* Prompt user if he wants to continue or maybe change the home folder */
    printf("Warning! the %s program is going to create a directory in the HOME [%s] user directory \n Full path: [%s]\n \
        press y to continue \n\
        press p to redefine the path to another directory\n\
        press anything else to exit the program without nothing being done\n",
           program_name, HOME_dir, full_path_name);
    /* Read the response from the user */
    switch (getc(STDIN_FILENO))
    {
    case 'y':
        DEBUG_PRINT("button y pressed");
        break;
    case 'p':
        DEBUG_PRINT("button p pressed");
    retry: // handling of when the first path name inputted is wrong and we need the user to input a new one
        /* yapping */
        printf("Input into terminal a new path name that will substitute the previous:\n\
            %s\n\
            NOTE: only the first %lu characters will be read\n\
            Insert new path: ",
               full_path_name, sizeof(full_path_name));
        while (getchar() != '\n' && getchar() != EOF)
        {
        }; // flush stdin in any case
        read(STDIN_FILENO, full_path_name, sizeof(full_path_name)); // read path and put it into the variable
        break;                                                      // Other checks are done above
    default:
        return (OPERATION_ABORTED);
        /** @todo handle this in the main() */
    };

    if (mkdir(full_path_name, 0700) == -1) /** @note: I've chosen 0700 because it is what the .ssh folder is using */
    {
        DEBUG_PRINT("WARNING: mkdir returned an error: [%s]", strerror(errno));
        if (errno == EEXIST)
        {
            printf("Directory already exists: %s\n", full_path_name);
            goto success;
        }
        else if (errno == EACCES)
        {
            printf("Permission denied: cannot create directory %s\n", full_path_name);
        }
        else if (errno == ENOENT)
        {
            printf("A component of the path does not exist: %s\n", full_path_name);
        }
        else
        {
            printf("Unknown error (%d): %s\n", errno, strerror(errno));
        }

        /* Ask if the user wants to retry inputting the path name */
        printf("Would you like to retry inputting the path name? [Y/n]");
        while (getchar() != '\n' && getchar() != EOF)
        {
        }; // flush stdin in any case
        // if the choice is no then we close the program
        if (getc(STDIN_FILENO) == 'n')
        {
            printf("No folder created, exiting program");
            return (EXIT_PROGRAM); // A return value that asks whoever called the program to explicitly close it, we don't close it here because there may be some unsaved work or other close routines to handle
        }
        else
        {
            goto retry;
        }
    }
    printf("Directory created successfully: %s\n", full_path_name);
    
    success:
    return (NO_ERROR);
}
/** mkdir modes:
 * | Mode   | Octal     | Meaning                                     |
| ------ | --------- | ------------------------------------------- |
| `0700` | rwx------ | Only owner can read, write, execute         |
| `0750` | rwxr-x--- | Owner: all, Group: read+execute             |
| `0755` | rwxr-xr-x | Owner: all, Group/Others: read+execute      |
| `0770` | rwxrwx--- | Owner + Group: all, Others: none            |
| `0777` | rwxrwxrwx | Everyone can read, write, execute (!!!)      |
| `0644` | rw-r--r-- | Usually for files: Owner write, others read |
| `0555` | r-xr-xr-x | Read/execute for everyone (no write)        |

 */
