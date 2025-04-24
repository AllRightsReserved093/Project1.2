#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 16
#define MAX_PIPE 3

// Background job tracking
pid_t bg_pid = -1;
char bg_cmd[CMDLINE_MAX];

// Split Arguments
// Input command line, and a int to receive err output
// err = 0 if no error, err = 1 if too many arguments(skip printing complete message)
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

// Search for command
int searchCmd(char *cmdToken){
    char fullpath[256];
    char *path_env = getenv("PATH");
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
        return -1;
    }
    return 0;
}

// Redirect input to file
int inputRedirection(const char *cmdLine, char *cmdOutput, char *fileInput)
{
    const char *redir = strchr(cmdLine, '<');
    if (!redir)
    {
        strcpy(cmdOutput, cmdLine);
        fileInput[0] = '\0';
        return 0;
    }

    // Copy command part to send to input file
    size_t cmdLength = redir - cmdLine;
    strncpy(cmdOutput, cmdLine, cmdLength);
    cmdOutput[cmdLength] = '\0';

    // Reads file name
    redir++;
    while (*redir == ' ' || *redir == '\t')
        redir++;

    // Copies file name for input file
    strcpy(fileInput, redir);

    // Removes whitespace
    char *end = fileInput + strlen(fileInput) - 1;
    while (end > fileInput && (*end == ' ' || *end == '\n' || *end == '\t'))
    {
        *end-- = '\0';
    }

    // If file isn't specified
    if (strlen(fileInput) == 0)
    {
        fprintf(stderr, "Error: no input file\n");
        fflush(stderr);
        return -1;
    }

    return 1;
}

// Redirect output to file
int outputRedirection(const char *cmdLine, char *cmdOutput, char *fileOutput)
{
    const char *redir = strchr(cmdLine, '>');
    if (!redir)
    {
        strcpy(cmdOutput, cmdLine);
        fileOutput[0] = '\0';
        return 0;
    }

    // Copy command part to send to output file
    size_t cmdLength = redir - cmdLine;
    strncpy(cmdOutput, cmdLine, cmdLength);
    cmdOutput[cmdLength] = '\0';

    // Reads file name
    redir++;
    while (*redir == ' ' || *redir == '\t')
        redir++;

    // Copies file name for output file
    strcpy(fileOutput, redir);

    // Removes whitespace 
    char *end = fileOutput + strlen(fileOutput) - 1;
    while (end > fileOutput && (*end == ' ' || *end == '\n' || *end == '\t'))
    {
        *end-- = '\0';
    }

    // If file isn't specified
    if (strlen(fileOutput) == 0)
    {
        fprintf(stderr, "Error: no output file\n");
        fflush(stderr);
        return -1;
    }

    return 1; 
}

