# system-shell
A modern, colorful terminal shell built from scratch in C with process control, job management, and an ncurses-powered UI.

To run:
1) gcc smallsh-with-ui.c race.c -o smallsh -lncurses -lpthread
2) ./smallsh
3) to exit, type exit in command line

It is capable of:
-forking a child process for background/foreground commands
-handling commands with execvp()
-handling built-it commands: cd, status, exit
-printing PID when detects $$
-treating the line as a comment line when detects # at the beginning of the input
-handling the signals, and printing what signal killed the child process if any
-informing the user when the child process is done with the job

