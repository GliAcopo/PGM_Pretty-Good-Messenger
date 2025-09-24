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
} ERROR_CODE;

extern const char *convert_error_code_to_string(const ERROR_CODE code){}

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              SIZES, CONSTANTS, VARIABLES                                      */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

enum sizes{
    MESSAGE_SIZE_CHARS = 4096, // 4096
    RSA_KEY_SIZE_BYTES = 256, // RSA-2048 BITS 256 bytes
    USERNAME_SIZE_CHARS = 64,
};

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              MESSAGE STRUCT AND METHODS                                       */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
typedef struct MESSAGE {
    char sender[USERNAME_SIZE_CHARS];
    char recipient[USERNAME_SIZE_CHARS];
    char message[MESSAGE_SIZE_CHARS];
} MESSAGE;

/*
 * @note: initialize MESSAGE structure on stack before function call
 * */
extern inline ERROR_CODE create_message(LOGIN_SESSION_ENVIRONMENT* login_env, MESSAGE* message){}

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              LOGIN ENVIRONMENT                                                */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/** 
 * To sustain a user session we only need the name, there is no need for an explicit password because only the two following situations can occur
 * 1. The user is registered, so we can check whether the communicated ssh key matches the one in the <user-name>-ssh.pubkey
 * 2. The user is not registered, in that case we can ask if he wants to get registered.
 */
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

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                              HOME FOLDER CREATION                                             */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

extern inline ERROR_CODE create_home_folder(void);

/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */
/*                                           PROGRAM NAME AND ASCII ART                                          */
/* █████████████████████████████████████████████████████████████████████████████████████████████████████████████ */

extern const char *program_name;
extern const char *ascii_art;
