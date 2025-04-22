#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 16
#define MAX_PIPE 3

//implement syscall()
int mySystem(const char *cmdLine){
    int i;
    char *args[MAX_ARGS + 1]; // 最多 16 个参数，加 1 作为结尾 NULL
    int err;
    int ec = 0;
    int exitCode = 0;
    char cmdCopy[512];
    
    pid_t pid = fork();  // fork

    if (pid < 0) {
        perror("fork failed");
        exit(errno);
    
    } else if (pid == 0) {  // child
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

        // Search command in PASS
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

//implement syscall() with pipe
int mySysPipe(const char *cmd){
    
}

char **sArgs(const char *cmdline, int *err) {
    char *buf = strdup(cmdline);
    char **args = malloc((MAX_ARGS + 2) * sizeof *args);

    int i = 0;
    char *tok = strtok(buf, " ");
    while (tok) {
        if (i >= MAX_ARGS) {
            fprintf(stderr, "Error: too many arguments (max %d)\n", MAX_ARGS);
            *err = 1;
        }
        args[i++] = tok;
        tok = strtok(NULL, " ");
    }
    args[i] = NULL;
    *err = 0;

    return args;
}

// Split commands
// Input full command line, and a int to store # of pipes
char **splitCmds(const char *cmd, int *pipeNum) {
    char *buf = strdup(cmd);
    if (!buf) {
        perror("strdup");
        exit(255);
    }

    char **cmds = malloc((MAX_PIPE + 2) * sizeof *cmds);
    if (!cmds) {
        perror("malloc");
        free(buf);
        exit(255);
    }

    // split cmd
    int count = 0;
    char *saveptr = NULL;
    char *seg = strtok_r(buf, "|", &saveptr);
    while (seg) {
        if (count >= MAX_PIPE + 1) {
            fprintf(stderr, "Error: too many pipes (max %d)\n", MAX_PIPE);
            fflush(stderr);
            free(buf);
            free(cmds);
            exit(255);
        }

        // remove space
        while (*seg == ' ' || *seg == '\t') seg++;
        char *end = seg + strlen(seg) - 1;
        while (end > seg && (*end == ' ' || *end == '\t' || *end == '\n'))
            *end-- = '\0';

        cmds[count++] = seg;
        seg = strtok_r(NULL, "|", &saveptr);
    }

    // append NULL at the end
    cmds[count] = NULL;
    cmds[MAX_PIPE+1] = buf;

    *pipeNum = (count > 0 ? count - 1 : 0);
    return cmds;
}

//implement pwd(Print Working Directory)
void printWorkingDirectory(){
    char* cwd = getcwd(NULL, 0);
    if (!cwd) {
        // print the error stderr
        perror("getcwd failed\n");
        return;
    }
    
    printf("%s\n", cwd);
    free(cwd);
    fflush(stdout);
    return;
}

//implement cd(Change Directory)
int changeDirectory(const char *cmd){
    int i;

    char **argv = sArgs(cmd, &i);
    if(i == 1){
        // Too many arguments
        free(argv[MAX_ARGS+1]);
        free(argv);
        return -1;
    }else if(chdir(argv[1]) != 0){
        // chdir() error
        fprintf(stderr, "Error: cannot cd into directory\n");
        fflush(stderr);
        free(argv[MAX_ARGS+1]);
        free(argv);
        return -2;
    }

    free(argv[MAX_ARGS+1]);
    free(argv);

    return 0;
}

int hasPipe(const char *s) {
    return strchr(s, '|') != NULL;
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



        // Pipe or no pipe
        if(hasPipe(cmd)){
            // With pipe
            retval = mySysPipe(cmd);

        }else{
            // Normal no pipe code

            int pSkip = 1;

            //pwd
            if(!strcmp(cmd, "pwd")){
                printWorkingDirectory();
                int pSkip = 0;
            }
    
            //pwd
            if(!strncmp(cmd, "cd", 2)){
                pSkip = changeDirectory(cmd);
            }
    
            /* Regular command */
            // skip systemCall if pSkip = 1
            if(pSkip == 1){
                retval = mySystem(cmd);
            }
        }


        if(retval == 255 || pSkip == -1){
            // Skip output complete message if retval = 255
        }else{
            fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
            fflush(stderr);
        }
    }
    return EXIT_SUCCESS;
}