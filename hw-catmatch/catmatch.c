/*
PART ONE
1. 1, 2, 3
2. 1, 2
3. 2, 3
4. 2
5. #include <sys/types.h>, #include <sys/stat.h>, #include <fcntl.h>
6. 2, 7
7. 7
8. 3
9. null terminated
10. positive integer

PART TWO
I completed the TMUX exercise from Part 2
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int match(char*, char*);

int main (int argc, char **argv)
{
    int p_id = getpid();
    fprintf(stderr, "%d\n\n", p_id);

    FILE *ptr_fd;
    ptr_fd = fopen(argv[1], "r");

    char *line;
    size_t len = 0;
    ssize_t read;
    char *searchString = getenv("CATMATCH_PATTERN");

    while ((read = getline(&line, &len, ptr_fd)) != -1) {
        if (!searchString || !strstr(line, searchString))
        {
            printf("0 ");
        }
        else
        {
            printf("1 ");
        }
        printf("%s", line);
    }

    fclose(ptr_fd);
}