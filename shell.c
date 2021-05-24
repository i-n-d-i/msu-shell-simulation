#define COLOR_GREEN   "\x1b[32m"
#define COLOR_RESET   "\x1b[0m"
#define COLOR_BLUE    "\x1b[34m"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>

void super_print() {
  char *pwd = getcwd(NULL, 256);             //полный путь текущего каталога в нужный каталог
  char *user = getenv("USER");         //getenv - значение переменной среды
  char host[256];
  gethostname(host, _SC_HOST_NAME_MAX);                    //имя машины
  printf(COLOR_GREEN "%s@%s" COLOR_RESET ":" COLOR_BLUE "%s" COLOR_RESET "$ ", user, host, pwd);
  fflush(stdout);
  free(pwd);
}

char *get_word(char *end) {
    char *word = NULL, ch;
    int n = 0, bytes;
    do {
        ch = getchar();
    } while (ch == ' ');
    while (ch != ' ' && ch != '\n' && ch != '\t') {
        if (ch != '"') {
            bytes = (n + 1) * sizeof(char);
            word = realloc(word, bytes);
            word[n] = ch;
            n++;
        }
        ch = getchar();
    }
    bytes = (n + 1) * sizeof(char);
    word = realloc(word, bytes);
    word[n] = '\0';
    *end = ch;
    return word;
}

char **get_list(char *list_end) {
    char end = '\0', **list = NULL;
    int i = 0, flag = 0;
    while (end != '\n') {
        list = realloc(list, (i + 1) * sizeof(char*));
        list[i] = get_word(&end);
        if (strcmp(list[i], "|") == 0) {
            flag = 1;
            *list_end = end;
            break;
        }
        if (strcmp(list[i], "&&") == 0) {
            flag = 1;
            *list_end = 'c';
            break;
        }
        i++;
    }
    if (flag == 1) {
        free(list[i]);
        list[i] = NULL;
    } else {
        list = realloc(list, (i + 1) * sizeof(char*));
        list[i] = NULL;
        *list_end = '\n';
    }
    return list;
}

char ***get_arr_list(int *conv) {
    char ***list = NULL, list_end = '\0';
    int i = 0;
    while (list_end != '\n' && list_end != 'c') {
        list = realloc(list, (i + 1) * sizeof(char**));
        list[i] = get_list(&list_end);
        i++;
    }
    if (list_end == 'c') {
        *conv = 1;                      //если есть &&
    } else {
        *conv = 0;                      //если нет &&
    }
    list = realloc(list, (i + 1) * sizeof(char**));
    list[i] = NULL;
    return list;
}

void free_list(char **list) {
    for (int i = 0; list[i] != NULL; i++) {
        free(list[i]);
    }
    free(list);
}

void free_arr_list(char ***arr_list) {
    for (int i = 0; arr_list[i] != NULL; i++) {
        free_list(arr_list[i]);
    }
    free(arr_list);
}

int find_io(char **list, int flag) {
    if (list[0][0] != '\0') {
        for (int i = 0; list[i] != NULL; i++) {
            if (strcmp(list[i], "<") == 0 && flag == 0) {
                return i;
            }
            if (strcmp(list[i], ">") == 0 && flag == 1) {
                return i;
            }
        }
    }
    return 0;
}

void dup_with_check(int old_fd, int new_fd) {
    if (old_fd >= 0) {
        dup2(old_fd, new_fd);
    }
}

void next_pipe(int *old_fd, int *new_fd) {
    old_fd[0] = new_fd[0];
    old_fd[1] = new_fd[1];
}

void close_with_check(int *fd) {
    if (fd != NULL) {
        if (fd[0] >= 0) {
            close(fd[0]);
            fd[0] = -1;
        }
        if (fd[1] >= 0) {
            close(fd[1]);
            fd[1] = -1;
        }
    }
}

int change_dir(char ***list) {
    if (strcmp(list[0][0], "cd") == 0) {
        char *home = getenv("HOME");
        if (list[0][1] == NULL || strcmp(list[0][0], "~") == 0) {
            chdir(home);
        } else {
            chdir(list[0][1]);
        }
        return 1;
    }
    return 0;
}

