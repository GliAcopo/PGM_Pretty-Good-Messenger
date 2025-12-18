/**
 * @file 3-Global-Variables-and-Functions.c
 * @author Jacopo Rizzuto (jacoporizzuto04@gmail.com)
 * @brief .c File in wich variables, constants, parameters and functions are used both by client code and Server code.
 * @version 0.1
 * @date 2025-07-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "3-Global-Variables-and-Functions.h" 
#include <unistd.h>

// ierror, an internal debug substitute to errno
//ERROR_CODE ierrno = NO_ERROR;

/**
 * @brief Converts an ERROR_CODE enum value to its corresponding string representation.
 *
 * This function takes an ERROR_CODE value and returns a constant string describing the error.
 *
 * @param code The ERROR_CODE value to convert.
 * @return A constant string representing the error code. If the code is not recognized, "UNKNOWN_ERROR_CODE" is returned.
 * 
 * @note: since the 
 */
const char *convert_error_code_to_string(const ERROR_CODE code)
{
    switch (code)
    {
    case NO_ERROR:
        return "NO_ERROR";
    case ERROR:
        return "ERROR";
    case STRING_SIZE_INVALID:
        return "STRING_SIZE_INVALID";
    case STRING_SIZE_EXCEEDING_MAXIMUM:
        return "STRING_SIZE_EXCEEDING_MAXIMUM";
    case TTY_ERROR:
        return "TTY_ERROR";
    case SYSCALL_ERROR:
        return "SYSCALL_ERROR";
    case OPERATION_ABORTED:
        return "OPERATION_ABORTED";
    case NULL_PARAMETERS:
	return "NULL_PARAMETERS";
    case EXIT_PROGRAM:
        return "EXIT_PROGRAM";
        case START_REGISTRATION:
        return "START_REGISTRATION";
    case WRONG_PASSWORD:
        return "WRONG_PASSWORD";
    default:
        return "UNKNOWN_ERROR_CODE";
    }
}

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
/*
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
	do
	{
		printf("Now write the message, press ENTER to confirm, \
			the message can be %u ascii chars long, the remaining chars will be truncated:\n>", MESSAGE_SIZE_CHARS - 1);
		ssize_t n = read(STDIN_FILENO, message->message, (MESSAGE_SIZE_CHARS - 1));
		if(unlikely(n < 0))
		{
    }

    // /* Prompt user if he wants to continue or maybe change the home folder
    printf("Warning! the %s program is going to create a directory in the HOME [%s] user directory \n Full path: [%s]\n \
        press y to continue \n\
        press p to redefine the path to another directory\n\
        press anything else to exit the program without nothing being done\n",
           program_name, HOME_dir, full_path_name);
    // Read the response from the user 
    switch (getc(STDIN_FILENO))
    {
    case 'y':
        DEBUG_PRINT("button y pressed");
        break;
        DEBUG_PRINT("button p pressed");
    retry: // handling of when the first path name inputted is wrong and we need the user to input a new one
        ///* yapping 
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
        ///** @todo handle this in the main() 
    };

    if (mkdir(full_path_name, 0700) == -1) //** @note: I've chosen 0700 because it is what the .ssh folder is using 
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

        ///* Ask if the user wants to retry inputting the path name 
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

        DEBUG_PRINT("button p pressed");
    retry: // handling of when the first path name inputted is wrong and we need the user to input a new one
        //* yapping 
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
        //** @todo handle this in the main() 
    };

    if (mkdir(full_path_name, 0700) == -1) //** @note: I've chosen 0700 because it is what the .ssh folder is using 
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

        //* Ask if the user wants to retry inputting the path name 
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
*/
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

