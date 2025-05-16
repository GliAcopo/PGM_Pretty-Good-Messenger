#include "main.c"

typedef enum MAX_SIZE
{
    MAX_MESSAGE_LENGTH = 1024,
    MAX_MESSAGE_SUBJECT_LENGTH = 256,
    MAX_RECIPIENT_NAME_LENGTH = 64,
    MAX_SENDER_NAME_LENGTH = 64
} MAX_SIZE;

typedef struct fixed_lenght_message
{
    char message_recipient[MAX_RECIPIENT_NAME_LENGHT];
    char message_sender[MAX_MESSAGE_SENDER_NAME_LENGHT];
    char message_subject[MAX_MESSAGE_SUBJECT_LENGHT];
    char message_body[MAX_MESSAGE_LENGHT];
} message;

typedef struct variable_lenght_message
{
};