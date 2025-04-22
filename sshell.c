#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 16

int mySystem(const char *cmdLine){
    int i;
    char *args[MAX_ARGS + 1]; // 最多 16 个参数，加 1 作为结尾 NULL
    int err;
    int ec = 0;
    int exitCode = 0;
    char cmdCopy[512];
    
    pid_t pid = fork();  // 创建一个子进程

    if (pid < 0) {
        perror("fork failed");
        exit(errno);
    } else if (pid == 0) {  // 子进程
        i = 0;
        strncpy(cmdCopy, cmdLine, sizeof(cmdCopy));

        char *path_env = getenv("PATH");
        char fullpath[256];
    
        // If there is no command in PATH
        if (path_env == NULL){
            printf("no command\n");
        }
        
        // get all args
        char *token = strtok(cmdCopy, " ");
        char *cmdToken = token;
        args[0] = token;
        while (token != NULL && i < MAX_ARGS + 1) {
            token = strtok(NULL, " ");
            args[++i] = token;
            if(token == " "){
                i--;
            }
            if(i > 16){
                fprintf(stderr, "Error: too many process arguments\n");
                fflush(stderr);
                exit(255);
            }
        }
        args[++i] = "NULL";
        fflush(stdout);

        //
        char *Ktoken = strtok(path_env, ":");
        while (1) {
            Ktoken = strtok(NULL, ":");
            
            // Combine path with token
            snprintf(fullpath, sizeof(fullpath), "%s/%s", Ktoken, cmdToken);
            
            if (access(fullpath, X_OK) == 0){
                break;
            }
            else if(Ktoken != NULL){
                continue;
            }
            fprintf(stderr, "Error: command not found\n");
            fflush(stderr);
            exit(255);
        }
        
        if (execv(fullpath, args) == -1) {
            
            if (errno == ENOENT) {
                exit(errno);
            }
            
        // 如果 execvp 返回，则说明出错
        
        exit(errno);
        }
    } else {  
        int status;
        waitpid(pid, &status, 0);
        if(status == 255){
            return 255;
        }
        ec = WIFEXITED(status);
        if(ec){
            exitCode = WEXITSTATUS(status);
        }
    }
    return exitCode;
}

int main(void){
    char cmd[CMDLINE_MAX];
    char *eof;

    while (1) {
        char *nl;
        int retval;

        /* Print prompt */
        printf("sshell@ucd$ ");
        fflush(stdout);

        /* Get command line */
        eof = fgets(cmd, CMDLINE_MAX, stdin);
        if (!eof)
            /* Make EOF equate to exit */
            strncpy(cmd, "exit\n", CMDLINE_MAX);

        /* Print command line if stdin is not provided by terminal */
        if (!isatty(STDIN_FILENO)) {
            printf("%s", cmd);
            fflush(stdout);
        }

        /* Remove trailing newline from command line */
        nl = strchr(cmd, '\n');
        if (nl){
            *nl = '\0';
        }

        /* Builtin command */
        if (!strcmp(cmd, "exit")) {
            fprintf(stderr, "Bye...\n");
            fprintf(stderr, "+ completed 'exit' [0]\n");
            fflush(stderr);
            break;
        }

        /* Regular command */
        retval = mySystem(cmd);
        if(retval == 255){
            
        }else{
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
            fflush(stderr);
        }
    }
    return EXIT_SUCCESS;
}