const char *file_suffix_user_data = ".pgm"; // Suffix for message files
const char *password_filename = ".PASSWORD"; // File to store passwords
const char *data_filename = ".DATA";         // File to store user data like number of messages sent, received, etc.
const char *program_name = "PGM";
const char *ascii_art =
    "          _____                    _____                    _____          \n"
    "         /\\    \\                  /\\    \\                  /\\    \\         \n"
    "        /::\\    \\                /::\\    \\                /::\\____\\        \n"
    "       /::::\\    \\              /::::\\    \\              /::::|   |        \n"
    "      /::::::\\    \\            /::::::\\    \\            /:::::|   |        \n"
    "     /:::/\\:::\\    \\          /:::/\\:::\\    \\          /::::::|   |        \n"
    "    /:::/__\\:::\\    \\        /:::/  \\:::\\    \\        /:::/|::|   |        \n"
    "   /::::\\   \\:::\\    \\      /:::/    \\:::\\    \\      /:::/ |::|   |        \n"
    "  /::::::\\   \\:::\\    \\    /:::/    / \\:::\\    \\    /:::/  |::|___|______  \n"
    " /:::/\\:::\\   \\:::\\____\\  /:::/    /   \\:::\\ ___\\  /:::/   |::::::::\\    \\ \n"
    "/:::/  \\:::\\   \\:::|    |/:::/____/  ___\\:::|    |/:::/    |:::::::::\\____\\\n"
    "\\::/    \\:::\\  /:::|____|\\:::\\    \\ /\\  /:::|____|\\::/    / ~~~~~/:::/    /\n"
    " \\/_____/\\:::\\/:::/    /  \\:::\\    /::\\ \\::/    /  \\/____/      /:::/    / \n"
    "          \\::::::/    /    \\:::\\   \\:::\\ \\/____/               /:::/    /  \n"
    "           \\::::/    /      \\:::\\   \\:::\\____\\                /:::/    /   \n"
    "            \\::/____/        \\:::\\  /:::/    /               /:::/    /    \n"
    "             ~~               \\:::\\/:::/    /               /:::/    /     \n"
    "                               \\::::::/    /               /:::/    /      \n"
    "                                \\::::/    /               /:::/    /       \n"
    "                                 \\::/____/                \\::/    /        \n"
    "                                                           \\/____/         \n"
    "\n"
    "  _____          _   _            _____                 _   __  __                                          \n"
    " |  __ \\        | | | |          / ____|               | | |  \\/  |                                         \n"
    " | |__) | __ ___| |_| |_ _   _  | |  __  ___   ___   __| | | \\  / | ___  ___ ___  ___ _ __   __ _  ___ _ __ \n"
    " |  ___/ '__/ _ \\ __| __| | | | | | |_ |/ _ \\ / _ \\ / _` | | |\\/| |/ _ \\/ __/ __|/ _ \\ '_ \\ / _` |/ _ \\ '__|\n"
    " | |   | | |  __/ |_| |_| |_| | | |__| | (_) | (_) | (_| | | |  | |  __/\\__ \\__ \\  __/ | | | (_| |  __/ |   \n"
    " |_|   |_|  \\___|\\__|\\__|\\__, |  \\_____|\\___/ \\___/ \\__,_| |_|  |_|\\___||___/___/\\___|_| |_|\\__, |\\___|_|   \n"
    "                          __/ |                                                              __/ |          \n"
    "                         |___/                                                              |___/           \n"
    "A super simple terminal messenger app written in C \n";

// that uses PGP (Pretty Good Privacy) encryption

/**
          _____                    _____                    _____
         /\    \                  /\    \                  /\    \
        /::\    \                /::\    \                /::\____\
       /::::\    \              /::::\    \              /::::|   |
      /::::::\    \            /::::::\    \            /:::::|   |
  /::::::\   \:::\    \    /:::/    / \:::\    \    /:::/  |::|___|______
 /:::/\:::\   \:::\____\  /:::/    /   \:::\ ___\  /:::/   |::::::::\    \
/:::/  \:::\   \:::|    |/:::/____/  ___\:::|    |/:::/    |:::::::::\____\
\::/    \:::\  /:::|____|\:::\    \ /\  /:::|____|\::/    / ~~~~~/:::/    /
 \/_____/\:::\/:::/    /  \:::\    /::\ \::/    /  \/____/      /:::/    /
          \::::::/    /    \:::\   \:::\ \/____/               /:::/    /
           \::::/    /      \:::\   \:::\____\                /:::/    /
            \::/____/        \:::\  /:::/    /               /:::/    /
             ~~               \:::\/:::/    /               /:::/    /
                               \::::::/    /               /:::/    /
                                \::::/    /               /:::/    /
                                 \::/____/                \::/    /
                                                           \/____/

  _____          _   _            _____                 _   __  __
 |  __ \        | | | |          / ____|               | | |  \/  |
 | |__) | __ ___| |_| |_ _   _  | |  __  ___   ___   __| | | \  / | ___  ___ ___  ___ _ __   __ _  ___ _ __
 |  ___/ '__/ _ \ __| __| | | | | | |_ |/ _ \ / _ \ / _` | | |\/| |/ _ \/ __/ __|/ _ \ '_ \ / _` |/ _ \ '__|
 | |   | | |  __/ |_| |_| |_| | | |__| | (_) | (_) | (_| | | |  | |  __/\__ \__ \  __/ | | | (_| |  __/ |
 |_|   |_|  \___|\__|\__|\__, |  \_____|\___/ \___/ \__,_| |_|  |_|\___||___/___/\___|_| |_|\__, |\___|_|
                          __/ |                                                              __/ |
                         |___/                                                              |___/
A super simple terminal messenger app written in C that uses PGP (Pretty Good Privacy) encryption
*/
