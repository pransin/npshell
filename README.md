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

## Screenshots

### Simple shell commands
![Simple shell commands](./screenshots/simple_commands.png?raw=true)

### Piped commands
![Piped commands](./screenshots/pipe_commands.png?raw=true)

### Input and output redirection
![Input and output redirection](./screenshots/input_and_output_redirection.png?raw=true)

### History command
![History command](./screenshots/history_command.png?raw=true)

### Command aliasng
![Command aliasing](./screenshots/alias_and_unalias.png?raw=true)

### Changing directory and quitting shell
![Changing directory and quitting shell](./screenshots/cd_and_exit.png?raw=true)