//implement syscall()
int mySystem(const char *cmdLine){
    int i;
    char *args[MAX_ARGS + 1]; // Up to 16 parameters + 1 for terminating NULL
    int err;
    int ec = 0;
    int exitCode = 0;
    char cmdCopy[CMDLINE_MAX];
    char cmdParsed[CMDLINE_MAX]; // includes cmd after removing both input and output
    char cmdParsed2[CMDLINE_MAX]; // includes only cmd and output command after removing input
    char fileOutput[CMDLINE_MAX];
    char fileInput[CMDLINE_MAX];

    int inputRedirect = inputRedirection(cmdLine, cmdParsed2, fileInput);
    int outputRedirect = outputRedirection(cmdParsed2, cmdParsed, fileOutput);

    // To handle printing error messages twice
    if (inputRedirect == -1 || outputRedirect == -1)
    {
        return 1;
    }

    strncpy(cmdCopy, cmdParsed, sizeof(cmdCopy));

    pid_t pid = fork();  // fork

    if (pid < 0) {
        perror("fork failed");
        exit(errno);
    
    } else if (pid == 0) {  // child
        i = 0;

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

        // Input redirection
        if (inputRedirect)
        {
            int fd = open(fileInput, O_RDONLY);
            if (fd < 0)
            {
                fprintf(stderr, "Error: cannot open input file\n");
                fflush(stderr);
                exit(errno);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // Output redirection
        if (outputRedirect)
        {
            int fd = open(fileOutput, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0)
            {
                fprintf(stderr, "Error: cannot open output file\n");
                fflush(stderr);
                exit(errno);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        // Search command in PATH
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
        // If execvp returns, error has occurred
        exit(errno);
        }
    
    // parent
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

int mislocatedRedirection(const char *cmd)
{
    int pipeNum;
    char **cmds = splitCmds(cmd, &pipeNum);
    const char *outputMisRedirect = strchr(cmd, '>');
    const char *pipe = strchr(cmd, '|');
    const char *backgroundSign = strchr(cmd, '&');

    if (backgroundSign && (outputMisRedirect || pipe))
    {
        return 3;
    }

    // input error handling
    for (int i = 1; i <= pipeNum; i++)
    {
        if (strchr(cmds[i], '<'))
        {
            return 1;
        }
    }

    // outut error handling
    if (outputMisRedirect && outputMisRedirect < pipe)
    {
        return 2;
    }

    return 0; 
}

// create pipeNum of pipes
int createPipes(int pipeNum, int (*fds)[2]) {
    // check if too many pipes
    if (pipeNum > MAX_PIPE) {
        fprintf(stderr, "超过最大管道数 %d\n", MAX_PIPE);
        return -1;
    }

    // create pipes
    for (int i = 0; i < pipeNum; ++i) {
        if (pipe(fds[i]) == -1) {
            perror("pipe");
            // close all pipes if error
            for (int j = 0; j < i; ++j) {
                close(fds[j][0]);
                close(fds[j][1]);
            }
            return -2;
        }
        // fds[i][0] is read，fds[i][1] is write
    }
    return 0;
}

// implement syscall() with pipe
int mySysPipe(const char *cmdLine, int *pipErr) {
    int pipeNum;
    char *cmdLineCopy = cmdLine;

    size_t len = strlen(cmdLine);
    if (len && cmdLine[len-1] == '\n'){
        cmdLineCopy[len-1] = '\0';
    }
    
    char **cmds = splitCmds(cmdLineCopy, &pipeNum);  // pipeNum = “|” 的个数 

    char ***args = malloc((pipeNum+1) * sizeof(char**));
    if (!args) { perror("malloc args"); return -1; }
    for (int i = 0; i <= pipeNum; i++) {
        int err = 0;
        args[i] = sArgs(cmds[i], &err);
        if (err) {
            return 255;
        } // too many arguments, return -1, no complete message
    }

    int (*fds)[2] = malloc(pipeNum * sizeof(int[2]));
    int err = createPipes(pipeNum, fds);

    for (int i = 0; i <= pipeNum; i++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); exit(1); }
        if (pid == 0) {
            if (i > 0)              dup2(fds[i-1][0], STDIN_FILENO);
            if (i < pipeNum)        dup2(fds[i][1], STDOUT_FILENO);

            for (int j = 0; j < pipeNum; j++) {
                close(fds[j][0]);
                close(fds[j][1]);
            }

            int cmdNotExist = searchCmd(cmds[i]);
            if(cmdNotExist){
                pipErr[i] = 255;
            }
            
            printf("%s\n", args[i][0]);
            execvp(args[i][0], args[i]);
            perror("execvp");
            _exit(1);
        }
    }

    for (int j = 0; j < pipeNum; j++) {
        close(fds[j][0]);
        close(fds[j][1]);
    }
    for (int i = 0; i <= pipeNum; i++) {
        wait(NULL);
    }

    return 0;
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

// get number of pipes.
int numOfPipes(const char *cmd) {
    size_t cnt = 0;
    if(strchr(cmd, '|') != NULL){
        for (size_t i = 0; cmd[i] != '\0'; i++) {
            if (cmd[i] == '|') {
                cnt++;
            }
        }
        cnt++;
    }
    return cnt;
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

            // If background job exists, cannot exit
            if (bg_pid > 0)
            {
                fprintf(stderr, "Error: active job still running\n");
                fflush(stderr);
                continue;
            }
            fprintf(stderr, "Bye...\n");
            fprintf(stderr, "+ completed 'exit' [0]\n");
            fflush(stderr);
            break;
        }

        // Background job
        int backgroundJob = 0;
        size_t len = strlen(cmd);
        char original_cmd[CMDLINE_MAX];
        strncpy(original_cmd, cmd, CMDLINE_MAX);
        if (len > 0 && cmd[len - 1] == '&')
        {
            backgroundJob = 1;

            cmd[--len] = '\0';
            while (len > 0 && isspace((unsigned char)cmd[len - 1]))
            {
                cmd[--len] = '\0';
            }
        }

        // Handles mislocated redirection error
        int misRedirError = mislocatedRedirection(cmd);
        if (misRedirError)
        {
            if (misRedirError == 1)
            {
                fprintf(stderr, "Error: mislocated input redirection\n");
            }
            else if (misRedirError == 2)
            {
                fprintf(stderr, "Error: mislocated output redirection\n");
            }
            else if (misRedirError == 3)
            {
                fprintf(stderr, "Error: mislocated background sign\n");
            }
            fflush(stderr);
            continue;
        }

        int pSkip = 1;
        int numPipes = numOfPipes(cmd);
        // Pipe or no pipe
        if(numPipes != 0){
            // With pipe
            int pipErr[numPipes + 1];
            mySysPipe(cmd, pipErr);

        }else{
            // Normal no pipe code

            //pwd
            if(!strcmp(cmd, "pwd")){
                printWorkingDirectory();
                pSkip = 0;
            }
            //cd
            if(!strncmp(cmd, "cd", 2)){
                pSkip = changeDirectory(cmd);
            }
            /* Regular command */
            // skip systemCall if pSkip = 1
            if(pSkip == 1){
                if (backgroundJob)
                {
                    pid_t pid = fork();
                    if (pid == 0) // child
                    {
                        exit(mySystem(cmd));
                    }
                    else // parent
                    {
                        bg_pid = pid; // bg_pid > 0 only if backgroundJob parent
                        strncpy(bg_cmd, original_cmd, sizeof(bg_cmd));
                        continue;
                    }
                }
                else
                {
                    retval = mySystem(cmd);
                }
            }
        }
        
        if(numPipes != 0){

        }

        if(retval == 255 || pSkip == -1){
            // Skip output complete message if retval = 255
        }

        // Check for background job completion then print complete message
        if (bg_pid > 0)
        {
            int status;
            pid_t bg_status = waitpid(bg_pid, &status, WNOHANG);
            if (bg_status > 0)
            {
                int exit_code;
                if (WIFEXITED(status))
                {
                    exit_code = WEXITSTATUS(status);
                }
                else
                {
                    exit_code = 1;
                }

                fprintf(stderr, "+ completed '%s' [%d]\n", bg_cmd, exit_code);
                bg_pid = -1;
                fflush(stderr);
            }
        }
        
        if (retval != 255 && pSkip != -1)
        {
            fprintf(stderr, "+ completed '%s' [%d]\n", original_cmd, retval);
            fflush(stderr);
        }
    }
}