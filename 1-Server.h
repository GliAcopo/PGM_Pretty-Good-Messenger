/**
 * @file 1-Server.h
 * @author Jacopo Rizzuto (jacoporizzuto04@gmail.com)
 * @brief .h file for the server process
 * @version 0.1
 * @date 2025-07-17
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once /** @note Not truly necessary but you never know...*/

enum server_sizes_and_costants {
    MAX_AQUIRE_SEMAPHORE_RETRY = 3,
    MAX_AQUIRE_SEMAPHORE_TIME_WAIT_SECONDS = 10
};

typedef struct {
    int connection_fd;
    int thread_index;
} thread_args_t;