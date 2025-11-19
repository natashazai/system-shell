// Natalia Zaitseva
//May 15th 2025 - core functionality and base
//November 18th - Added UI 

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>

#include <ncurses.h>
#include <stdarg.h>
#include "race.h"

WINDOW *header_win = NULL;
WINDOW *output_win = NULL;
WINDOW *input_win  = NULL;


// global var for status
int status = 0;
// global var to store child num
int child_count = 0;
// global array to store child pids
pid_t background_children[10] = {0};
// flag to check if foreground only mode was turned on
int foreground_only_flag = 0;

void input_line(int* exit_flag);
void cd_command(char user_input_command[]);
void dollar_expansion(char user_input_command[]);
int dollar_check(char user_input_command[]);
void status_command();
void handle_commands(char user_input_command[]);
int background_check(char user_input_command[]);
int input_check(char user_input_command[]);
int output_check(char user_input_command[]);
void child_signal_pid_handler(int signal_number);
void run_foreground_commands(int input_flag, int output_flag, char input_file[20], char output_file[20], char **args);
void run_background_commands(int input_flag, int output_flag, char input_file[20], char output_file[20], char **args, int args_num, int background_flag);



void ui_init() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    if (has_colors()) {
        start_color();
        //header
        init_pair(1, COLOR_WHITE, COLOR_CYAN);
        //output
        init_pair(2, COLOR_CYAN, COLOR_BLACK);
        //input
        init_pair(3, COLOR_BLACK, COLOR_WHITE);
        //errors
        init_pair(4, COLOR_RED, COLOR_BLACK);
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    header_win = newwin(3, cols, 0, 0);
    output_win = newwin(rows - 6, cols, 3, 0);
    input_win  = newwin(3, cols, rows - 3, 0);

    if (has_colors()) {
        wbkgd(header_win, COLOR_PAIR(1));
        wbkgd(output_win, COLOR_PAIR(2));
        wbkgd(input_win,  COLOR_PAIR(3));
    }

    scrollok(output_win, TRUE);

    wclear(header_win);
    wclear(output_win);
    wclear(input_win);

    wrefresh(header_win);
    wrefresh(output_win);
    wrefresh(input_win);
}

void ui_cleanup() {
    if (header_win) delwin(header_win);
    if (output_win) delwin(output_win);
    if (input_win)  delwin(input_win);
    endwin();
}

void ui_draw_header() {
    int cols;
    int rows;
    getmaxyx(stdscr, rows, cols);

    werase(header_win);

    const char *title = " System Shell ";
    int x = (cols - (int)strlen(title)) / 2;
    mvwprintw(header_win, 1, x, "%s", title);

    mvwprintw(header_win, 1, 2, "PID: %d", getpid());

    const char *mode = foreground_only_flag ? "Foreground-only: ON"
                                            : "Foreground-only: OFF";
    mvwprintw(header_win, 1, cols - (int)strlen(mode) - 2, "%s", mode);

    wrefresh(header_win);
}

void ui_print(const char *fmt, ...) {
    // start a little in from the left so we don't hug the edge
    wmove(output_win, getcury(output_win), 1);
    va_list args;
    va_start(args, fmt);
    vw_printw(output_win, fmt, args);
    va_end(args);
    wrefresh(output_win);
}

void ui_print_error(const char *fmt, ...) {
    if (has_colors()) wattron(output_win, COLOR_PAIR(4));
    wmove(output_win, getcury(output_win), 1);
    va_list args;
    va_start(args, fmt);
    vw_printw(output_win, fmt, args);
    va_end(args);
    if (has_colors()) wattroff(output_win, COLOR_PAIR(4));
    wrefresh(output_win);
}

void ui_prompt() {
    werase(input_win);
    mvwprintw(input_win, 1, 2, ": ");
    wmove(input_win, 1, 4);
    wrefresh(input_win);
}

int ui_read_line(char *buf, size_t size) {
    keypad(input_win, TRUE);
    echo();
    curs_set(1);
    wgetnstr(input_win, buf, (int)size - 1);
    noecho();
    return 0;
}

void ui_show_loading() {
    const char *msg = "Loading System Shell";
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int x = (cols - (int)strlen(msg)) / 2;
    int y = rows / 2;

    for (int i = 0; i <= 3; i++) {
        werase(stdscr);
        mvprintw(y, x, "%s", msg);
        for (int j = 0; j < i; j++) {
            printw(".");
        }
        refresh();
        napms(300);
    }

    napms(200);
    werase(stdscr);
    refresh();
}

