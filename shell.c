#define _XOPEN_SOURCE 700 // Enables vs code to see sigjmp_buf, not sure why
#define BUFFER_SIZE 1024
#define MAX_CMD_SIZE 1024
#define DEFAULT_MALLOC_SIZE 4
#define GREEN "\033[0;32m"
#define BOLD_GREEN "\033[1;32m"
#define RESET "\033[0;37m"
#define PATH_MAX 4096

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
#include <signal.h>
#include <setjmp.h>

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

typedef struct HistoryNode
{
    char input[MAX_CMD_SIZE];
    struct HistoryNode *next;
    struct HistoryNode *prev;
} HistoryNode;

typedef struct History
{
    HistoryNode *head;
    HistoryNode *tail;
} History;

History *ptr = NULL;
pid_t gpid; // To identify if the process is parent or child
static sigjmp_buf senv;
void int_handler(int signo)
{
    if (gpid == getpid()) // For parent
    {
        siglongjmp(senv, 1);
    }
    else // For child
    {
        exit(130); // usual exit code for SIGINT
    }
}

void unignore_int()
{
    signal(SIGINT, int_handler);
}

void ignore_int()
{
    signal(SIGINT, SIG_IGN);
}

void error_exit(char *msg)
{
    perror(msg);
    exit(EXIT_FAILURE);
}

void insert_input_in_history(char *input)
{
    if (input == NULL || *input == '\0' || *input == '\n' || strlen(input) == 0)
    {
        return;
    }
    HistoryNode *hn = (HistoryNode *)malloc(sizeof(HistoryNode));
    if (hn == NULL)
    {
        error_exit("malloc");
    }
    strcpy(hn->input, input);
    if (ptr->head == NULL)
    {
        ptr->head = hn;
        hn->prev = NULL;
        hn->next = NULL;
        ptr->tail = hn;
    }
    else
    {
        hn->next = ptr->head;
        hn->prev = NULL;
        ptr->head->prev = hn;
        ptr->head = hn;
    }
}

void pop()
{
    if (ptr->head == NULL)
    {
        return;
    }
    if (ptr->head->next == NULL)
    {
        free(ptr->head);
        return;
    }
    HistoryNode *hn = ptr->head;
    ptr->head = hn->next;
    ptr->head->prev = NULL;
    hn->prev = NULL;
    hn->next = NULL;
    free(hn);
    hn = NULL;
}

void change_dir(char **args)

{
    if (args[1] == NULL)

    {
        perror("Expected argument to \"cd\"\n");
    }
    else if (chdir(args[1]) != 0)

    {
        perror("chdir");
    }
}

void free_pipeline(Pipeline *pipeline)

{
    Command *tmp;
    if (!pipeline)
    {
        return;
    }
    for (tmp = pipeline->cmd_list->next; tmp != NULL; tmp = tmp->next)
    {
        free(tmp->input_file);
        free(tmp->output_file);
        free(tmp->argv);
        free(tmp);
    }
    pipeline->last = pipeline->cmd_list;
    pipeline->last->next = NULL;
    pipeline->cnt = 0;
}

void close_all_pipes(int pipe_fd[][2], int count)
{
    for (int i = 0; i < count; i++)
    {
        close(pipe_fd[i][0]);
        close(pipe_fd[i][1]);
    }
}

bool is_valid_filename(char *filename)

{
    int len = strlen(filename);
    for (int i = 0; i < len; i++)
    {
        if (isalpha(filename[i]))
        {
            return true;
        }
    }
    return false;
}

