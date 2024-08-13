#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<string.h>

int main(int argc, char *argv[]) {
	int pid;
	int wstatus;
	int pipefd[2];
	char *str = "hello from Section B\n";
	char buf;
	int length;
	printf("Starting program; process has pid %d\n", getpid());

	FILE* f = fopen("./fork-output.txt", "w");
	fprintf(f, "BEFORE FORK\n");
	fflush(f);

	if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

	if ((pid = fork()) < 0) {
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */
	fprintf(f, "SECTION A\n");
	fflush(f);
	printf("Section A;  pid %d\n", getpid());
//	sleep(30);

	/* END SECTION A */
	if (pid == 0) {
		/* BEGIN SECTION B */
		fprintf(f, "SECTION B\n");
		fflush(f);
		printf("Section B\n");

		close(pipefd[0]);
		write(pipefd[1], str, strlen(str));
//		sleep(30);
//		sleep(30);
//		printf("Section B done sleeping\n");
		close(pipefd[1]);

		char *newenviron[] = { NULL };

		printf("Program \"%s\" has pid %d. Sleeping.\n", argv[0], getpid());
//		sleep(30);

		if (argc <= 1) {
		printf("No program to exec.  Exiting...\n");
		exit(0);
		}

		printf("Running exec of \"%s\"\n", argv[1]);
		dup2(fileno(f), 1);
		execve(argv[1], &argv[1], newenviron);
		printf("End of program \"%s\".\n", argv[0]);

		exit(0);
		/* END SECTION B */
	} else {
		/* BEGIN SECTION C */
		fprintf(f, "SECTION C\n");
		fflush(f);
		printf("Section C\n");
		waitpid(0, &wstatus, WUNTRACED | WCONTINUED);

		close(pipefd[1]);          /* Close unused write end */

        while (read(pipefd[0], &buf, 1) > 0)
            write(STDOUT_FILENO, &buf, 1);

		write(STDOUT_FILENO, "\n", 1);
//		sleep(30);
//		sleep(30);
//		printf("Section C done sleeping\n");
		close(pipefd[0]);
		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */
	fprintf(f, "SECTION D\n");
	fflush(f);
	printf("Section D\n");
//	sleep(30);

	/* END SECTION D */
}

