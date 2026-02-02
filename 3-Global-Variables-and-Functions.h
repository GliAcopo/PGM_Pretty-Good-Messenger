/**
 * @file 3-Global-Variables-and-Functions.h
 * @author Jacopo Rizzuto (jacoporizzuto04@gmail.com)
 * @brief .h File in wich variables, constants, parameters and functions are used both by client code and Server code.
 * @version 0.1
 * @date 2025-07-17
 *
 * @copyright Copyright (c) 2025
 *
 */

#pragma once /* only stops a header from being included more than once within the same translation unit, it does nothing to prevent each .c file that does #include "settings.h" from getting its own copy of everything in that header */

#include <stdio.h>  /* For stderr */
#include <errno.h>  /* For errno */
#include <string.h> /* For strerror */
#include <stdint.h> /* For uint32_t */

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                           GLOBAL MACROS DEFINITIONS                                           */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

/* --- DEBUG --- */
#define DEBUG 1

#ifdef DEBUG
// PRINT DEBUG MESSAGE WITH FILE:FUNCTION:LINE METADATA WITHOUT INTERROGATING PERROR
#define PD(fmt, ...) do{fprintf(stderr, "[%s:%s:%d] " fmt "\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__);}while(0);
// PRINT SYSCALL/SYSTEM ERROR WITH FILE:FUNCTION:LINE AND INTERROGATING PERROR
#define PSE(fmt, ...) do{fprintf(stderr, "[%s:%s:%d] " fmt " (Strerror output: [%s])\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__, strerror(errno));}while(0);
// PRINT INTERNAL ERROR WITH FILE:FUNCTION:LINE AND PRINTING INTERNAL ERROR_CODE
#define PIE(fmt, ...) do{fprintf(stderr, "[%s:%s:%d] " fmt " (Internal ERROR_CODE: [%s])\n", __FILE__, __func__, __LINE__, ##__VA_ARGS__, convert_error_code_to_string(ierrno));}while(0);
#else
#define DEBUG_PRINT(fmt, ...)
#define PSE(fmt, ...)
#define PIE(fmt, ...)
#endif


#define E() do{exit(EXIT_FAILURE);}while(0);


/* --- BUILTIN EXPECT CONDITIONAL JUMP COMPILER HELPERS --- */
// Probably evaluates to true
#define likely(condition) __builtin_expect(condition, 1)
// Probably evaluates to false
#define unlikely(condition) __builtin_expect(condition, 0)


/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                           ERROR_CODE ENUM DEFINITION                                          */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

/**
 * @brief This enum contains all the possible error codes that the software may produce during execution, if an error occurs it will be documented here
 */
typedef enum ERROR_CODE
{
    NO_ERROR = 0,
    ERROR = -1,
    STRING_SIZE_INVALID = -2,
    STRING_SIZE_EXCEEDING_MAXIMUM = -3,
    TTY_ERROR = -4,
    SYSCALL_ERROR = -5,
    OPERATION_ABORTED = -6,
    // NULL passed as parameters to function
    NULL_PARAMETERS = -7, 
    EXIT_PROGRAM = -99, // A return value that asks whoever called the program to explicitly close it, we don't close it here because there may be some unsaved work or other close routines to handle
    START_REGISTRATION = -100, // Used to indicate that the user wants to start the registration process
    WRONG_PASSWORD = -101, // Used to indicate that the password provided is wrong
    USER_NOT_FOUND = -102, // Used to indicate that the user was not found in the most general sense, that means both during login and message sending
} ERROR_CODE;

typedef enum MESSAGE_CODE
{
    REQUEST_LOAD_UNREAD_MESSAGES = 7,
    REQUEST_DELETE_MESSAGE = 6,
    REQUEST_LOAD_SPECIFIC_MESSAGE = 5,
    REQUEST_LOAD_MESSAGE = 4,
    REQUEST_SEND_MESSAGE = 3,
    REQUEST_LIST_REGISTERED_USERS = 2,
    REQUEST_LOAD_PREVIOUS_MESSAGES = 1,
    MESSAGE_SENT = 0,
    MESSAGE_RECEIVED = 1,
    MESSAGE_ERROR = -1,
    MESSAGE_OPERATION_ABORTED = -2,
    MESSAGE_NOT_FOUND = -3,
    LOGOUT = -4,
} MESSAGE_CODE;

