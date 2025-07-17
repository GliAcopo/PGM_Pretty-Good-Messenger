#pragma once /* only stops a header from being included more than once within the same translation unit, it does nothing to prevent each .c file that does #include "settings.h" from getting its own copy of everything in that header */

#include "settings.h"
#include <stddef.h> /* For the definition of NULL*/

/* -------------------------------------------------------------------------- */
/*                                  TYPEDEFS                                  */
/* -------------------------------------------------------------------------- */

typedef enum MAX_SIZE
{
    MAX_MESSAGE_LENGTH = 1024,
    MAX_MESSAGE_SUBJECT_LENGTH = 256,
    MAX_RECIPIENT_NAME_LENGTH = 64,
    MAX_SENDER_NAME_LENGTH = 64
} MAX_SIZE;

typedef struct fixed_length_message
{
    char message_recipient[MAX_RECIPIENT_NAME_LENGTH];
    char message_sender[MAX_SENDER_NAME_LENGTH];
    char message_subject[MAX_MESSAGE_SUBJECT_LENGTH];
    char message_body[MAX_MESSAGE_LENGTH];
} fixed_length_message;

/**
 * @brief Represents a message with variable-length fields
 *
 * Contains pointers to dynamically sized strings for recipient, sender, subject, and body
 */
typedef struct variable_length_message
{
    char *message_recipient;
    char *message_sender;
    char *message_subject;
    char *message_body;
} variable_length_message;

/* -------------------------------------------------------------------------- */
/*                                   METHODS                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Checks the length of the fields in a message for validity.
 *
 * This function validates the lengths of the provided message fields: recipient, sender, subject, and body. 
 * Each field is checked.
 * and does not exceed its respective maximum allowed length.
 *
 * If a field is found to be empty, or if it exceeds the maximum length,
 * an appropriate error code is returned.
 *
 * @param message_recipient Pointer to the recipient string.
 * @param message_sender Pointer to the sender string.
 * @param message_subject Pointer to the subject string.
 * @param message_body Pointer to the body string.
 *
 * @warning the function does not check for NULL pointers passed in parameters
 * 
 * @return ERROR_CODE
 * - NO_ERROR if all fields are within valid length bounds.
 * - STRING_SIZE_INVALID if any field has length 0.
 * - STRING_SIZE_EXCEEDING_MAXIMUM if any field exceeds its defined maximum length.
 */
inline ERROR_CODE check_length_message_fields(char *message_recipient,
                                              char *message_sender,
                                              char *message_subject,
                                              char *message_body)
{
    /** @note using only one variable will favour compiler optimizations, like keeping the variable in a register */
    register size_t string_size = strlen(message_recipient);
    if (string_size == 0)
    {
        DEBUG_PRINT("ERROR: The message_recipient field length is 0 (%zu)", string_size);
        return STRING_SIZE_INVALID;
    }
    else if (string_size > MAX_RECIPIENT_NAME_LENGTH)
    {
        DEBUG_PRINT("ERROR: The message_recipient field length (%zu) is exceeding the maximum length (%d)", string_size, MAX_RECIPIENT_NAME_LENGTH);
        return STRING_SIZE_EXCEEDING_MAXIMUM;
    }

    string_size = strlen(message_sender);
    if (string_size == 0)
    {
        DEBUG_PRINT("ERROR: The message_sender field length is 0 (%zu)", string_size);
        return STRING_SIZE_INVALID;
    }
    else if (string_size > MAX_SENDER_NAME_LENGTH)
    {
        DEBUG_PRINT("ERROR: The message_sender field length (%zu) is exceeding the maximum length (%d)", string_size, MAX_SENDER_NAME_LENGTH);
        return STRING_SIZE_EXCEEDING_MAXIMUM;
    }

    string_size = strlen(message_subject);
    if (string_size == 0)
    {
        DEBUG_PRINT("ERROR: The message_subject field length is 0 (%zu)", string_size);
        return STRING_SIZE_INVALID;
    }
    else if (string_size > MAX_MESSAGE_SUBJECT_LENGTH)
    {
        DEBUG_PRINT("ERROR: The message_subject field length (%zu) is exceeding the maximum length (%d)", string_size, MAX_MESSAGE_SUBJECT_LENGTH);
        return STRING_SIZE_EXCEEDING_MAXIMUM;
    }

    string_size = strlen(message_body);
    if (string_size == 0)
    {
        DEBUG_PRINT("ERROR: The message_body field length is 0 (%zu)", string_size);
        return STRING_SIZE_INVALID;
    }
    else if (string_size > MAX_MESSAGE_LENGTH)
    {
        DEBUG_PRINT("ERROR: The message_body field length (%zu) is exceeding the maximum length (%d)", string_size, MAX_MESSAGE_LENGTH);
        return STRING_SIZE_EXCEEDING_MAXIMUM;
    }

    return NO_ERROR;
}

inline void null_message(variable_length_message *message_struct)
{
    message_struct->message_recipient = NULL;
    message_struct->message_sender = NULL;
    message_struct->message_subject = NULL;
    message_struct->message_body = NULL;
}

/**
 * @brief Creates a variable length message structure.
 *
 * This function allocates memory for a `variable_length_message` struct and
 * populates its fields with the provided strings. It performs NULL checks on
 * the input strings and checks the length of the message fields.
 *
 * @param message_recipient A pointer to the recipient's address string.
 * @param message_sender A pointer to the sender's address string.
 * @param message_subject A pointer to the message subject string.
 * @param message_body A pointer to the message body string.
 * @return A pointer to the newly created `variable_length_message` struct, or
 *         NULL if an error occurred (e.g., NULL input, invalid length).
 * @note If any of the input strings are NULL or the message fields are too long,
 *          the function will return NULL.
 *
 * @warning The function does not copy the input strings. The pointers in the
 *       returned struct point directly to the provided strings.  Therefore,
 *       the caller must ensure that the strings remain valid for the lifetime
 *       of the message struct.  Do not free the strings immediately after
 *       calling this function.
 *
 */
inline variable_length_message *SECURE_create_variable_length_message(char *message_recipient,
                                                                      char *message_sender,
                                                                      char *message_subject,
                                                                      char *message_body)
{
    /* If retval is 0 then an ERROR occurred and NULL is returned, else we return the pointer to the allocated struct */
    int retval = 1;
    if (message_recipient == NULL)
    {
        DEBUG_PRINT("ERROR: message_recipient is NULL");
        retval = 0;
    }
    if (message_sender == NULL)
    {
        DEBUG_PRINT("ERROR: message_sender is NULL");
        retval = 0;
    }
    if (message_subject == NULL)
    {
        DEBUG_PRINT("ERROR: message_subject is NULL");
        retval = 0;
    }
    if (message_body == NULL)
    {
        DEBUG_PRINT("ERROR: message_body is NULL");
        retval = 0;
    }

    if (check_length_message_fields(message_recipient, message_sender, message_subject, message_body) != 0)
    {
        retval = 0;
    }

    variable_length_message *message_struct;
    if (retval)
    {
        variable_length_message *message_struct = malloc(sizeof(variable_length_message));
        message_struct->message_recipient = message_recipient;
        message_struct->message_sender = message_sender;
        message_struct->message_subject = message_subject;
        message_struct->message_body = message_body;
    }

    return (retval) ? message_struct : NULL;
}