void execute(Pipeline *pipeline)
{
    if (!(pipeline->cnt))
    {
        return;
    }
    if (!strcmp(pipeline->cmd_list->next->argv[0], "cd"))
    {
        change_dir(pipeline->cmd_list->next->argv);
        return;
    }
    if (!strcmp(pipeline->cmd_list->next->argv[0], "history"))
    {
        pop();
        HistoryNode *hn = ptr->head;
        while (hn != NULL)
        {
            printf("%s", hn->input);
            hn = hn->next;
        }
        return;
    }
    if (!strcmp(pipeline->cmd_list->next->argv[0], "exit"))
    {
        exit(EXIT_SUCCESS);
    }
    int count = pipeline->cnt;
    int pipe_fd[count - 1][2];
    for (int i = 0; i < count - 1; i++)
    {
        if (pipe(pipe_fd[i]) == -1)
        {
            error_exit("pipe");
        }
    }
    Command *cmd = pipeline->cmd_list->next;
    int pipe_index = -1;
    for (int i = 0; i < count; i++)
    {
        if (cmd->out_count > 0)
        {
            pipe_index++;
        }
        pid_t ret = fork();
        if (ret == -1)
        {
            error_exit("fork");
        }
        if (ret == 0)
        {
            signal(SIGINT, SIG_DFL);
            if (i < count - 1)
            {
                if (cmd->out_count == 0 && count != 1)
                {
                    int buffersize = BUFFER_SIZE;
                    char buffer[BUFFER_SIZE];
                    int position = 0;
                    int bytes_read = 0;
                    char *data = (char *)malloc(sizeof(char) * buffersize);
                    if (data == NULL)
                    {
                        error_exit("malloc");
                    }
                    while ((bytes_read = read(pipe_fd[i - 1][0], &buffer, BUFFER_SIZE)) != 0)
                    {
                        strcpy(data + position, buffer);
                        if (bytes_read == BUFFER_SIZE)
                        {
                            buffersize += BUFFER_SIZE;
                            data = (char *)realloc(data, sizeof(char) * buffersize);
                            if (data == NULL)
                            {
                                error_exit("realloc");
                            }
                        }
                        position += bytes_read;
                    }
                    data[position] = '\0';
                    int temp_fd[2];
                    if (pipe(temp_fd) == -1)
                    {
                        error_exit("pipe");
                    }
                    if (write(temp_fd[1], data, position) == -1)
                    {
                        error_exit("write");
                    }
                    if (write(pipe_fd[i][1], data, position) == -1)
                    {
                        error_exit("write");
                    }
                    if (close(temp_fd[1]) == -1)
                    {
                        error_exit("close");
                    }
                    if (dup2(temp_fd[0], STDIN_FILENO) == -1)
                    {
                        error_exit("dup2");
                    }
                }
                if (cmd->out_count > 0)
                {
                    if (dup2(pipe_fd[i][1], STDOUT_FILENO) == -1)
                    {
                        error_exit("dup2");
                    }
                }
            }
            if (i != 0)

            {
                if (i == count - 1 || cmd->out_count > 0)

                {
                    if (dup2(pipe_fd[i - 1][0], STDIN_FILENO) == -1)

                    {
                        error_exit("dup2");
                    }
                }
            }
            if (cmd->input_redirect == true && is_valid_filename(cmd->input_file))

            {
                int input_fd = open(cmd->input_file, O_RDONLY);
                if (input_fd == -1)

                {
                    error_exit("open");
                }
                if (dup2(input_fd, STDIN_FILENO) == -1)

                {
                    error_exit("dup2");
                }
                if (close(input_fd) == -1)

                {
                    error_exit("close");
                }
            }
            if (cmd->output_redirect == true && is_valid_filename(cmd->output_file))

            {
                int output_fd = open(cmd->output_file, O_TRUNC | O_WRONLY | O_CREAT, 0777);
                if (output_fd == -1)

                {
                    error_exit("open");
                }
                if (dup2(output_fd, STDOUT_FILENO) == -1)

                {
                    error_exit("dup2");
                }
                if (close(output_fd) == -1)

                {
                    error_exit("close");
                }
            }
            if (cmd->output_append == true && is_valid_filename(cmd->output_file))

            {
                int output_fd = open(cmd->output_file, O_APPEND | O_WRONLY | O_CREAT, 0777);
                if (output_fd == -1)

                {
                    error_exit("open");
                }
                if (dup2(output_fd, STDOUT_FILENO) == -1)

                {
                    error_exit("dup2");
                }
                if (close(output_fd) == -1)

                {
                    error_exit("close");
                }
            }
            close_all_pipes(pipe_fd, count - 1);
            if (execvp(cmd->argv[0], cmd->argv) == -1)
            {
                error_exit("execvp");
            }
        }
        else
        {
            if (i > 0)
            {
                close(pipe_fd[i - 1][0]);
            }
            if (i < count - 1)

            {
                close(pipe_fd[i][1]);
            }
            int status;
            if (wait(&status) == -1)

            {
                error_exit("wait");
            }
            printf("-------- Process[%d] pid: %d status: %d --------\n", i, ret, status);
        }
        cmd = cmd->next;
    }
    close_all_pipes(pipe_fd, count - 1);
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
    if (!cmd->argv)
        error_exit("malloc");
    int argmax = DEFAULT_MALLOC_SIZE;
    enum ParseMode mode = COMMAND_BEGIN;
    for (int i = 0; i < len; i++)
    {
        switch (token[i])
        {
        case '>':
            if (i + 1 < len && token[i + 1] == '>')
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
                if (argc == argmax)
                {
                    argmax = (argmax * 3) / 2;
                    cmd->argv = realloc(cmd->argv, argmax * sizeof(char *));
                    if (!cmd->argv)
                        error_exit("realloc");
                }

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
    cmd->argv[argc] = NULL;
}

char *read_cmds()
{
    char *commands = NULL;
    size_t size = 0;
    ssize_t result = getline(&commands, &size, stdin);
    if (result == -1)
    {
        free(commands);
        commands = NULL;
        error_exit("read_cmd");
    }
    return commands;
}

char *tokeniser(char **input, int *out_count)
{
    *out_count = 0;
    if (*input == NULL)
        return NULL;

    // Remove leading whitespaces
    while (**input != '\0' && isspace(**input))
        (*input)++;

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
        // skip continuous delims
        while (**input != '\0' && **input == '|')
        {
            **input = '\0';
            (*out_count)++;
            (*input)++;
        }
        if (*out_count > 3)
        {
            *out_count = 3;
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
    char *token = tokeniser(&input, &delim_count); // Split on pipe and comma
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
    ptr = (History *)malloc(sizeof(History));
    if (ptr == NULL)
    {
        error_exit("malloc");
    }

    gpid = getpid();
    ptr->head = NULL;
    ptr->tail = NULL;
    char cwd[PATH_MAX];
    ignore_int();
    while (1)
    {
        if (getcwd(cwd, PATH_MAX) != NULL)
            printf("%s %s", BOLD_GREEN, cwd);
        printf(GREEN ":=> " RESET);
        fflush(stdout);
        char *input = read_cmds();
        insert_input_in_history(input);
        Pipeline *pipeline = NULL;

        unignore_int();
        if (sigsetjmp(senv, 1) == 0)
            pipeline = create_pipeline(input);
        ignore_int();

        execute(pipeline);
        free_pipeline(pipeline);
        free(input);
    }
}
