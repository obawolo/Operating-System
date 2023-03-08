// Name: Paul Kim 
// Netid: obawolo
// FALL 2020 CS 416 PROJECT 4
// cp.csrutgers.edu

// install the following before running the code:
// sudo apt-get install libreadline-dev


#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include <fcntl.h>     
#include <readline/readline.h> 
#include <readline/history.h> 
#include <signal.h>



/* PROTOTYPES */
int takeInput(char* str);
void printDir();
void startup_shell();
void sigintHandler(int sig_num);

void pipe_parse(char *input); 
void io_parse(char *input); 
void no_ops_execute(char **args); 
void input_redir(char **cmd, char *input); 
void output_redir(char **cmd, char *output, int append_flag);  
void pipe_handler(char **pipe_args, int pipe_count); 
void io_redir(char **cmd, char *input, char *output, int append_flag); 
int count_pipes(char *args); 
char check_op_order(char *input); 


/*GLOBALS*/
#define INPUT_SIZE 4096

int main(int argc, char *argv[]) {
    startup_shell();

    ssignal(SIGINT, sigintHandler);

    while(1) {

        // print current directory
		printDir();  
        
        char input[INPUT_SIZE];
        takeInput(input);

        if(strcmp(input, "\n") == 0) { 
            continue; 
        }
    
        //remove newline char and parse
        strtok(input, "\n");
        //printf("my command is: %s\n", input); 

        char* piece = strtok(input, ";");
		// executing one command at a time
    	while(piece != NULL) 
		{
            //printf("my command is: %s\n", piece);
            pipe_parse(piece); 
            piece = strtok(NULL, ";");
        }

    }
    printf("\n"); 
    return 0; 
}

// Function to print Current Directory. 
void printDir() 
{ 
	char cwd[1024]; 
	getcwd(cwd, sizeof(cwd)); 
	printf("\n%s ", cwd); 
} 

// Greets the user
void startup_shell() 
{  
    printf("#####################\n");
    printf("Welcome to PKSH Shell\n");
    printf("#####################\n");
	printf("#'exit' to terminate#\n");
	printf("#####################\n");
} 

/* Signal Handler for SIGINT */
void sigintHandler(int sig_num) 
{ 
    signal(SIGINT, sigintHandler); 
    printf("The shell cannot be terminated using ctrl+c \n"); 
	printf("But the running command/program was cancelled\n"); 
    fflush(stdout); 
} 

// Function to take input 
int takeInput(char* str) 
{ 
	char* buf; 

	buf = readline("$ "); 
	if (strlen(buf) != 0) { 
		add_history(buf); 
		strcpy(str, buf); 
		return 0; 
	} 


    else { 
		return 1; 
	} 

} 


void
pipe_parse(char *input) {
    char *arg; 
    char **args = malloc(INPUT_SIZE);  
    int count; 

    //parse first for pipes and pass to pipe_handler: if no pipes, pass input to io_parse 
    if(strchr(input, '|')) { 
        int pipe_number = count_pipes(input); 
        count = 0;  
        while((arg = strtok_r(input, "|", &input))) {
            args[count] = arg; 
            count ++; 
        }
        pipe_handler(args, pipe_number); 
        free(args); 
    } else {

        io_parse(input); 
    }
    return; 
}

void 
io_parse(char * input) {
    char *arg; 
    char **args = malloc(INPUT_SIZE);  
    int io_flag; 
    int append_flag = 0; 
    int count = 0; 
    int io_order_flag = 0; 
    
    if((strchr(input, '<')) || (strstr(input, ">"))) {
        //set append flag first 
        if(strstr(input, " >> ")) {
            append_flag = 1; 
        }
        //handle both input and output redir
        if((strchr(input, '<')) && (strstr(input, ">"))) {
            if(check_op_order(input) == '>') {
                io_order_flag = 1; 
            } else {
                io_order_flag = 0; 
            }
            while((arg = strtok_r(input, "<>", &input))) {
                args[count] = arg; 
                count ++; 
            }
            io_flag = 2; 
        //handles input redir
        } else if(strchr(input, '<')) {
            while((arg = strtok_r(input, "<", &input))) {
                args[count] = arg; 
                count ++; 
            }
            io_flag = 1; 
        //handles output redir
        } else if(strstr(input, ">")) {
            while((arg = strtok_r(input, ">", &input))) {
                args[count] = arg; 
                count ++; 
            }
            io_flag = 0;
        }
        //parse out io files and cmds
        count = 0; 
        char *cmd_arg; 
        char **cmd_args = malloc(INPUT_SIZE); 
        while((cmd_arg = strtok_r(args[0], " ", &args[0]))) {
            cmd_args[count] = cmd_arg; 
            count ++; 
        }
        char *io_file = strtok(args[1], " "); 
        //call appropriate function 
        if(io_flag == 2) {
            char *io_file2 = strtok(args[2], " "); 
            if(io_order_flag == 0) {
                io_redir(cmd_args, io_file, io_file2, append_flag); 
            } else {
                io_redir(cmd_args, io_file2, io_file, append_flag); 
            }
        } else if(io_flag == 1) {
            input_redir(cmd_args, io_file); 
        } else {
            output_redir(cmd_args, io_file, append_flag); 
        }
        free(cmd_arg); 
    //handles no ops 
    } else {
        count = 0; 
        while((arg = strtok_r(input, " ", &input))) {
            args[count] = arg; 
            count ++; 
        }       
        no_ops_execute(args);    
    }    
    free(args); 
    return; 
}

