/*
1.  1, 2, 3
2.  1, 2
3.  2, 3
4.  2
5.  #include <sys/types.h>
    #include <sys/stat.h>
    #include <fcntl.h>
6.  2, 7
7.  SO_ACCEPTCONN
8.  3
9.  null-terminated
10. An int greater than 0
*/
// I completed the TMUX exercise from Part 2

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    fprintf(stderr, "%d\n\n", getpid());

    char *CATMATCH_PATTERN = getenv("CATMATCH_PATTERN");
    

    FILE *stream;
    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    stream = fopen(argv[1], "r");
    if (stream == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    while ((nread = getline(&line, &len, stream)) != -1) {
        if (CATMATCH_PATTERN != NULL) {
            if (strstr(line, CATMATCH_PATTERN) == NULL) {
                fprintf(stdout, "0 ");
            } else {
                fprintf(stdout, "1 ");
            }
        } else {
            fprintf(stdout, "0 ");
        }
        fwrite(line, nread, 1, stdout);
    }

    free(line);
    fclose(stream);

    return 0;
}