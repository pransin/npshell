#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h> //ssize_t

#define DEFAULT_MALLOC_SIZE 4

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

Command *create_cmd()
{
    Command *cmd = (Command *)malloc(sizeof(Command));
    if (!cmd)
    {
        perror("malloc");
        return NULL;
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
            if (token[i + 1] == '>')
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
    if (result == -1)
    {
        free(commands);
        commands = NULL;
        perror("Unable to read input");
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
        // skip continuous delims
        while (**input != '\0' && **input == '|')
        {
            **input = '\0';
            (*out_count)++;
            (*input)++;
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
    pipeline->cnt = 0;
    pipeline->cmd_list = malloc(sizeof(Command));
    pipeline->last = pipeline->cmd_list;
    if (!pipeline)
    {
        perror("malloc");
        return NULL;
    }

    populate_pipeline(input, pipeline);
    return pipeline;
}

int main()
{
    // while (1)
    // {
    printf("=> ");
    char *input = read_cmds();
    Pipeline *pipeline = create_pipeline(input);
    // }
}