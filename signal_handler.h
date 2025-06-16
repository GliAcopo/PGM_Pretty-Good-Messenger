#define _GNU_SOURCE /* for sigabbrev_np(3)            */
#include <signal.h>
#include <error.h> /* For error handling and printing (strerror)*/
#include <errno.h> /* For errno */
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "settings.h" /* Of course for various settings definitions along with ERROR_CODE definition */

/**
 * @brief Catches all signals and logs a message indicating if no action is taken.
 *
 * This signal handler logs information about the signal that was caught.
 * It first attempts to log an abbreviated signal name (when available via glibc's sigabbrev_np)
 * along with the signal number, or a full description if the abbreviated form is unavailable.
 * @note The message is fistly logged inside a char array and then written directly to standard error using write(2), so it is async-signal-safe.
 *
 * @param sig The signal number.
 * @param si Pointer to a siginfo_t structure (unused) (will be used in the future tho) @todo: Update comment when done.
 * @param ucontext Pointer to a ucontext structure representing the user context (unused) (Don't think I need it, maybe...).
 */
static void catch_all(int sig, siginfo_t *si, void *ucontext)
{
    // (void)si; (void)ucontext;       /* just to silence warnings*/

    const char *abbr = NULL;
#ifdef __GLIBC__
    abbr = sigabbrev_np(sig); /* returns abbreviated signal names "INT", "TERM", ...*/
#endif
    const char *desc = strsignal(sig);

    char msg[128];
    if (abbr)
    { /* If abreviated name is available then we use it */
        snprintf(msg, sizeof msg,
                 "Caught SIG%s (%d) - no action taken\n", abbr, sig);
    }
    else
    { /* If abbreviated name is not available then we only use the description */
        snprintf(msg, sizeof msg,
                 "Caught signal %d (%s) - no action taken\n",
                 sig, desc ? desc : "unknown"); /* If we do have an extended description then we print it or else we print unknown */
    }
    /* write(2) is async-signal-safe */
    write(STDERR_FILENO, msg, strlen(msg));
}

ERROR_CODE install_signal_handler(void)
{
    struct sigaction sa = {.sa_sigaction = catch_all, /* Set the handler function */
                           .sa_flags = SA_SIGINFO}; 
    /* We firstly initialize our personal signal bimask so that we can update it later while we ingore signals in the for loop below */
    if(sigemptyset(&sa.sa_mask) < 0) /* Man page: "initializes  the  signal set given by set to empty, with all signals excluded from the set."*/
    {
        DEBUG_PRINT("Error: sigemptyset(%p) failed. Details: [%s]", &sa.sa_mask, strerror(errno));
        return(SYSCALL_ERROR);
    }
    for (int s = 1; s < NSIG; ++s)
    {                                     /* These macros will expand to the actual number of the signal */
        if (s == SIGKILL || s == SIGSTOP) /* cannot be caught so we just skip them */
            continue;                     /* Basically a skip command for the loop */
        if(sigaction(s, &sa, NULL) == -1)          /* ignore the errors we handle */
        {
            DEBUG_PRINT("Error: sigaction(%d) failed. Details: [%s]", s, strerror(errno));
            return(SYSCALL_ERROR);
        }
    }
    /* MAN: "The  sigaction()  system  call  is used to change the action taken by a process on receipt  of  a  specific  signal. signum  specifies the signal and can be any valid signal except SIGKILL and SIGSTOP. If act is non-NULL, the new action for signal signum is installed  from act.  If oldact is non-NULL, the previous action is saved in oldact." */

    /* Wait forever; use Ctrl-C, ‘kill’, etc. to test. */
    /*
    for (;;) // basically a cool way to say while(1) 
        pause();
    */
   return(NO_ERROR);
}

/* MAN PAGE:
The sigaction structure is defined as something like:

           struct sigaction {
               void     (*sa_handler)(int);
               void     (*sa_sigaction)(int, siginfo_t *, void *);
               sigset_t   sa_mask;
               int        sa_flags;
               void     (*sa_restorer)(void);
           };
*/