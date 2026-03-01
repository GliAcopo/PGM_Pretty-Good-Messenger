
# Specifiacations to be implemented
Messaging service

Implementation of a message exchange service supported by a sequential or concurrent server (your choice). The service must accept messages coming from clients (generally hosted on machines different from the one where the server resides) and store them.

The client application must provide a user with the following functions:
1. Reading all messages sent to the user.
2. Sending a new message to any of the users in the system.
3. Deleting messages received by the user.

A message must contain at least the fields Recipient, Subject, and Text.

It is specified that the specification requires the implementation of both the client and the server application. Moreover, the service can be used only by authorized users (an authentication mechanism must therefore be provided).

For mixed Unix/Windows projects, it is up to you which of the two applications to develop for one of the two systems.

## My choices:
1. Concurrent server
2. Unix-based client application

# Implementation details
### Message handling
When authentication succeeds, the server thread dedicated to the client enters an infinite loop waiting for a `MESSAGE_CODE` value (enum in `3-Global-Variables-and-Functions.c`).  
Unless otherwise stated, every request receives an `ERROR_CODE` reply before any further payload exchange.

Note: unread messages are messages whose filename starts with the `UNREAD` flag: `UNREAD<YYYYMMDDHHMMSS><counter><file_suffix_user_data>`
Note: the message count .DATA message count is to be discontinued, is not useful as a feature.
Note: When referring to "save the MESSAGE structure in the file" I mean that it is saved as WHOLE
- the structure contains a flexible array, so sizeof(MESSAGE) will not give the body of the message but only the first part of the structure.
    - Use offsetof(MESSAGE, message) + message_length bytes.
    - Do NOT use sizeof(MESSAGE).
- see `3-Global-Variables-and-Functions.h`->`MESSAGE STRUCT AND METHODS` for details.

- `REQUEST_SEND_MESSAGE`:
    - Server receives the `MESSAGE` header (read `offsetof(MESSAGE, message)` bytes, not `sizeof(MESSAGE)`).
        - Header fixed fields are: sender, recipient, subject, message_length.
    - Server checks that the recipient folder exists.
        - If not found, reply with `USER_NOT_FOUND` and stop.
        - Otherwise reply with `NO_ERROR` and continue.
    - Server validates `subject` from header.
        - If subject is empty, server responds with `ERROR_CODE` `STRING_SIZE_INVALID`.
    - Server validates `message_length` from the header.
        - If `message_length == 0` then the message is empty and the server responds with `ERROR_CODE` `STRING_SIZE_INVALID`.
        - If `message_length > MESSAGE_SIZE_CHARS` then the message is invalid and the server responds with `ERROR_CODE` `STRING_SIZE_INVALID`.
    - Server receives the message body (exactly `message_length` bytes).
    - The server will always overwrite the `MESSAGE` `SENDER` field with the authenticated user. (Since that field is used by the receiver to know the sender), and this is why the Client can set that field to null anyway.
    - Server creates a message file in the recipient folder named `<YYYYMMDDHHMMSS><(num to avoid name clash)><file_suffix_user_data>`. Opened "exclusively" to avoid data access race among threads. ("num to avoid name clash" is used if a message is received in the same second, very unlikely but possible, if the server is not able to open the file exclusively then this counter increates, so the server tries to open a differently named file).
        - First bytes: serialized `MESSAGE` struct.
        - NOTE: careful about network byte order
        - File is marked as `UNREAD` (filename starts with `UNREAD` as said above)
    - For network/file protocols use a fixed-width type (e.g., `uint32_t`) and define byte order (usually network byte order).
        - Sender: `uint32_t len_net = htonl((uint32_t)len);`
        - Receiver: `len = ntohl(len_net);`
- `REQUEST_LIST_REGISTERED_USERS`:
    - Server scans the server folder (current working directory) and lists every folder ending with `folder_suffix_user`, (that is the names of the registered users)
    - Server replies with a `uint32_t` length prefix in network byte order, then waits for `NO_ERROR`.
        - Client replies with `NO_ERROR`
        - Else the operation gets aborted
    - Server replies with a newline-separated list of usernames (suffix stripped), null-terminated `\0`.