void 
no_ops_execute(char **args) {
    int exit_value;
    pid_t pid; 

	    char* ListOfOwnCmds[2];
    ListOfOwnCmds[0] = "exit"; 
    ListOfOwnCmds[1] = "cd";
    //printf("args: %s\n", args[0]);
    //printf("args: %s\n", args[1]);
    if (strcmp(args[0], ListOfOwnCmds[0]) == 0 ) {
        printf("\nGoodbye\n"); 
        exit(0); 
    } 
    else if(strcmp(args[0], ListOfOwnCmds[1]) == 0 ) {
        chdir(args[1]); 
        return; 
    }
	
    pid = fork(); 
    if (pid == -1) {
        perror("fork"); 
        return; 
    } else if(pid == 0) {
        execvp(args[0], args);
        perror("execvp"); 
        exit(1); 
    }
    wait(&exit_value); 
    return; 
}

void 
input_redir(char **cmd, char *input) {
    int exit_value;  
    int stdin = dup(0);  
    int file_fd = open(input, O_RDONLY); 
    if(file_fd == -1){
        perror("open"); 
        return; 
    }
    dup2(file_fd, 0); 

    //fork child process 
    pid_t pid; 
    pid = fork(); 
    if (pid == -1) {
        perror("fork"); 
        return; 
    } else if(pid == 0) {
        execvp(cmd[0], cmd); 
        perror("execvp"); 
        exit(1); 
    }
    wait(&exit_value); 
    dup2(stdin, 0); 
    close(stdin); 
    close(file_fd);
    return; 
}

void 
output_redir(char **cmd, char *output, int append_flag) {
    int exit_value;  
    int flags; 
    int stdout = dup(1);
    if(append_flag == 1) {
        flags = (O_RDWR | O_CREAT | O_APPEND); 
    } else {
        flags = (O_RDWR | O_CREAT); 
    }
    int file_fd = open(output, flags, 0644); 
    if(file_fd == -1){
        perror("open"); 
        return; 
    }
    dup2(file_fd, 1); 

    //fork child process 
    pid_t pid; 
    pid = fork(); 
    if (pid == -1) {
        perror("fork"); 
        return; 
    } else if(pid == 0) {
        execvp(cmd[0], cmd); 
        perror("execvp"); 
        exit(1); 
    }
    wait(&exit_value); 
    dup2(stdout, 1); 
    close(stdout); 
    close(file_fd);
    return; 
}

void 
pipe_handler(char **cmds, int pipe_count) {
    int exit_value;  
    int infd; 
    int pipefd[2]; 

    //loop through pipe commands 
    for (int i = 0; i <= pipe_count; i++) {
        //create new pipe for cmd i 
        if (pipe(pipefd) == -1) { 
            perror("pipe"); 
            exit(EXIT_FAILURE); 
        }
        //fork child to handle cmd 
        pid_t pid; 
        pid = fork(); 
        if (pid == -1) {
            perror("fork"); 
            return; 
        } else if(pid == 0) {
            //for all but first cmd, connect stdin with pipefd[0]
            if(i != 0) { 
                dup2(infd, 0); 
            }
            //for all but last cmd, connect stdout with pipefd[1] 
            if (i != pipe_count) { 
                dup2(pipefd[1], 1); 
            }
            io_parse(cmds[i]); 
            exit(1); 
        } else {
            //wait and store pipefd[0] for next iteration 
            wait(&exit_value); 
            infd = pipefd[0]; 
            close(pipefd[1]); 
        }
    }
}

void
io_redir(char **cmd, char *input, char *output, int append_flag) {
    int exit_value;  
    int flags; 

    //set flags
    if(append_flag == 1) {
        flags = (O_RDWR | O_CREAT | O_APPEND); 
    } else {
        flags = (O_RDWR | O_CREAT); 
    }
    
    //manipulate fds
    int stdout = dup(1);
    int stdin = dup(0); 
    int outfile_fd = open(output, flags, 0644); 
    if(outfile_fd == -1){
        perror("open out"); 
        return; 
    }
    int infile_fd = open(input, O_RDONLY); 
    if(infile_fd == -1){ 
        perror("open in"); 
        return; 
    }
    dup2(infile_fd, 0);
    dup2(outfile_fd, 1);

    //fork and execute cmd 
    pid_t pid; 
    pid = fork(); 
    if (pid == -1) {
        perror("fork"); 
        return; 


    } else if(pid == 0) {
        execvp(cmd[0], cmd); 
        perror("execvp"); 
        exit(1); 
    }
    wait(&exit_value); 

    //clean up 
    dup2(stdin, 0); 
    dup2(stdout, 1); 
    close(stdin); 
    close(stdout); 
    close(infile_fd);
    close(outfile_fd);
    return;
}

int 
count_pipes(char *args) {
    int count = 0; 
    for (int i=0; i < strlen(args); i++) {
        count += (args[i] == '|');
    }
    return count; 
}

char
check_op_order(char *input) {
    int first_op; 
    for(int i = 0; i < strlen(input); i ++){
        if(input[i] == '<' || input[i] == '>') {
            first_op = input[i]; 
            break; 
        }
    }
    return (char) first_op; 
}