int output_check(char user_input_command[]){
    char *result = strstr(user_input_command, ">");
    if(result != NULL){
        return 1;
    }
    return 0;
}

int input_check(char user_input_command[]){
    char *result = strstr(user_input_command, "<");
    if(result != NULL){
        return 1;
    }
    return 0;
}

int dollar_check(char user_input_command[]){
    char *result = strstr(user_input_command, "$$");
    if(result != NULL){
        return 1;
    }
    return 0;
}

int background_check(char user_input_command[]){
    char *result = strstr(user_input_command, "&");
    if(result != NULL){
        return 1;
    }
    return 0;
}


void cd_command(char user_input_command[]){
    char save_full_command[2049] = "";
    strcpy(save_full_command, user_input_command);
    char *args[2];
    char *token = strtok(save_full_command, " ");
    args[0] = token;
    args[1] = strtok(NULL, " ");
    char s[100];
    if (args[1] == NULL) {
        ui_print("old dir: %s\n", getcwd(s, 100));
        char *home_dir = getenv("HOME");
        chdir(home_dir);
        ui_print("new dir: %s\n", getcwd(s, 100));
    } else {
        chdir(args[1]);
        ui_print("new dir: %s\n", getcwd(s, 100));
    }
}

void status_command(){
    int i = 0;
    int empty_flag = 0;
    for (i = 0; i < 20; i++) {
        if (background_children[i] != 0) {
            empty_flag = 1;
            break;
        }
    }
    if(empty_flag == 1){
        status = 0;
    }
    ui_print("%d\n", status);
}


void child_signal_pid_handler(int signal_number){
    pid_t wait_result;
    while ((wait_result = waitpid(-1, &status, WNOHANG)) > 0) {
        if(WIFEXITED(status)){
            int exit_code = WEXITSTATUS(status);
            //ui_print("The child %d normally ended with code %d\n",wait_result, exit_code);
            status = exit_code;
        }else if(WIFSIGNALED(status)){
            int signal_number2 = WTERMSIG(status);
            //ui_print("The child process %d was killed by signal number %d\n",wait_result, signal_number2);
        }
        int count = 20;
        int i = 0;
        for (i = 0; i < 20; i++) {
            if (background_children[i] == wait_result) {
                background_children[i] = background_children[count - 1];
                count--;
                break;
            }
        }
    }
}

void sigtstp_handler_func(int signal_number){
    if(foreground_only_flag == 0){
        foreground_only_flag = 1;
        ui_print_error("Entered foreground only mode.\n");
    }else{
        foreground_only_flag = 0;
        ui_print_error("Stopped foreground only mode.\n");
    }
}

void run_foreground_commands(int input_flag, int output_flag, char input_file[20], char output_file[20], char **args){
    int pipefd[2];
    int capture_output = (output_flag == 0);

    if (capture_output) {
        if (pipe(pipefd) == -1) {
            ui_print_error("pipe() failed\n");
            return;
        }
    }

    pid_t fork_result = fork();
    if (fork_result == -1) {
        ui_print_error("fork() failed\n");
        if (capture_output) {
            close(pipefd[0]);
            close(pipefd[1]);
        }
        return;
    }

    if(fork_result == 0){
        struct sigaction sigint_child_handler = {0};
        sigint_child_handler.sa_handler = SIG_DFL;
        sigfillset(&sigint_child_handler.sa_mask);
        sigint_child_handler.sa_flags = SA_RESTART;
        if (sigaction(SIGINT, &sigint_child_handler, NULL) == -1) {
            _exit(EXIT_FAILURE);
        }

        if (capture_output) {
            close(pipefd[0]);
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[1]);
        }

        if(output_flag == 1){
            int output_file_ptr = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
            if (output_file_ptr < 0) {
                perror("smallsh");
                _exit(1);
            }
            if (dup2(output_file_ptr, STDOUT_FILENO) < 0) {
                perror("smallsh");
                _exit(1);
            }
            close(output_file_ptr);
        }

        if(input_flag == 1){
            int input_file_ptr = open(input_file, O_RDONLY);
            if (input_file_ptr < 0) {
                perror("smallsh");
                _exit(1);
            }
            if (dup2(input_file_ptr, STDIN_FILENO) < 0) {
                perror("smallsh");
                _exit(1);
            }
            close(input_file_ptr);
        }

        execvp(args[0], args);
        perror("smallsh");
        _exit(1);

    } else {
        int exit_status;

        if (capture_output) {
            close(pipefd[1]);
            char buffer[1024];
            ssize_t n;
            while ((n = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[n] = '\0';
                ui_print("%s", buffer);
            }
            close(pipefd[0]);
        }

        waitpid(fork_result, &exit_status, 0);
    }
}


