### Signal handling
#### Server
##### Threads

* At the start, each thread sets its signal mask (per thread) with `pthread_sigmask()`. (Because signal handlers are process-wide, while signal masks are per-thread).
* All interruptions get masked before executing critical operations such as saving files, using the `deactivate_interrupts()` function.
  * `deactivate_interrupts()` is **local per-thread**. And uses a boolean variable `signals_enabled` to check whether signals are enabled or not.
  * After operations are completed, the function `activate_interrupts()` gets called and all the interrupts happened during that time get served.
	  * By using this logic:
		  * the thread does not terminate itself immediately on shutdown,
		   * the thread complete the current critical operation and only then exit at the next safe point.
  * when executing`activate_interrupts()` the thread checks `shutdown_requested`:
	  * if set, it cleanly terminates the thread at that safe point.
	  * NOTE: No thread can “reactivate” another thread’s interrupts.
* `SIGPIPE` is ignored process-wide with `ignore_sigpipe()` (so we never kill the server due to a broken pipe).
* If `send_all()` fails with `EPIPE` or `ECONNRESET`, the client is gone: clean up, and terminate the thread.
  * In cases like `REQUEST_SEND_MESSAGE`, if a disconnect happens around file I/O,  partially written files are removed before exiting.
	* For other errors: log and clean up before terminating the thread.
##### Main Thread
* At the start, the server setup:
  * `SIGPIPE` is ignored (via `ignore_sigpipe()` called in main).
  * The server ignores child death signals (`SIGCHLD`, `SIGCLD`).
  * Termination signals (`SIGINT`, `SIGTERM`, `SIGHUP`) are **blocked** via `pthread_sigmask()` before creating any worker threads, so they do not asynchronously interrupt workers.

* Signal reception is centralized:
  * A dedicated signal-management thread (or the main thread, if no separate thread is used) waits for termination signals using `sigwait()`/`sigwaitinfo()`.
  * When a termination signal is received, the server:
    1. sets `shutdown_requested = 1` (global, atomic),
    2. stops accepting new connections (close listening socket or otherwise unblock `accept()`),
    3. wakes all worker threads so they can exit (e.g. `shutdown(fd, SHUT_RDWR)` on each active client socket or worker-side poll timeouts),
    4. exits once all resources are released (threads may be joined or detached; in both cases, socket/resource cleanup must be guaranteed).
* “Propagate signal to other threads” requirement:
  * The server must **not** use `kill(0, SIGTERM)` for thread propagation (process group side-effects).
  * Propagation is implemented **inside the process** using `shutdown_requested` + explicit wakeups.
* If the server receives any other unmapped signal, then it terminates all the other threads before terminating itself.
  * Implemented as: set `shutdown_requested`, stop accept, wake workers, then exit.
#### Client
* The client handles signals through `int handler_client_main(int signum)`.
* `SIGPIPE` restarts the server access loop (asks IP again).
  * Specification requirement: the client must not be terminated by SIGPIPE.
    * Either ignore SIGPIPE process-wide, or use per-send suppression.
  * When a send fails with `EPIPE` / `ECONNRESET`, treat it as “server disconnected” and restart the connection loop.
* Every other signal sends `LOGOUT` to the server and closes the client program.
  * The `LOGOUT` send is best-effort:
    * if sending fails because the server is already gone, the client must still close and exit.
* TO IMPLEMENT: The server has to accept at any given time phase the `LOGOUT` message so that the client can disconnect whenever it wants.
  * Specification requirement: the server must treat `LOGOUT` as a valid command at any point where it is waiting for a `MESSAGE_CODE` from the client, and must immediately clean up that connection and terminate the worker thread.
