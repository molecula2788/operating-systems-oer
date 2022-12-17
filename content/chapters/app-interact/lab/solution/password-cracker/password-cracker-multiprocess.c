#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <openssl/sha.h>

#define NUM_WORKERS 4

#define PASSWORD_LEN 4

#define PASSWORD_HASH "\x59\xa5\xab\xc2\xa9\x9b\x95\xbe"		\
	"\x73\xc3\x1e\xa2\x72\xab\x0f\x2f"				\
	"\x2f\xe4\x2f\xec\x30\x36\x71\x55"				\
	"\xcb\x73\xf6\xf6\xce\xf1\xf4\xe6"				\
	"\xee\x37\xf5\x86\xcb\xd0\x2c\xc7"				\
	"\x38\xa8\x7a\x5d\x6a\xdd\x3b\xa3"				\
	"\x1d\xbe\xaf\x39\xec\x77\xca\xd9"				\
	"\x10\x83\x7c\x94\xc6\x58\x37\xfb"
#define HASH_LEN 64

int check_password(char *password, int len)
{
	unsigned char hash[HASH_LEN];

	SHA512((unsigned char *)password, len, hash);

	return memcmp(hash, PASSWORD_HASH, HASH_LEN) == 0;
}

void worker(int idx, int request_pipe_fd, int result_pipe_fd)
{
	char first_char_begin, first_char_end;
	char c;
	int ret;
	char password[PASSWORD_LEN+1];
	int i, k;
	int found = 0;
	int v;

	ret = read(request_pipe_fd, &first_char_begin, sizeof(first_char_begin));
	if (ret <= 0) {
		perror("read");
		return;
	}

	ret = read(request_pipe_fd, &first_char_end, sizeof(first_char_end));
	if (ret <= 0) {
		perror("read");
		return;
	}

	for (c = first_char_begin; c <= first_char_end && !found; c++) {
		password[0] = c;

		for (i = 1; i < PASSWORD_LEN; i++)
			password[i] = 'a'-1;


		/*
		 * Generate all possible combinations.
		 */
		k = 1;

		while (k >= 1) {
			if (password[k] < 'z') {
				password[k]++;

				if (k < PASSWORD_LEN) {
					k++;
					password[k] = 'a'-1;
				}
			}

			if (k == PASSWORD_LEN) {
				/* Check one combination. */
				if (check_password(password, PASSWORD_LEN)) {
					found = 1;
					break;
				}
			}

			if ((password[k] == 'z') || (k == PASSWORD_LEN)) {
				k--;
			}
		}
	}

	if (found) {
	    /*
	     * Found the password. Send the password length and the password
	     * through the pipe.
	     */
	    v = PASSWORD_LEN;

	    ret = write(result_pipe_fd, &v, sizeof(v));
	    if (ret < 0) {
		perror("write");
		exit(1);
	    }

	    ret = write(result_pipe_fd, password, PASSWORD_LEN);
	    if (ret < 0) {
		perror("write");
		exit(1);
	    }
	} else {
	    /*
	     * Didn't find the password. Send the value 0 through the pipe.
	     */
	    v = 0;

	    ret = write(result_pipe_fd, &v, sizeof(v));
	    if (ret < 0) {
		perror("write");
		exit(1);
	    }
	}
}

void create_workers(int *request_pipefd, int *result_pipefd)
{
	int tmp_request_pipe[2];
	int tmp_result_pipe[2];
	int ret;
	int pid;
	int i;

	for (i = 0; i < NUM_WORKERS; i++) {
		/* Create the request pipe. */
		ret = pipe(tmp_request_pipe);
		if (ret < 0) {
			perror("pipe");
			exit(1);
		}

		/* Create the result pipe. */
		ret = pipe(tmp_result_pipe);
		if (ret < 0) {
			perror("pipe");
			exit(1);
		}

		pid = fork();
		if (pid < 0) {
			perror("fork");
			exit(1);
		}

		if (pid == 0) {
			/*
			 * In child process.
			 *
			 * Close the unused pipe ends:
			 * - the write end of the request pipe, since the child will only read from this pipe.
			 * - the read end of the result pipe, since the child will only write to this pipe.
			 */
			close(tmp_request_pipe[1]);
			close(tmp_result_pipe[0]);

			/* Call the worker function. */
			worker(i, tmp_request_pipe[0], tmp_result_pipe[1]);

			exit(0);
		}

		/*
		 * In parent process.
		 *
		 * Close the unused pipe ends:
		 * - the read end of the request pipe, since the parent will only write to this pipe.
		 * - the write end of the result pipe, since the parent will only read from this pipe.
		 */
		close(tmp_request_pipe[0]);
		close(tmp_result_pipe[1]);

		request_pipefd[i] = tmp_request_pipe[1];
		result_pipefd[i] = tmp_result_pipe[0];
	}
}

int main()
{
	int i;
	int request_pipefd[NUM_WORKERS];
	int result_pipefd[NUM_WORKERS];
	int len;
	int ret;
	char char_list[] = "abcdefghijklmnopqrstuvwxyz";
	int char_list_len = sizeof(char_list) - 1;
	char password[PASSWORD_LEN+1];
	char first_char_begin, first_char_end;
	int chunk_size;

	create_workers(request_pipefd, result_pipefd);

	/*
	 * Send the first character of the password to each worker.
	 */
	chunk_size = char_list_len / NUM_WORKERS;

	for (i = 0; i < NUM_WORKERS; i++) {
		first_char_begin = char_list[i * chunk_size];
		first_char_end = char_list[(i+1) * chunk_size - 1];

		if (i == NUM_WORKERS - 1)
		    first_char_end = char_list[char_list_len - 1];

		ret = write(request_pipefd[i], &first_char_begin, sizeof(char));
		if (ret < 0) {
			perror("write");
			return 1;
		}

		ret = write(request_pipefd[i], &first_char_end, sizeof(char));
		if (ret < 0) {
			perror("write");
			return 1;
		}
	}

	for (i = 0; i < NUM_WORKERS; i++) {
		/*
		 * Read the result for each worker.
		 */
		ret = read(result_pipefd[i], &len, sizeof(len));
		if (ret < 0) {
			perror("read");
			return 1;
		}

		if (len) {
			ret = read(result_pipefd[i], password, len);
			if (ret < 0) {
				perror("read");
				return 1;
			}

			password[len] = 0;

			printf("worker %d found %s\n", i, password);
		}
	}

	for (i = 0; i < NUM_WORKERS; i++) {
		ret = wait(NULL);
		if (ret < 0) {
			perror("wait");
			return 1;
		}
	}

	return 0;
}
