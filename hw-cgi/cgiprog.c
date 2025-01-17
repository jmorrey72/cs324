// 1. USER         PID    PPID NLWP     LWP S CMD
//    jmorrey   370649  370083    1  370649 S echoserveri
// 2. One process and one thread, because echoserveri just listens at a port and doesn't create any threads or processes.
// 3. It connects to the next client
// 4. USER         PID    PPID NLWP     LWP S CMD
//    jmorrey   372996  370083    1  372996 S echoserverp
//    jmorrey   373119  372996    1  373119 S echoserverp
//    jmorrey   373135  372996    1  373135 S echoserverp
//    jmorrey   373155  372996    1  373155 S echoserverp
// 5. 4 processes and 4 threads, because echoserverp creates a child for each connection.
// 6. USER         PID    PPID NLWP     LWP S CMD
//    jmorrey   375867  370083    4  375867 S echoservert
//    jmorrey   375867  370083    4  375910 S echoservert
//    jmorrey   375867  370083    4  375928 S echoservert
//    jmorrey   375867  370083    4  375944 S echoservert
// 7. 1 process and 4 threads, because echoservert created a new thread to handle each new connection.
// 8. USER         PID    PPID NLWP     LWP S CMD
//    jmorrey   379389  370083    9  379389 S echoservert_pre
//    jmorrey   379389  370083    9  379390 S echoservert_pre
//    jmorrey   379389  370083    9  379391 S echoservert_pre
//    jmorrey   379389  370083    9  379392 S echoservert_pre
//    jmorrey   379389  370083    9  379393 S echoservert_pre
//    jmorrey   379389  370083    9  379394 S echoservert_pre
//    jmorrey   379389  370083    9  379396 S echoservert_pre
//    jmorrey   379389  370083    9  379397 S echoservert_pre
//    jmorrey   379389  370083    9  379398 S echoservert_pre
// 9. 1 process and 9 threads, because echoservert_pre creates 8 extra threads to handle client communications by accessing the sbuf populated by accept.
// 10. 1
// 11. 8
// 12. New connections or clients
// 13. the producer to populate sbuf with new connfds
// 14. A new connection from a new client
// 15. The producer adding the new connection to the sbuf
// 16. 1
// 17. The producer adding the new connection to the sbuf
// 18. Another new connection
// 19. Process-based, thread-based, threadpool-based
// 20. echoservert_pre
// 21. echoservert and echoservert_pre
// 22. echoserverp

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLINE 1024

int main(void) {
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], response[MAXLINE];
    int n1=0, n2=0;

    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        sprintf(response, "The query string is: %s", buf);
    }

    /* Generate the HTTP response */
    printf("Content-type: text/plain\r\n");
    printf("Content-length: %d\r\n\r\n", (int)strlen(response));
    printf("%s", response);
    fflush(stdout);
    exit(0);
}