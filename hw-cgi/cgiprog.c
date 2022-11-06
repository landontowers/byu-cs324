/*
1.  USER         PID    PPID NLWP     LWP S CMD
    latowers 3129508 3124170    1 3129508 S echoserveri
2.  1 process/thread because echoserveri uses no concurrency, meaning only one client can connect at a time.
3.  The other nc processes stop hanging and connect one at a time, in the order they were started in, once the previous process is terminated.
4.  USER         PID    PPID NLWP     LWP S CMD
    latowers 3133353 3124170    1 3133353 S echoserverp
    latowers 3133473 3133353    1 3133473 S echoserverp
    latowers 3133519 3133353    1 3133519 S echoserverp
    latowers 3133566 3133353    1 3133566 S echoserverp
5.  There are 4 processes: 1 parent and 3 children (1 for each connected client). Each process has just one thread, all with different ID's, which confuses me because I was under the impression that they still shared a common thread in echoserverp.
6.  USER         PID    PPID NLWP     LWP S CMD
    latowers 3136192 3124170    4 3136192 S echoservert
    latowers 3136192 3124170    4 3136243 S echoservert
    latowers 3136192 3124170    4 3136283 S echoservert
    latowers 3136192 3124170    4 3136329 S echoservert
7.  There is 1 process with 4 seperate threads, since echoservert uses simple threads for concurrency.
8.  USER         PID    PPID NLWP     LWP S CMD
    latowers 3138964 3124170    9 3138964 S echoservert_pre
    latowers 3138964 3124170    9 3138965 S echoservert_pre
    latowers 3138964 3124170    9 3138966 S echoservert_pre
    latowers 3138964 3124170    9 3138967 S echoservert_pre
    latowers 3138964 3124170    9 3138968 S echoservert_pre
    latowers 3138964 3124170    9 3138969 S echoservert_pre
    latowers 3138964 3124170    9 3138970 S echoservert_pre
    latowers 3138964 3124170    9 3138971 S echoservert_pre
    latowers 3138964 3124170    9 3138972 S echoservert_pre
9.  There is 1 process with 9 separate threads. An interesting difference between this and the simple threading is that this has its thread ID's within one of each other, instead of whatever thread is available.
10. 1
11. 8
12. A client to connect with.
13. The semaphore to increment.
14. A client successufully connects to the server via accept().
15. sbuf_insert() uses sem_post() to unlock the buffer.
16. 1
17. A client successufully connects to the server via accept().
18. Another client to connect.
19. echserverp, echoservert_pre, echoservert. I am using assuming that the question implies just one connected client, and that we are including the cost of setting up the server, not just the connection itself. My answer changes if we not assuming these things. I think the question could be more specific.
20. echoservert_pre
21. echoserverp
22. echoservert seems easiest to implement because you don't have to worry about reaching the thread limit as in echoservert_pre, but you get the benefits of threads. Although echoserverp may be easier simply because I'm more familiar with forks than threads, and semaphores aren't necessary to share data.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main (void) {
    char *qs = getenv("QUERY_STRING");
    printf("Content-type: text/plain\r\nContent-length: %d\r\n\r\nThe query string is: %s\n", 21+(int)strlen(qs), qs);
}