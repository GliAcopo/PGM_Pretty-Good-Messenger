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
ERROR_CODE ierrno = NO_ERROR;

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
    default:
        return "UNKNOWN_ERROR_CODE";
    }
}

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              MESSAGE STRUCT CREATION                                          */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

inline ERROR_CODE verify_message_integrity(MESSAGE* message)
{

}

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
		char confirm = 'Y';
		if (unlikely(read(STDIN_FILENO, &confirm, 1) < 0))
		{
			PSE("");
			return(SYSCALL_ERROR);
		}

	} while(confirm == 'n' || confirm == 'N'); // Continue the loop until they do not input 'n'


	// ENCRYPT MESSAGE?
	
	return(NO_ERROR);
}
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              HOME FOLDER CREATION                                             */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

/**
 * @brief Create a home folder object
 * This function tries to create a folder in the home directory of the user home/<program name>. It will ask the user if they are ok with the creation.
 * The user can also redefine the path the home folder will be created in, if the path is not to the user liking, they can change it.
 *
 * @return ERROR_CODE
 * @retval NO_ERROR           Directory created successfully
 * @retval SYSCALL_ERROR      System call failed (getenv, snprintf, mkdir)
 * @retval OPERATION_ABORTED  User aborted the operation
 * @retval EXIT_PROGRAM       User chose to exit the program
 */
inline ERROR_CODE create_home_folder(void)
{
    printf("The program needs to create a folder where to keep all its files\n");
    const char *HOME_dir;
    HOME_dir = getenv("HOME"); /* The  getenv() function searches the environment list to find the environment variable name, and returns a pointer to the corresponding value string.*/
    /** @note: BE CAREFUL NOT TO MODIFY THE ENV VARIABLE -> this will result in modification of the TRUE anv variable for the program! */
    if (HOME_dir == NULL)
    {
        pe("ERROR: Couldn't get home environment from env variables. The folder creation is not possible \n");
        return (SYSCALL_ERROR);
        /** @todo handle the error in the main program, if the folder already exist you may decide to not override it and go on, else you may want to close the program */
    }

    char full_path_name[4096]; /** @note The maximum number of characters that can be included in any path name is 4096 on Unix-based platforms */
    if (snprintf(full_path_name, sizeof(full_path_name), "%s/%s", HOME_dir, program_name) != (strlen(HOME_dir)) + (strlen(program_name)))
    {
        pe("ERROR: snprintf stored less bytes than expected, suspected truncated output: [%s]", full_path_name);
        return (SYSCALL_ERROR);
    }

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
// #endregion

ERROR_CODE

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                           PROGRAM NAME AND ASCII ART                                          */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
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
    "A super simple terminal messenger app written in C that uses PGP (Pretty Good Privacy) encryption\n";

/**
          _____                    _____                    _____
         /\    \                  /\    \                  /\    \
        /::\    \                /::\    \                /::\____\
       /::::\    \              /::::\    \              /::::|   |
      /::::::\    \            /::::::\    \            /:::::|   |
     /:::/\:::\    \          /:::/\:::\    \          /::::::|   |
    /:::/__\:::\    \        /:::/  \:::\    \        /:::/|::|   |
   /::::\   \:::\    \      /:::/    \:::\    \      /:::/ |::|   |
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