void do_commands(char ***list) {
    if (change_dir(list) == 1) {
        return;
    }
    int fd_old[2] = {-1, -1}, fd_new[2] = {-1, -1}, pid, i;
    int in_num, out_num, last_list_num = 0, fd_in = -1,fd_out = -1; 
    char *in_var, *out_var;
    while (list[last_list_num + 1] != NULL) {
        last_list_num++;
    }  
    in_num = find_io(list[0], 0);
    if (in_num != 0) {               
        fd_in = open(list[0][in_num + 1], O_RDONLY);
    }
    out_num = find_io(list[last_list_num], 1);
    if (out_num != 0) {
        fd_out = open(list[last_list_num][out_num + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    }
    if (in_num != 0) {
        in_var = list[0][in_num];
        list[0][in_num] = NULL;
    }
    if (out_num != 0) {
        out_var = list[last_list_num][out_num];
        list[last_list_num][out_num] = NULL;
    }
    fd_old[0] = fd_in;
    for (i = 0; list[i] != NULL; i++) {      //pipes
        if (i != last_list_num) {
            pipe(fd_new);
        } else {
            fd_new[1] = fd_out;
        }
        pid = fork();
        if (pid < 0) {
            perror("Error");
            exit(1);
        } else if (pid == 0) {                  //if child
            dup_with_check(fd_old[0], 0);
            dup_with_check(fd_new[1], 1);
            close_with_check(fd_old);
            close_with_check(fd_new);
            if (execvp(list[i][0], list[i]) < 0) {
                perror("Error");
                exit(1);
            }
        }
        close_with_check(fd_old);
        next_pipe(fd_old, fd_new);
    }
    if (in_num != 0) {
        list[0][in_num] = in_var;
    }
    if (out_num != 0) {
        list[last_list_num][out_num] = out_var;
    }
    for (; i >= 0; i--) {
        wait(NULL);
    }
}

int back_check(char ***list) {                //фоновый режим
    int i, j, k;
    char ch;
    if (list[0] == NULL || list[0][0] == NULL || list[0][0][0] == '\0')
            return -1;
    for (i = 0; list[i] != NULL; i++) {
        for (j = 0; list[i][j] != NULL; j++) {
            for (k = 0; list[i][j][k] != '\0'; k++) {
                ch = list[i][j][k];   
            }
        }
    }
    if (ch == '&') {
        if (k == 1) {                        //если отдельное слово
            free(list[i - 1][j - 1]);
            list[i - 1][j - 1] = NULL;
        } else {                             //если последний символ последнего слова
            list[i - 1][j - 1][k - 1] = '\0';
        }
        if (list[0] == NULL || list[0][0] == NULL || list[0][0][0] == '\0') {
            return -1;
        }
        return 1;
    }
    return 0;
}

void call_proc(char ***list, int *number, int *conv) {
    int pid = fork();
    if (pid < 0) {
        perror("Error");
        exit(1);
    } else if (pid == 0) {
        do_commands(list);
        free_arr_list(list);
        exit(0);
    } else {
        if (*conv != 0) {          //&
            int wstatus;
            wait(&wstatus);
            printf("%d\n", WEXITSTATUS(wstatus));
        } else {
            *number += 1;
            printf("[%d] %d\n", *number, pid);
        }
    }
}

            
void handler(int signo) {
    putchar('\n');
    super_print();
}
            
int main() {
    signal(SIGINT, handler);
    char ***list;
    int checker, conv, number = 0;
    super_print();
    list = get_arr_list(&conv);
    while (list[0][0] == NULL) {
        free_arr_list(list);
        super_print();
        list = get_arr_list(&conv);
    }
    while (strcmp(list[0][0], "exit") != 0 && strcmp(list[0][0], "quit") != 0) {
        number = 0;
        checker = back_check(list);       //наличие &
        if (checker == 0 && conv == 0) {  //если нету & и &&  
            do_commands(list);
        } else if (checker == 1 || (checker >= 0 && conv == 1)){   //если есть & или &&
            call_proc(list, &number, &conv);
        }
        free_arr_list(list);
        if (conv == 0) {
            super_print();
        }
        list = get_arr_list(&conv);
        while (list[0][0] == NULL) {
            free_arr_list(list);
            super_print();
            list = get_arr_list(&conv);
        }
    }
    free_arr_list(list);
    return 0;
}