extern const char *convert_error_code_to_string(const ERROR_CODE code);

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              SIZES, CONSTANTS, VARIABLES                                      */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

enum sizes_and_constants{
    MESSAGE_SIZE_CHARS = 4096, // 4096
    // RSA_KEY_SIZE_BYTES = 256, // RSA-2048 BITS 256 bytes
    USERNAME_SIZE_CHARS = 64,
    PASSWORD_SIZE_CHARS = 256,
    MAX_PASSWORD_ATTEMPTS = 3,
};

extern const char *password_filename;
extern const char *data_filename;
extern const char *folder_suffix_user;
extern const char *file_suffix_user_data;

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              MESSAGE STRUCT AND METHODS                                       */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/**
 * @brief Structure representing a message in the Pretty Good Messenger (PGM) system.
 *
 * This structure contains the sender and recipient usernames, the length of the message,
 * and a flexible array member to hold the actual message content.
 * @warning: The 'message' field is a flexible array member, this allows the structure to be serialized as a contiguous block of memory,
 * and also to be sent as a single unit over network sockets. However, be careful when allocating and sending because sizeof(MESSAGE) will
 * just return the size of sender+recipient+int and not include the size of the flexible array member.
 *
 * @note write/send offsetof(MESSAGE, message) + message_length bytes.
 * @note read/recv offsetof(MESSAGE, message) first to get message_length, then read/recv message_length bytes to get the actual message.
 */
typedef struct MESSAGE {
    char sender[USERNAME_SIZE_CHARS];
    char recipient[USERNAME_SIZE_CHARS];
    uint32_t message_length;
    char message[];
} MESSAGE;

/** @example
size_t header_size = offsetof(MESSAGE, message);
size_t total_size = header_size + message_length;

MESSAGE *msg = malloc(total_size);
memset(msg, 0, header_size);
memcpy(msg->sender, sender, USERNAME_SIZE_CHARS);
memcpy(msg->recipient, recipient, USERNAME_SIZE_CHARS);
msg->message_length = htonl((uint32_t)message_length);
memcpy(msg->message, body, message_length);

// write/send total_size bytes
*/

/*
 * @note: initialize MESSAGE structure on stack before function call
*/ // TODO: DELETE SINCE I MOVED IT TO THE 2-CLIENT.C FILE
/*
extern inline ERROR_CODE create_message(LOGIN_SESSION_ENVIRONMENT* login_env, MESSAGE* message);
*/

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              LOGIN ENVIRONMENT                                                */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/** 
 * To sustain a user session we only need the name, there is no need for an explicit password because only the two following situations can occur
 * 1. The user is registered, so we can check whether the communicated ssh key matches the one in the <user-name>-ssh.pubkey
 * 2. The user is not registered, in that case we can ask if he wants to get registered.
 */
/** @deprecated
typedef struct LOGIN_SESSION_ENVIRONMENT {
	// User currently logged in
	char sender[USERNAME_SIZE_CHARS];
	// Other user that the logged user wants to send messages to
	char receiver[USERNAME_SIZE_CHARS];
	// Logged in user RSA-Key
	char sender_RSA_key[RSA_KEY_SIZE_BYTES];
	// Recipient user RSA-Key
	char Recipient_RSA_key[RSA_KEY_SIZE_BYTES];
	
} LOGIN_SESSION_ENVIRONMENT;
*/

typedef struct {
    // User currently logged in
    char sender[USERNAME_SIZE_CHARS];
    // Other user that the logged user wants to send messages to
    char receiver[USERNAME_SIZE_CHARS];
} LOGIN_SESSION_ENVIRONMENT;


/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              HOME FOLDER CREATION                                             */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

// @deprecated
// extern inline ERROR_CODE create_home_folder(void);

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                           PROGRAM NAME AND ASCII ART                                          */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

extern const char *program_name;
extern const char *ascii_art;
