# [chat suggestions](https://chatgpt.com/c/682e06f9-124c-8008-9104-434aeff7d43e)

# Assignment
Realizzazione di un servizio di scambio messaggi supportato tramite un server
sequenziale o concorrente (a scelta). Il servizio deve accettare messaggi
provenienti da client (ospitati in generale su macchine distinte da quella
dove riese il server) ed archiviarli. 

L'applicazione client deve fornire ad un utente le seguenti funzioni:
1. Lettura tutti i messaggi spediti all'utente.
2. Spedizione di un nuovo messaggio a uno qualunque degli utenti del sistema.
3. Cancellare dei messaggi ricevuti dall'utente.
 
Un messaggio deve contenere almeno i campi Destinatario, Oggetto e Testo.

Si precisa che la specifica prevede la realizzazione sia dell'applicazione client che di quella server. Inoltre, il servizio potra' essere utilizzato solo
da utenti autorizzati (deve essere quindi previsto un meccanismo di autenticazione).       

# Development choices:
- I've chosen to develop a "concurrent server" instead of a sequential one.
- The messages have a fixed length, 
- I've chosen enums over #define precompiler macros because:
    - **Type-safety** A macro has no Type-safety and overload resolution, It is just a chunk of text. while enums are constant that have an integral type, so the compiler can perform ordinary type checking, overload resolution, warn about narrowing, etc.
    - **Debugging** A macro would only show a number, while in enums the symbolic name is preserved, so the debugger can display the name. E.g. `MAX_MESSAGE_LENGTH = 1024`
    - **Name collisions** When using macros unrelated headers can collide silently. Using enums the collisions are diagnosed
    - **Overhead** Both are compiled away into numbers. So there is no overhead
- All the code documentation in the form of comments will be written using the doxigen format.

# Code Documentation

## SECURE functions
Functions prefixed with `SECURE` in their name perform all kinds of security checks on inputs and other critical elements of the case. So they can be considered "secure" at the cost of the performance needed to perform all the security checks.

Despite the additional validation, `SECURE` functions are guaranteed to produce the same output as their `non-SECURE` counterparts under normal conditions. This ensures full compatibility and consistency, allowing developers to switch between versions without altering the expected behavior of their applications.

Note that `SECURE` functions may accept different input parameters compared to their standard equivalents. Any such differences will be clearly documented using Doxygen comments.

###  
