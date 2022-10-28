# Shell

## Run
```
    gcc -o shell shell.c
    
    ./shell
```

## Features implemented

- Simple shell commands
```
    ls
    
    pwd
    
    clear
```

- Pipelining of commands ( Works with files of very large sizes as well, even for those files which exceed pipe buffer limit)
```
    ls -l | grep r | wc
    
    ls -l || head, tail | wc
    
    cat shell.c ||| wc -c, wc -l, sort > out
    
```

- Input output redirection
```
    ls -l > out
    
    wc < out
    
    ls -a >> out
```

- History command
```
    history
```

- Command aliasing
```
    alias L = ls -l
    
    L
    
    unalias L
    
    alias C = cat shell.c | head || sort, tail -n 5 ||| wc -c, wc -l, wc -w
    
    C
    
    unalias C
```

- Changing directory
```
    cd Desktop
    
    cd ..
```

- Exiting shell
```
    exit
```

## Assumptions

For simplicity, following assumptions have been made

- Maximum number of aliases allowed at a time is 517
- Maximum command size is 1024 characters
- Maximum length of alias variable is 128 characters
- For using alias command, each identifier in the command must be separated by a single space. Thus `alias L = ls -a` is valid but `alias L= ls -a` is not valid
- For using grep command, the pattern must not be enclosed within "". Thus `ls -l | grep r` is valid but `ls -l | grep "r"` isn't valid
- In commands separated by `||` and `|||`, only the last command is allowed to have `|`, `||` or `|||`. The result of the previous commands (previous 2 commands in case of `|||` and previous command in case of `||`) is shown on STDOUT

## Design Features

### Parsing of input and making the pipeline

- The input is parsed based on the delimiters `,`, `|`, `||` and `|||`
- Each of the tokens obtained in previous step is structured into a command. The information like number of arguments, argument list, input/output redirection or not, whether it is first command after a `||`, etc is extracted from the input and structured into a command
- The above commands are then inserted into a linked list (pipeline)

### Execution of commands

- Firstly, it is checked whether the command is a builtin command or any other custom command (alias, unalias or history command, made as a part of extra features). If it is so, it is executed separately
- Pipes are created between successive commands
- A child process is created for each command in the pipeline. In the child, the current command reads its input from the pipe connecting it and the previous command (It reads from STDIN if it's the first command in the pipeline). It writes its output to the pipe connecting it and the next command (It writes to STDOUT if it's the last command in the pipeline)
- For commands which are immediately after a `||` or a `|||`, the input from the pipe is first read into a temporary buffer. A temporary pipe is created and data from the temporary buffer is written into it inside a child process (this child process is created for this purpose only). The data from temporary buffer is also written to the next pipe (also done inside a separate child process). Separate child processes have been created to do the above because otherwise, the whole pipeline may get blocked if the pipe buffer gets full
- If the current command is an input/output redirection operation, the output of the current command is redirected to the file specified in the command
- The command is executed using the `execvp` library call

### Additional Features

#### History command

- A separate linked list is maintained which stores the input entered by the user
- When the user types `history`, the linked list is traversed and the commands inputted by the user before are printed on the terminal with latest command at top

#### Command Aliasing

- A hash table has been implemented to store the alias name and the command associated with it
- When something like `alias L = ls -a` is executed, the hash table stores the alias name (`L`) and the command associated with it (`ls -a`)
- Whenever the alias (`L` in above example) is entered, the hash table is searched and the command corresponding to the alias (`ls -a` in above example) is executed
- Using the `unalias L` command, the entry corresponding to `L` in hash table is removed

## Screenshots

### Simple shell commands
![Simple shell commands](./screenshots/simple_commands.png?raw=true)

### Piped commands
![Piped commands](./screenshots/pipe_commands.png?raw=true)

### Input and output redirection
![Input and output redirection](./screenshots/input_and_output_redirection.png?raw=true)

### History command
![History command](./screenshots/history_command.png?raw=true)

### Command aliasing
![Command aliasing](./screenshots/alias_and_unalias.png?raw=true)

### Changing directory and quitting shell
![Changing directory and quitting shell](./screenshots/cd_and_exit.png?raw=true)