- `REQUEST_LOAD_MESSAGE`:
    - Server lists user message files, sorts by filename (only filenames in the directory, not the entire path) (descending), and prepares the list of filenames divided by newlines. Ends with null termination.
    - Server replies with a `uint32_t` length prefix in network byte order, then waits for `NO_ERROR` before sending the list.
        - Client replies with `NO_ERROR`
        - Else the operation gets aborted
        - If list is to big @todo handle send of bigger data portions (is it even necessary?). (NOT IMPLEMENTED FOR NOW)
    - Server waits for `MESSAGE_CODE`
    - Client on his side prints to the user the sent list of filenames.
        - Client user chooses not to load any message.
            - Client responds with `MESSAGE_OPERATION_ABORTED` from the `MESSAGE_CODE` enum in `3-Global-Variables-and-Functions.c`.
            - Server does nothing and restarts the loop
        - Client user chooses a message by writing the assigned number by the program listing.
            - Client responds to the server with `REQUEST_LOAD_SPECIFIC_MESSAGE`
            - Client then sends to the server the corresponding filename
            - Server searches for message in the user folder
                - if found, server responds with `NO_ERROR`
                    - Server sends message struct and message body
                        - IF the message is `UNREAD` then the `UNREAD` marker gets removed from the filename
                - if not found server sends `MESSAGE_NOT_FOUND`
- `REQUEST_LOAD_UNREAD_MESSAGES`:
    - Behaves exactly like the first part of `REQUEST_LOAD_MESSAGE`, but only lists `UNREAD` messages.
    - After sending the list to the client, the loop restarts (no message sending is expected), that is because it is supposed to be used in conjuction with `REQUEST_LOAD_MESSAGE`.
- `REQUEST_DELETE_MESSAGE`:
    - Works **exactly** as `REQUEST_LOAD_MESSAGE`, except for the fact that the selected message gets deleted.
        - if the deletion was a success then the server responds with `NO_ERROR`
        - if the deletion was a failure then the server responds with anything else.
- `LOGOUT`:
    - The connection gets terminated
    - The client closes

### File and persistent storage
Only the server stores the messages, since the professor said that I cannot assume that I have storage permissions on Client devices.
- For every registered user, a folder with its name and `folder_suffix_user` is created.

- The file named with the name specified in the `password_filename` variable contains the user password.

- The file named with the name specified in the `data_filename` variable conains
    - First line: unsigned integer: number of messages received (currently in the folder)

- Every single file apart from those is a message:
    - Naming convention for the message files is `<date of receival><time of receival><file_suffix_user_data>`.
        - The format for the date is standard ISO 8601: YYYYMMDDHHMMSS without separators, so file fetching and ordering is easier.
    - The first thing to be stored in the file is the MESSAGE struct
    - then the message contents themselves

### Message exchange between two or more users
#### Logged in users management 
- At the start of the Server application, three relevant values are initialized:
    - `char* current_loggedin_users`: array of dynamically allocated usernames of currently logged in users
    - `unsigned int current_loggedin_users_bitmap`: The occupation bitmap of the array (initialized at 0)
    - `sem_t current_loggedin_users_semaphore`: The semaphore for synchronizing thread access to these resources.
    - Access to these resources always requires the semaphore, except for a debug-only dump on fatal errors.

When a connection is estabilished with the server, and the user authenticates, the server threads handling the connection:

- Enters a loop where it:
    - Tries to aquire the lock on the `current_loggedin_users_semaphore` for 5 seconds. (The system call is also checked for erroneus exit upon receiving SIGNAL)
    - If unable to, retries for up to `MAX_AQUIRE_SEMAPHORE_RETRY` (defined in `3-Global-Variables-and-Functions.h`).
    - If unable to, terminates the entire application.
        - _DEBUG: Every action is described and printed_

- Checks whether the username is already present in `current_loggedin_users`. If it is, the login is rejected.

- Allocates for the username of the logged in user (up to `USERNAME_SIZE_CHARS`) only after authentication succeeds.
    - _DEBUG: allocation is checked and printed_

- Checks the `current_loggedin_users_bitmap`, for an empty space. (That is, up until `MAX_BACKLOG` positions)
    - If found, the position index in the array is **saved** into a local variable `current_loggedin_users_used_index`.
        - _DEBUG: the bitmap is printed before and after the modification of the program_
    - There should never be an occasion in wich a free position is not found, since the lenght of the array is exactly how many users can be logged in at a time (that is `MAX_BACKLOG`)
        - _DEBUG: In that case, a DEBUG defined function `dump_loggedin_users()` gets called, said function prints the bitmap and the names of the loggedin users from the array (without aquiring semaphore) before closing the entire application_