void run_background_commands(int input_flag, int output_flag, char input_file[20], char output_file[20], char **args, int args_num, int background_flag){
    if(strcmp(args[0], "status") == 0){
        status_command();
        return;
    }

    if (background_flag == 1 && strcmp(args[args_num - 1], "&") == 0) {
        args[args_num - 1] = NULL;
        args_num--;
    }

    pid_t fork_result = fork();
    if (fork_result == -1) {
        ui_print_error("fork() failed\n");
        return;
    }

    background_children[child_count] = fork_result;
    child_count++;

    if(fork_result == 0){
        execvp(args[0], args);
        perror("smallsh");
        _exit(1);
    }else{
        ui_print("Background process started with PID %d\n", fork_result);
    }
}

//handles non-built in commands
void handle_commands(char user_input_command[]){
    char full_command[2049];
    strcpy(full_command, user_input_command);
    full_command[strcspn(full_command, "\n")] = '\0';
    char input_file[20] = "";
    char output_file[20] = "";
    char *args[512];
    int args_num = 0;
    char *token = strtok(full_command, " ");
    while (token != NULL) {
        if(strcmp(token, ">") == 0){
            token = strtok(NULL, " ");
            strcpy(output_file, token);
            token = strtok(NULL, " ");
            continue;
        }else if(strcmp(token, "<") == 0){
            token = strtok(NULL, " ");
            strcpy(input_file, token);
            token = strtok(NULL, " ");
            continue;
        }else{
            args[args_num++] = token;
            token = strtok(NULL, " ");
        }
    }
    args[args_num] = NULL;

    int background_flag = background_check(user_input_command);
    int input_flag = input_check(user_input_command);
    int output_flag = output_check(user_input_command);

    if(background_flag == 0){
        run_foreground_commands(input_flag, output_flag, input_file, output_file, args);
    }else{
        if(foreground_only_flag == 1){
            if (args_num > 0 && strcmp(args[args_num - 1], "&") == 0) {
                args[args_num - 1] = NULL;
                args_num--;
            }
            run_foreground_commands(input_flag, output_flag, input_file, output_file, args);
        }else{
            run_background_commands(input_flag, output_flag, input_file, output_file, args, args_num, background_flag);
        }
    }
}

void help_command(WINDOW *output) {
    werase(output);
    const char *lines[] = {
        "=================== smallsh Help Menu ===================",
        "",
        "Your custom interactive shell supports the following",
        "commands, features, and UI tools:",
        "",
        "---------------------------------------------------------",
        " Built-in Commands",
        "---------------------------------------------------------",
        "  exit",
        "      - Exit the shell.",
        "      - Automatically kills all background processes.",
        "",
        "  cd [path]",
        "      - Change the working directory.",
        "      - No argument: goes to $HOME.",
        "",
        "  status",
        "      - Prints exit status or terminating signal of the",
        "        most recent foreground process.",
        "",
        "---------------------------------------------------------",
        " Execution Features",
        "---------------------------------------------------------",
        "  External Commands",
        "      - Executes any program found in PATH.",
        "      - Uses fork() + execvp().",
        "",
        "  Arguments",
        "      - Supports up to 20 arguments.",
        "",
        "  $$ Expansion",
        "      - All occurrences of $$ expand to the PID of smallsh.",
        "",
        "  Input Redirection:   < filename",
        "  Output Redirection:  > filename",
        "      - Uses dup2() inside the child process.",
        "",
        "  Background Execution:  &",
        "      - Runs processes without waiting.",
        "      - Prints background pid.",
        "",
        "  Foreground-only Mode: CTRL+Z",
        "      - Toggles mode ON/OFF.",
        "",
        "---------------------------------------------------------",
        " Signal Behavior",
        "---------------------------------------------------------",
        "  CTRL+C (SIGINT)",
        "      - Ignored by the shell.",
        "      - Kills foreground children.",
        "",
        "  CTRL+Z (SIGTSTP)",
        "      - Toggles foreground-only mode.",
        "",
        "---------------------------------------------------------",
        " Custom UI Features",
        "---------------------------------------------------------",
        "  • ncurses interface",
        "  • Header bar with PID + mode",
        "  • Loading animations",
        "  • Clean separation of output areas",
        "",
        "---------------------------------------------------------",
        " Thread Race Visualization (type: race)",
        "---------------------------------------------------------",
        "  - Multithreaded car race demo",
        "  - Each car runs in its own pthread",
        "  - Live animation inside UI",
        "  - Demonstrates concurrency",
        "  - Mutex-protected drawing",
        "",
        "---------------------------------------------------------",
    };

    int n_lines = sizeof(lines)/sizeof(lines[0]);
    for (int i = 0; i < n_lines; i++) {
        mvwprintw(output, i+1, 2, "%s", lines[i]);
    }
    wrefresh(output);
    wgetch(output);
    werase(output);
    wrefresh(output);
}


