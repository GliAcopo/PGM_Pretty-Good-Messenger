static void dump_loggedin_users(void)
{
    P("Logged in users bitmap: 0x%08X", current_loggedin_users_bitmap);

    for (int i = 0; i < MAX_BACKLOG; i++)
    {
        if (current_loggedin_users[i] == NULL)
            continue;

        P("\t[%d]: %s", i, current_loggedin_users[i]);
    }
}

static void lock_loggedin_users_or_exit(void)
{
    for (int attempt = 1; attempt <= MAX_AQUIRE_SEMAPHORE_RETRY; attempt++)
    {
        struct timespec ts;

        if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        {
            PSE("clock_gettime() failed while locking loggedin users");
#ifdef DEBUG
            dump_loggedin_users();
#endif
            E();
        }

        ts.tv_sec += 5;

        for (;;) //for-ever muahahaha
        {
            if (sem_timedwait(&current_loggedin_users_semaphore, &ts) == 0)
                return; 

            if (errno == EINTR)
                continue;

            break;
        }

        if (errno == ETIMEDOUT)
        {
            P("Timed out waiting for current_loggedin_users_semaphore (%d/%d)",
              attempt, MAX_AQUIRE_SEMAPHORE_RETRY);
            continue;
        }

        PSE("sem_timedwait() failed for current_loggedin_users_semaphore");
        break;
    }

    P("Unable to acquire current_loggedin_users_semaphore, exiting");
#ifdef DEBUG
    dump_loggedin_users();
#endif
    E();
}

















static int send_all(int fd, const void *buffer, size_t length)
{
    const char *cursor = buffer;
    size_t remaining = length;
    while (remaining > 0)
    {
        ssize_t sent = send(fd, cursor, remaining, 0);
        if (sent < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (sent == 0)
        {
            return -1;
        }
        cursor += sent;
        remaining -= (size_t)sent;
    }
    return 0;
}

static int recv_all(int fd, void *buffer, size_t length)
{
    char *cursor = buffer;
    size_t remaining = length;
    while (remaining > 0)
    {
        ssize_t recvd = recv(fd, cursor, remaining, 0);
        if (recvd < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            return -1;
        }
        if (recvd == 0)
        {
            return 0;
        }
        cursor += recvd;
        remaining -= (size_t)recvd;
    }
    return 1;
}