- Writes the pointer to the allocated space to the name in the found array position.

- Restores `current_loggedin_users_semaphore` access.

Upon closing the connection, the thread cleanup routine includes:
- Enters a loop where it:
    - Tries to aquire the lock on the `current_loggedin_users_semaphore` for 5 seconds. (The system call is also checked for erroneus exit upon receiving SIGNAL)
    - If unable to, retries for up to `MAX_AQUIRE_SEMAPHORE_RETRY` (defined in `3-Global-Variables-and-Functions.h`).
    - If unable to, terminates the entire application.
        - _DEBUG: Every action is printed_

- Frees the `current_loggedin_users_used_index` entry number in the `current_loggedin_users` array.

- Removes the entry from the `current_loggedin_users_bitmap`.

---

## Signal handling

The explicit handling of `SIGINT`/`SIGTERM` is done by a dedicated signal thread.  
The main problem is `SIGPIPE` when sending on broken connection, that is avoided by using `MSG_NOSIGNAL` in socket sends (`send_all` declared in `3-Global-Variables-and-Functions.c`).

### Shutdown phase

- In `main`, `SIGINT` and `SIGTERM` are blocked with `pthread_sigmask`, that enables the signal mask to be inherited by worker threads.
- Global flag: `volatile sig_atomic_t shutdown_now` is set by the signal thread when `SIGINT`/`SIGTERM` is received.
  - Said signal thread runs `sigwait()` on the blocked signal set.
- When `sigwait()` receives `SIGINT`/`SIGTERM`, the signal thread:
  - calls `shutdown(listen_fd, SHUT_RDWR)`,
  - calls `close(listen_fd)`,
  - sets `shutdown_now = 1`.
- Main accept loop checks `shutdown_now`:
  - before calling `accept()`,
  - after `accept()` failure.
- During global shutdown:
    - The main thread calls `shutdown(fd, SHUT_RDWR)` on active client sockets, so that worker threads will wake up from blocking `recv()` calls and check `shutdown_now` to exit.
    - Then joins worker threads to ensure all threads finished their cleanup before closing the whole application.

### Worker-side behavior

- Worker send path uses `send(..., MSG_NOSIGNAL)` inside `send_all`.
- Worker receive path uses `recv_all` and handles:
  - `recv == 0` as peer disconnect,
  - `errno == EINTR` as retry. (Even if `SIGINT`/`SIGTERM` are blocked, other signals can cause `recv()` to be interrupted)
- Keepalive is enabled on each accepted socket:
  - `SO_KEEPALIVE`, the only one currently enabled in the code, is required to enable TCP keepalive probes. So that client disconnections will eventually be detected by the server, even if the client doesn't close the connection properly. This ensures that there are no ghost loggedin users.
  - `TCP_KEEPIDLE`,
  - `TCP_KEEPINTVL`,
  - `TCP_KEEPCNT`.
This is used to reduce ghost/stale connections.

#### SO_RCVTIMEO (optional extension, not currently enabled in code)

- Current code does **not** set `SO_RCVTIMEO`.
- It can be useful to make blocking `recv()` wake up periodically, so worker loops can check `shutdown_now` without waiting indefinitely on quiet sockets.
- It is not considered necessary since the `SO_KEEPALIVE` mechanism will eventually detect dead connections, but it can be a useful addition to make shutdown more responsive.
- The behaviour can be controlled by two compile-time macros, but needs some addetional code to handle the `EAGAIN`/`EWOULDBLOCK` errors from `recv()` as timeout wakeups:
  - `ENABLE_SOCKET_RCVTIMEO` (feature toggle),
  - `SOCKET_RCVTIMEO_SECONDS` (timeout value, e.g. `60`).
- Suggested behavior when enabled:
  - Set `SO_RCVTIMEO` once after `accept()`.
  - Treat `EAGAIN` / `EWOULDBLOCK` as timeout wakeups, then re-check `shutdown_now` and continue/exit accordingly.


# DEBUG

## Hold connection mode in the server

Use the `DEBUG` flag during application compilation to enable debug output in the server.
If the applcation is not compiled with the `DEBUG` flag, the debug output function will not be even present within the code. This was a personal choice I've made so that the executable's size can be made smaller and there is no need to include other conditional jumps every time a debug message gets printed.
