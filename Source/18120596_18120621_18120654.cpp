#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <time.h>

#define MAX_LENGTH 80 
#define SSH_TOK_DELIM " \t\r\n\a"
#define SSH_TOK_OPERATOR "|<>"
char* history = nullptr;
bool Is_Ampersand = false;



//Ham xoa khoang trang o dau va o cuoi. vd "  viet nam  " --> "viet nam"
void strip(char* str)
{
    //kiem tra chuoi rong
    if (!strlen(str))
        return;

    //xoa chuoi trang dau
    const char* d = str;
    while (*d == ' ')
        d++;
    strcpy(str, d);

    //xoa chuoi trang cuoi
    for (int i = strlen(str) - 1;; i--)
        if (str[i] != ' ')
        {
            str[i + 1] = '\0';
            break;
        }
}


//Ham nhap lenh cmd, xu ly history va kiem tra "&"
char* ssh_input()
{
    char* buffer = (char*)malloc(sizeof(char) * MAX_LENGTH);
    fgets(buffer, MAX_LENGTH, stdin);

    buffer[strlen(buffer) - 1] = '\0';
    strip(buffer);

    //luu history, neu buffer = !! or \n thi ko luu.
    if (strcmp(buffer, "!!") && *buffer)
    {
        if (history != nullptr)
            free(history);
        history = (char*)malloc(strlen(buffer) + 1);
        strcpy(history, buffer);
    }
    //kiem tra cuoi cau co dau &
    Is_Ampersand= false;

    if (buffer[strlen(buffer) - 1] == '&')
    {
        buffer[strlen(buffer) - 1] = '\0';
        Is_Ampersand = true;
    }
    strip(buffer);
    return buffer;
}

//ham chia chuoi dua theo "><|"  
char** split_operator(char* str)
{
    int position = 0;
    char** tokens = (char**)malloc(2 * sizeof(char*));
    char* token;

    token = strtok(str, SSH_TOK_OPERATOR);
    while (token)
    {
        strip(token);
        tokens[position++] = token;
        token = strtok(NULL, SSH_TOK_OPERATOR);
    }
    tokens[position] = NULL;
    return tokens;
}


//ham chia chuoi dua theo "\a\n\r..."
char** split_delim(char* str)
{
    int position = 0;
    char** tokens = (char**)malloc(MAX_LENGTH * sizeof(char*));
    char* token;

    token = strtok(str, SSH_TOK_DELIM);
    while (token)
    {
        tokens[position++] = token;
        token = strtok(NULL, SSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}


//ham phan loai xu ly
int classtify(char* str)
{
    if (strchr(str, '<')) //input
        return 1;
    else if (strchr(str, '>')) //output
        return 2;
    else if (strchr(str, '|')) //pipe
        return 3;
    return 0;
}


void Executing(char** flow)
{
    char** args = split_delim(flow[0]);
    
    pid_t pid, wpid;
    int status = 1;
    pid = fork();

    if (pid == 0)
    {
        //child process
        if (execvp(args[0], args) == -1)
            perror("execvp faild");
        exit(EXIT_FAILURE);
    }
    if (pid < 0)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (!Is_Ampersand)
    {
        // Parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        //wait(NULL);
        //waitpid(pid, NULL, 0);

    }
    
}

void Redirecting_Input(char** flow)
{
    char** args = split_delim(flow[0]);
    char** input_file = split_delim(flow[1]);

    pid_t pid, wpid;
    int status = 1;
    pid = fork();

    if (pid == 0)
    {
        //child process

        int fd = open(input_file[0], O_RDONLY);
        if (fd < 0)
        {
            perror("open failed");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDIN_FILENO) < 0)
        {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }

        close(fd);
        if (execvp(args[0], args) == -1)
            perror("execvp faild");
        exit(EXIT_FAILURE);
    }
    if (pid < 0)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (!Is_Ampersand)
    {
        // Parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        //wait(NULL);
        //waitpid(pid, NULL, 0);

    }
}

void Redirecting_Output(char** flow)
{
    char** args = split_delim(flow[0]);
    char** output_file = split_delim(flow[1]);
    
    pid_t pid, wpid;
    int status = 1;
    pid = fork();

    if (pid == 0)
    {
        //child process

        int fd = open(output_file[0], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd < 0)
        {
            perror("open failed");
            exit(EXIT_FAILURE);
        }
        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            perror("dup2 failed");
            exit(EXIT_FAILURE);
        }

        close(fd);
        if (execvp(args[0], args) == -1)
            perror("execvp faild");
        exit(EXIT_FAILURE);
    }
    if (pid < 0)
    {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    if (!Is_Ampersand)
    {
        // Parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        //wait(NULL);
        //waitpid(pid, NULL, 0);

    }

    
}

void Executing_Pipe(char** flow)
{
    //phan nay copy nha
    char** args1 = split_delim(flow[0]);
    char** args2 = split_delim(flow[1]);


    int pipefd[2];
    pid_t p1, p2;

    if (pipe(pipefd) < 0) {
        perror("pipe failed");
        exit(EXIT_FAILURE);
    }
    p1 = fork();


    if (p1 < 0) {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }

    if (p1 == 0) {
        // Child 1 executing.. 
        // It only needs to write at the write end 
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        if (execvp(args1[0], args1) < 0) {
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
    }
    else {
        // Parent executing 
        p2 = fork();

        if (p2 < 0) {
            perror("fork failed");
            exit(0);
        }

        // Child 2 executing.. 
        // It only needs to read at the read end 
        if (p2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            if (execvp(args2[0], args2) < 0) {
                perror("execvp failed");
                exit(EXIT_FAILURE);
            }
        }
        if(!Is_Ampersand) {
            // parent executing, waiting for two children 
            waitpid(p1, NULL, 0);
            waitpid(p2, NULL, 0);
        }
    }

    
}



void ssh_execute(char* str)
{
    int type = classtify(str);
    char** processing_flow = split_operator(str);

    if (type == 1)
        Redirecting_Input(processing_flow);
    else if (type == 2)
        Redirecting_Output(processing_flow);
    else if (type == 3)
        Executing_Pipe(processing_flow);
    else
        Executing(processing_flow);

    //free(processing_flow);
}


int main() {
    char* cmd;
    int should_run = 1; /* flag to determine when to exit program */

    while (should_run) {
        printf("ssh> ");
        fflush(stdout);
        cmd = ssh_input();

        if (!strlen(cmd)) //neu chuoi rong
            continue;
        if (!strcmp(cmd, "exit"))
            break;
        if (!strcmp(cmd, "!!"))
        {
            if (history == nullptr)
                printf("%s", "No commands in history.");
            else
                printf("%s", history);
                
            printf("%s", "\n");
            continue;
        }

        ssh_execute(cmd);

    }

  //  free(cmd);
    return 0;
}


