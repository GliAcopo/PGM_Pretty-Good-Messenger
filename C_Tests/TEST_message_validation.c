#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"
#include "struct_definitions_and_methods.h"

// Assume ERROR_CODE is defined in settings.h, along with DEBUG_PRINT macro
// Add stubs for illustration if needed


extern ERROR_CODE check_length_message_fields(char *message_recipient,
                                              char *message_sender,
                                              char *message_subject,
                                              char *message_body);

extern variable_length_message *SECURE_create_variable_length_message(char *message_recipient,
                                                                      char *message_sender,
                                                                      char *message_subject,
                                                                      char *message_body);

void test_check_length_valid()
{
    char recipient[] = "Alice";
    char sender[] = "Bob";
    char subject[] = "Meeting";
    char body[] = "Let's meet at 10 AM.";

    ERROR_CODE result = check_length_message_fields(recipient, sender, subject, body);
    printf("Test valid lengths: %s\n", (result == NO_ERROR) ? "PASS" : "FAIL");
}

void test_check_length_empty_field()
{
    char recipient[] = "";
    char sender[] = "Bob";
    char subject[] = "Update";
    char body[] = "Content here";

    ERROR_CODE result = check_length_message_fields(recipient, sender, subject, body);
    printf("Test empty recipient: %s\n", (result == STRING_SIZE_INVALID) ? "PASS" : "FAIL");
}

void test_check_length_exceeding_field()
{
    char recipient[MAX_RECIPIENT_NAME_LENGTH + 10];
    memset(recipient, 'A', sizeof(recipient));
    recipient[sizeof(recipient) - 1] = '\0';

    char sender[] = "Bob";
    char subject[] = "Update";
    char body[] = "Content here";

    ERROR_CODE result = check_length_message_fields(recipient, sender, subject, body);
    printf("Test recipient exceeding max length: %s\n", (result == STRING_SIZE_EXCEEDING_MAXIMUM) ? "PASS" : "FAIL");
}

void test_create_variable_message_success()
{
    char recipient[] = "Alice";
    char sender[] = "Bob";
    char subject[] = "Hello";
    char body[] = "How are you?";

    variable_length_message *msg = SECURE_create_variable_length_message(recipient, sender, subject, body);
    if (msg != NULL)
    {
        printf("Test SECURE_create_variable_length_message success: PASS\n");
        free(msg); // since SECURE_create allocates memory
    }
    else
    {
        printf("Test SECURE_create_variable_length_message success: FAIL\n");
    }
}

void test_create_variable_message_failure_null()
{
    char *recipient = NULL;
    char sender[] = "Bob";
    char subject[] = "Hello";
    char body[] = "How are you?";

    variable_length_message *msg = SECURE_create_variable_length_message(recipient, sender, subject, body);
    printf("Test SECURE_create_variable_length_message with NULL input: %s\n", (msg == NULL) ? "PASS" : "FAIL");
}

int main()
{
    test_check_length_valid();
    test_check_length_empty_field();
    test_check_length_exceeding_field();
    test_create_variable_message_success();
    test_create_variable_message_failure_null();

    return 0;
}
