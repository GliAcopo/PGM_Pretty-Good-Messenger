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
    case EXIT_PROGRAM:
        return "EXIT_PROGRAM";
    default:
        return "UNKNOWN_ERROR_CODE";
    }
}

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