//dollar expansion handler
void dollar_expansion(char user_input_command[]){
    char pid[20];
    char rewrote_string[2049];
    sprintf(pid, "%d", getpid());
    size_t pid_length = strlen(pid);
    int i = 0;
    int i_in = 0;
    int j_out = 0;
    if(dollar_check(user_input_command) == 1){
        while (user_input_command[i_in] != '\0'){
            if (user_input_command[i_in] == '$') {
                int count = 0;
                while (user_input_command[i_in] == '$') {
                    count++;
                    i_in++;
                }
                int pairs_num = count / 2;
                int remainder = count % 2;
                for (i = 0; i < pairs_num; i++) {
                    strcpy(&rewrote_string[j_out], pid);
                    j_out += pid_length;
                }
                if (remainder == 1) {
                    rewrote_string[j_out++] = '$';
                }
            } else {
                rewrote_string[j_out++] = user_input_command[i_in++];
            }
        }
        user_input_command[j_out] = '\0';
        strcpy(user_input_command, rewrote_string);
    }
}

void input_line(int* exit_flag){
    char user_input_command[2049] = "";
    char user_input_command_to_use[2049] = "";

    ui_draw_header();
    ui_prompt();

    wrefresh(output_win);

    ui_read_line(user_input_command, sizeof(user_input_command));

    dollar_expansion(user_input_command);

    strcpy(user_input_command_to_use, user_input_command);
    user_input_command[strcspn(user_input_command_to_use, "\n")] = '\0';

    char *command_token = strtok(user_input_command_to_use, " ");
    if (command_token == NULL){
        ui_prompt();
        return;
    }

    if(strcmp(user_input_command, "exit") == 0){
        int i = 0;
        for(i = 0; i < 20; i++){
            if(background_children[i] != 0){
                kill(background_children[i], SIGKILL);
            }
        }
        sleep(1);
        *exit_flag = 1;
    }else if(strcmp(user_input_command, "status") == 0){
        status_command();
    }else if(strcmp(user_input_command, "cd") == 0){
        cd_command(user_input_command);
    }else if(strcmp(command_token, "cd") == 0){
        cd_command(user_input_command);
    }else if(strcmp(command_token, "help") == 0){
        help_command(output_win);
    }else if(strcmp(command_token, "race") == 0){
        //thread car race
        race_command(output_win);
    }else if(user_input_command[0] == '#'){
        // comment
    }else if(user_input_command[0] == ' '){
        // leading space
    }else if(strlen(user_input_command) == 0){
        // empty
    }else{
        handle_commands(user_input_command);
    }
}

int main() {
    ui_init();
    ui_show_loading();

    struct sigaction child_pid_signal = {0};
    child_pid_signal.sa_handler = child_signal_pid_handler;
    sigfillset(&child_pid_signal.sa_mask);
    child_pid_signal.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &child_pid_signal, NULL) == -1) {
        ui_print_error("sigaction(SIGCHLD) failed\n");
        ui_cleanup();
        exit(EXIT_FAILURE);
    }

    struct sigaction sigint_handler = {0};
    sigint_handler.sa_handler = SIG_IGN;
    sigfillset(&sigint_handler.sa_mask);
    sigint_handler.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sigint_handler, NULL) == -1) {
        ui_print_error("sigaction(SIGINT) failed\n");
        ui_cleanup();
        exit(EXIT_FAILURE);
    }

    struct sigaction sigtstp_handler = {0};
    sigtstp_handler.sa_handler = sigtstp_handler_func;
    sigfillset(&sigtstp_handler.sa_mask);
    sigtstp_handler.sa_flags = SA_RESTART;
    if (sigaction(SIGTSTP, &sigtstp_handler, NULL) == -1) {
        ui_print_error("sigaction(SIGTSTP) failed\n");
        ui_cleanup();
        exit(EXIT_FAILURE);
    }

    int exit_flag = 0;
    while(exit_flag != 1){
        input_line(&exit_flag);
    }

    ui_cleanup();
    return 0;
}
