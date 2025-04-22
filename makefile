cc = gcc
cflag = -Wall -Wextra -Werror

sshell: sshell.c
	$(CC) $(CFLAGS) -o sshell sshell.c
.PHONY: clean
clean:
	rm -f sshell *.o
