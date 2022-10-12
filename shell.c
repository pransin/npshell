#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h> //ssize_t
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>

#define DEFAULT_MALLOC_SIZE 4
#define GREEN "\033[0;32m"
#define RESET "\033[0;37m"

enum ParseMode
{
    COMMAND_BEGIN,
    COMMAND_READ,
    ARGS_BEGIN,
    ARGS_READ,
    INPUT_BEGIN,
    OUTPUT_BEGIN,
    INPUT_READ,
    OUTPUT_READ
};

typedef struct Command
{
    int argc;
    char **argv;
    bool input_redirect;
    bool output_redirect;
    bool output_append;
    int out_count;
    char *input_file;
    char *output_file;
    struct Command *next;
} Command;

typedef struct Pipeline
{
    int cnt;
    Command *cmd_list; // First node is dummy
    Command *last;

} Pipeline;

int cnt_of_pipes;

void error_exit(char *msg){
    perror(msg);
    exit(EXIT_FAILURE);
}

void change_dir(char **args){
    if(args[1] == NULL){
        perror("Expected argument to \"cd\"\n");
    }
    else if(chdir(args[1]) != 0){
        perror("chdir");
    }
}

void free_pipeline(Pipeline *pipeline){
    Command *tmp;
    if(!pipeline){
        return;
    }
    for(tmp=pipeline->cmd_list->next;tmp!=NULL;tmp=tmp->next){
        free(tmp);
    }
    pipeline->last = pipeline->cmd_list;
    pipeline->last->next = NULL;
    pipeline->cnt = 0;
}

void close_all_pipes(int pipe_fd[][2], int count){
    for(int i=0;i<count;i++){
        close(pipe_fd[i][0]);
        close(pipe_fd[i][1]);
    }
}

bool is_valid_filename(char *filename){
    int len = strlen(filename);
    for(int i=0;i<len;i++){
        if(isdigit(filename[i])){
            return false;
        }
    }
    return true;
}

void execute(Pipeline *pipeline, bool *exit){
    if(!(pipeline->cnt)){
        return;
    }
    if(!strcmp(pipeline->cmd_list->next->argv[0], "cd")){
        change_dir(pipeline->cmd_list->next->argv);
        return;
    }
    if(!strcmp(pipeline->cmd_list->next->argv[0], "exit")){
        *exit = true;
        return;
    }
    int count = pipeline->cnt;
    int pipe_fd[cnt_of_pipes][2];
    for(int i=0;i<cnt_of_pipes;i++){
        if(pipe(pipe_fd[i]) == -1){
            error_exit("pipe");
        }
    }
    Command* cmd = pipeline->cmd_list->next;
    int pipe_index = -1;
    for(int i=0;i<count;i++){
        if(cmd->out_count > 0){
            pipe_index++;
        }
        pid_t ret = fork();
        if(ret == -1){
            error_exit("fork");
        }
        if(ret == 0){
            if(cmd->out_count > 0){
                if(pipe_index >= 1){
                    if(dup2(pipe_fd[pipe_index-1][0], STDIN_FILENO) == -1){
                        error_exit("dup2");
                    }
                    if(dup2(pipe_fd[pipe_index][1], STDOUT_FILENO) == -1){
                        error_exit("dup2");
                    }
                }
                else if(pipe_index == 0){
                    if(dup2(pipe_fd[pipe_index][1], STDOUT_FILENO) == -1){
                        error_exit("dup2");
                    }
                }
            }
            else{
                if(pipe_index >= 0){
                    if(dup2(pipe_fd[pipe_index][0], STDIN_FILENO) == -1){
                        error_exit("dup2");
                    }
                }
            }
            if(cmd->input_redirect == true && is_valid_filename(cmd->input_file)){
                int input_fd = open(cmd->input_file, O_RDONLY);
                if(input_fd == -1){
                    error_exit("open");
                }
                if(dup2(input_fd, STDIN_FILENO) == -1){
                    error_exit("dup2");
                }
                if(close(input_fd) == -1){
                    error_exit("close");
                }
            }
            if(cmd->output_redirect == true && is_valid_filename(cmd->output_file)){
                int output_fd = open(cmd->output_file, O_TRUNC | O_WRONLY | O_CREAT, 0777);
                if(output_fd == -1){
                    error_exit("open");
                }
                if(dup2(output_fd, STDOUT_FILENO) == -1){
                    error_exit("dup2");
                }
                if(close(output_fd) == -1){
                    error_exit("close");
                }
            }
            if(cmd->output_append == true && is_valid_filename(cmd->output_file)){
                int output_fd = open(cmd->output_file, O_APPEND | O_WRONLY | O_CREAT, 0777);
                if(output_fd == -1){
                    error_exit("open");
                    return;
                }
                if(dup2(output_fd, STDOUT_FILENO) == -1){
                    error_exit("dup2");
                }
                if(close(output_fd) == -1){
                    error_exit("close");
                }
            }
            close_all_pipes(pipe_fd, cnt_of_pipes);
            if(execvp(cmd->argv[0], cmd->argv) == -1){
                error_exit("execvp");
            }
        }
        else{
            if(pipe_index > 0){
                close(pipe_fd[pipe_index-1][0]);
            }
            if(pipe_index < cnt_of_pipes){
                close(pipe_fd[pipe_index][1]);
            }
            int status;
            if(wait(&status) == -1){
                error_exit("wait");
            }
            printf("Process[%d] pid: %d status: %d\n", i, ret, status);
        }
        cmd = cmd->next;
    }
    close_all_pipes(pipe_fd, cnt_of_pipes);
}

