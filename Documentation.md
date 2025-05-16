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
- The messages have a fixed lenght, 
- I've chosen enums over #define precompiler macros because:
    - **Type-safety** A macro has no Type-safety and overload resolution, It is just a chunk of text. while enums are constant that have an integral type, so the compiler can perform ordinary type checking, overload resolution, warn about narrowing, etc.
    - **Debugging** A macro would only show a number, while in enums the symbolic name is preserved, so the debugger can display the name. E.g. `MAX_MESSAGE_LENGTH = 1024`
    - **Name collisions** When using macros unrelated headers can collide silently. Using enums the collisions are diagnosed
    - **Overhead** Both are compiled away into numbers. So there is no overhead


# Code Documentation

###  