Command *create_cmd()
{
    Command *cmd = (Command *)malloc(sizeof(Command));
    if (!cmd)
    {
        error_exit("malloc");
    }
    cmd->argc = 0;
    cmd->argv = NULL;
    cmd->input_redirect = false;
    cmd->output_redirect = false;
    cmd->output_append = false;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->out_count = 0;
    cmd->next = NULL;
    return cmd;
}

// TODO: test empty command name case
// realloc argv;
void parse_cmd(Command *cmd, char *token)
{
    int argc = 0;
    int len = strlen(token);
    cmd->argv = malloc(DEFAULT_MALLOC_SIZE * sizeof(char *));
    // cmd->argv[0] = token;
    enum ParseMode mode = COMMAND_BEGIN;
    for (int i = 0; i < len; i++)
    {
        switch (token[i])
        {
        case '>':
            if (i+1<len && token[i + 1] == '>')
            {
                cmd->output_append = true;
                token[i] = token[i + 1] = '\0';
                i++;
            }
            else
            {
                cmd->output_redirect = true;
                token[i] = '\0';
            }
            mode = OUTPUT_BEGIN;
            break;

        case '<':
            cmd->input_redirect = true;
            mode = INPUT_BEGIN;
            token[i] = '\0';
            break;
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
        case '\v':
        case '\0':
            switch (mode)
            {
            case INPUT_READ:
            case OUTPUT_READ:
            case COMMAND_READ:
            case ARGS_READ:
                mode = ARGS_BEGIN;

            default:
                token[i] = '\0';
                break;
            }
            break;
        default:
            switch (mode)
            {
            case INPUT_BEGIN:
                cmd->input_file = (token + i);
                mode = INPUT_READ;
                break;

            case OUTPUT_BEGIN:
                cmd->output_file = (token + i);
                mode = OUTPUT_READ;
                break;
            case ARGS_BEGIN:
                cmd->argv[argc++] = (token + i);
                mode = ARGS_READ;
                break;
            case COMMAND_BEGIN:
                cmd->argv[0] = (token + i);
                mode = COMMAND_READ;
                argc++;
            default:
                break;
            }
        }
    }

    cmd->argc = argc;
}

char *read_cmds()
{
    char *commands = NULL;
    size_t size = 0;
    ssize_t result = getline(&commands, &size, stdin);
    if(result == 1){
        return NULL;
    }
    if (result == -1)
    {
        free(commands);
        commands = NULL;
        error_exit("Unable to read input");
    }
    return commands;
}

char *tokeniser(char **input, int *out_count)
{
    *out_count = 0;
    if (*input == NULL || **input == '\0')
    {
        return NULL;
    }
    char *res = *input;
    // Find delim
    while (**input != '|' && **input != ',' && **input != '\0')
    {
        (*input)++;
    }

    if (**input == ',')
    {
        **input = '\0';
        (*input)++;
    }
    else
    {
        bool flag = 0;
        // skip continuous delims
        while (**input != '\0' && **input == '|')
        {
            **input = '\0';
            (*out_count)++;
            (*input)++;
            flag = 1;
        }

        if(flag){
            cnt_of_pipes++;
        }

        if (*out_count > 2)
        {
            *out_count == 2;
        }
    }

    return res;
}

void insert_cmd(Pipeline *pipeline, Command *cmd)
{
    pipeline->last->next = cmd;
    pipeline->last = pipeline->last->next;
    pipeline->last->next = NULL;
    pipeline->cnt++;
}

void populate_pipeline(char *input, Pipeline *pipeline)
{
    int delim_count = 0;
    char *token = tokeniser(&input, &delim_count);
    while (token != NULL)
    {
        // Command struct initialization
        Command *cmd = create_cmd();
        cmd->out_count = delim_count;
        parse_cmd(cmd, token);
        insert_cmd(pipeline, cmd);
        token = tokeniser(&input, &delim_count);
    }
}
Pipeline *create_pipeline(char *input)
{
    Pipeline *pipeline = malloc(sizeof(Pipeline));
    if (!pipeline)
    {
        error_exit("malloc");
    }
    pipeline->cnt = 0;
    pipeline->cmd_list = malloc(sizeof(Command));
    if (!(pipeline->cmd_list))
    {
        error_exit("malloc");
    }
    pipeline->last = pipeline->cmd_list;
    pipeline->last->next = NULL;
    populate_pipeline(input, pipeline);
    return pipeline;
}

int main()
{
    bool exit = false;
    while (!exit)
    {
        printf(GREEN"=> "RESET);
        char *input = read_cmds();
        cnt_of_pipes = 0;
        Pipeline *pipeline = create_pipeline(input);
        execute(pipeline, &exit);
        free_pipeline(pipeline);
        printf("\n");
    }
}