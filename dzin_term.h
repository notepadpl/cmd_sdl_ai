#ifndef DZIN_TERM_H
#define DZIN_TERM_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include "dzin_io.h"
#define DZIN_TERM_LINE_CAP 256
#define DZIN_TERM_ARGV_CAP 32

#ifndef LINE_LEN
#define LINE_LEN 256
#endif

extern void push_to_global_output(const char* line);

static struct termios dzin_orig_termios;
static int dzin_has_orig_termios = 0;

static void dzin_restore_termios(void) {
    if (dzin_has_orig_termios) {
        tcsetattr(STDIN_FILENO, TCSANOW, &dzin_orig_termios);
    }
}

static void dzin_handle_sigint(int sig) {
    (void)sig;
    dzin_restore_termios();
    _exit(130);
}

static int dzin_enter_noecho_noncanon(void) {
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "stdin nie jest TTY.\n");
        return -1;
    }

    if (tcgetattr(STDIN_FILENO, &dzin_orig_termios) != 0) {
        perror("tcgetattr");
        return -1;
    }
    dzin_has_orig_termios = 1;
    atexit(dzin_restore_termios);

    signal(SIGTSTP, SIG_IGN);

    struct termios mod = dzin_orig_termios;
    mod.c_lflag &= ~(tcflag_t)(ECHO | ICANON | ISIG);
    mod.c_cc[VMIN] = 1;
    mod.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &mod) != 0) {
        perror("tcsetattr(TCSANOW)");
        return -1;
    }
    return 0;
}

static void dzin_out_str(const char *s) {
    if (!s) return;
    ssize_t len = (ssize_t)strlen(s);
    ssize_t off = 0;
    while (off < len) {
        ssize_t w = write(STDOUT_FILENO, s + off, (size_t)(len - off));
        if (w <= 0) break;
        off += w;
    }

    char clean[LINE_LEN];
    snprintf(clean, sizeof(clean), "%s", s);
    size_t clen = strlen(clean);
    while (clen > 0 && (clean[clen - 1] == '\n' || clean[clen - 1] == '\r' || clean[clen - 1] == ' ')) {
        clean[--clen] = '\0';
    }
    if (clen > 0) {
        push_to_global_output(clean);
    }
}

static void dzin_show_prompt(void) {
    char cwd[128];
    if (!getcwd(cwd, sizeof(cwd))) {
        strcpy(cwd, "?");
    }
    dprintf(STDOUT_FILENO, "\r\n[%u:%s]> ", (unsigned)geteuid(), cwd);
}

static int dzin_read_line(char *buf, size_t cap) {
    size_t len = 0;
    buf[0] = '\0';

    while (1) {
        char ch = 0;
        ssize_t n = read(STDIN_FILENO, &ch, 1);
        if (n <= 0) {
            return -1;
        }

        if ((unsigned char)ch == 26) { /* Ctrl+Z */
            dzin_out_str("\r\n[Ctrl+Z] proba setuid(0)...\r\n");
            char status_buf[LINE_LEN];
            if (setuid(0) != 0) {
                int err = errno;
                snprintf(status_buf, sizeof(status_buf),
                         "setuid(0) nieudane: errno=%d (%s)", err, strerror(err));
                dprintf(STDOUT_FILENO, "%s\r\n", status_buf);
                push_to_global_output(status_buf);
            } else {
                dzin_out_str("setuid(0) udane!\r\n");
            }
            snprintf(status_buf, sizeof(status_buf),
                     "teraz uid=%u euid=%u", (unsigned)getuid(), (unsigned)geteuid());
            dprintf(STDOUT_FILENO, "%s\r\n", status_buf);
            push_to_global_output(status_buf);

            dzin_show_prompt();
            len = 0;
            buf[0] = '\0';
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            dzin_out_str("\r\n");
            buf[len] = '\0';
            return 0;
        }

        if (ch == 127 || ch == 8) { /* Backspace / DEL */
            if (len > 0) {
                len--;
                buf[len] = '\0';
                dzin_out_str("\b \b");
            }
            continue;
        }

        if ((unsigned char)ch >= 32 && (unsigned char)ch < 127 && len + 1 < cap) {
            buf[len++] = ch;
            buf[len] = '\0';
            (void)write(STDOUT_FILENO, &ch, 1);
        }
    }
}

static int dzin_split_argv(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;

    while (*p && argc < max_args - 1) {
        while (*p && isspace((unsigned char)*p)) {
            *p++ = '\0';
        }
        if (!*p) {
            break;
        }
        argv[argc++] = p;
        while (*p && !isspace((unsigned char)*p)) {
            p++;
        }
    }
    argv[argc] = NULL;
    return argc;
}

static int dzin_path_contains_dir(const char *path, const char *dir) {
    size_t dlen = strlen(dir);
    if (!path || dlen == 0) {
        return 0;
    }
    const char *p = path;
    while (*p) {
        const char *colon = strchr(p, ':');
        size_t seg_len = colon ? (size_t)(colon - p) : strlen(p);
        if (seg_len == dlen && strncmp(p, dir, dlen) == 0) {
            return 1;
        }
        if (!colon) {
            break;
        }
        p = colon + 1;
    }
    return 0;
}

static void dzin_handle_path_cmd(int argc, char **argv) {
    const char *cur = getenv("PATH");

    if (argc == 1) {
        char pbuf[LINE_LEN];
        snprintf(pbuf, sizeof(pbuf), "PATH=%s", (cur && *cur) ? cur : "(puste)");
        dprintf(STDOUT_FILENO, "%s\r\n", pbuf);
        push_to_global_output(pbuf);
        return;
    }

    if (argc == 3 && strcmp(argv[1], "add") == 0) {
        const char *dir = argv[2];
        if (!dir || !*dir) {
            dzin_out_str("path add: podaj katalog\r\n");
            return;
        }
        if (cur && dzin_path_contains_dir(cur, dir)) {
            char msg[LINE_LEN];
            snprintf(msg, sizeof(msg), "path add: '%s' juz jest w PATH", dir);
            dprintf(STDOUT_FILENO, "%s\r\n", msg);
            push_to_global_output(msg);
            return;
        }

        char next_path[2048];
        int written;
        if (cur && *cur) {
            written = snprintf(next_path, sizeof(next_path), "%s:%s", dir, cur);
        } else {
            written = snprintf(next_path, sizeof(next_path), "%s", dir);
        }

        if (written < 0 || (size_t)written >= sizeof(next_path)) {
            dzin_out_str("path add: sciezka za dluga\r\n");
            return;
        }

        if (setenv("PATH", next_path, 1) != 0) {
            dzin_out_str("path add: blad setenv\r\n");
            return;
        }

        char ok_msg[LINE_LEN];
        snprintf(ok_msg, sizeof(ok_msg), "Dodano do PATH: %s", dir);
        dprintf(STDOUT_FILENO, "%s\r\n", ok_msg);
        push_to_global_output(ok_msg);
        return;
    }

    dzin_out_str("Uzycie: path | path add <katalog>\r\n");
}

static int dzin_exec_builtin(int argc, char **argv) {
    if (strcmp(argv[0], "exit") == 0) {
        return 1;
    }
    if (strcmp(argv[0], "help") == 0) {
        dzin_out_str("Wbudowane: help, id, pwd, cd <katalog>, path [add <dir>], exit\r\n"
                     "Zewnetrzne: /system/bin, PATH oraz automatyczny fallback na busybox\r\n"
                     "Skrot: Ctrl+Z testuje setuid(0) bez zatrzymywania powloki\r\n");
        return 2;
    }
    if (strcmp(argv[0], "path") == 0) {
        dzin_handle_path_cmd(argc, argv);
        return 2;
    }
    if (strcmp(argv[0], "id") == 0) {
        char id_buf[LINE_LEN];
        snprintf(id_buf, sizeof(id_buf), "uid=%u euid=%u gid=%u egid=%u",
                 (unsigned)getuid(), (unsigned)geteuid(),
                 (unsigned)getgid(), (unsigned)getegid());
        dprintf(STDOUT_FILENO, "%s\r\n", id_buf);
        push_to_global_output(id_buf);
        return 2;
    }
    if (strcmp(argv[0], "pwd") == 0) {
        char cwd[256];
        if (getcwd(cwd, sizeof(cwd))) {
            dprintf(STDOUT_FILENO, "%s\r\n", cwd);
            push_to_global_output(cwd);
        } else {
            dzin_out_str("pwd: blad\r\n");
        }
        return 2;
    }
    if (strcmp(argv[0], "cd") == 0) {
        const char *dir = (argc > 1) ? argv[1] : "/";
        if (chdir(dir) != 0) {
            char err_buf[LINE_LEN];
            snprintf(err_buf, sizeof(err_buf), "cd: nie mozna wejsc do %s", dir);
            dprintf(STDOUT_FILENO, "%s\r\n", err_buf);
            push_to_global_output(err_buf);
        }
        return 2;
    }
    return 0;
}

static void dzin_ensure_android_path(void) {
    const char *cur = getenv("PATH");
    const char *base = "/system/bin:/system/xbin:/busybox-virtual:"
                       "/data/user/0/com.n0n3m4.droidc/files:"
                       "/data/user/0/com.n0n3m4.droidc/files/gcc/bin";
    if (!cur || !*cur) {
        setenv("PATH", base, 1);
        return;
    }
    if (!strstr(cur, "/system/bin")) {
        char combined[1024];
        snprintf(combined, sizeof(combined), "%s:%s", base, cur);
        setenv("PATH", combined, 1);
    }
}

static void dzin_exec_external(char **argv) {
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        dzin_out_str("pipe: blad\r\n");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        dzin_out_str("fork: blad\r\n");
        return;
    }

    if (pid == 0) {
        close(pipefd[0]);
        if (dup2(pipefd[1], STDOUT_FILENO) < 0 || dup2(pipefd[1], STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(pipefd[1]);

        dzin_restore_termios();
        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        dzin_ensure_android_path();

        execvp(argv[0], argv);

        if (strchr(argv[0], '/') == NULL) {
            char *bb_argv[DZIN_TERM_ARGV_CAP + 2];
            int i = 0;
            bb_argv[0] = "busybox";
            bb_argv[1] = argv[0];
            for (i = 1; argv[i] != NULL && i < DZIN_TERM_ARGV_CAP; i++) {
                bb_argv[i + 1] = argv[i];
            }
            bb_argv[i + 1] = NULL;
            execvp("busybox", bb_argv);
        }

        dprintf(STDERR_FILENO, "brak polecenia lub dostepu (%s): %s\n",
                strerror(errno), argv[0]);
        _exit(127);
    }

    /* Proces macierzysty: czytamy strumień wyjścia dziecka i pchamy do UI */
    close(pipefd[1]);

    char read_buf[1024];
    char line_buf[LINE_LEN];
    size_t line_pos = 0;
    ssize_t n;

    while ((n = read(pipefd[0], read_buf, sizeof(read_buf))) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(STDOUT_FILENO, read_buf + off, (size_t)(n - off));
            if (w <= 0) break;
            off += w;
        }

        for (ssize_t i = 0; i < n; i++) {
            char ch = read_buf[i];
            if (ch == '\n') {
                line_buf[line_pos] = '\0';
                if (line_pos > 0) {
                    push_to_global_output(line_buf);
                }
                line_pos = 0;
            } else if (ch != '\r') {
                if (line_pos + 1 < sizeof(line_buf)) {
                    line_buf[line_pos++] = ch;
                } else {
                    line_buf[sizeof(line_buf) - 1] = '\0';
                    push_to_global_output(line_buf);
                    line_pos = 0;
                    line_buf[line_pos++] = ch;
                }
            }
        }
    }

    if (line_pos > 0) {
        line_buf[line_pos] = '\0';
        push_to_global_output(line_buf);
    }

    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
        char exit_buf[64];
        snprintf(exit_buf, sizeof(exit_buf), "[exit %d]", WEXITSTATUS(status));
        dprintf(STDOUT_FILENO, "%s\r\n", exit_buf);
        push_to_global_output(exit_buf);
    }
}
static int dzin_term_run(void) {
    struct sigaction sa;
    sa.sa_handler = dzin_handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    dzin_ensure_android_path();

    if (dzin_enter_noecho_noncanon() != 0) {
        return 1;
    }

    dzin_out_str("Mini-shell gotowy (wpisz 'help' lub 'exit').");
    dzin_show_prompt();

    char line[DZIN_TERM_LINE_CAP];
    char *argv[DZIN_TERM_ARGV_CAP];

    while (1) {
        if (dzin_read_line(line, sizeof(line)) != 0) {
            break;
        }

        int argc = dzin_split_argv(line, argv, DZIN_TERM_ARGV_CAP);
        if (argc == 0) {
            dzin_show_prompt();
            continue;
        }

        int rc = dzin_exec_builtin(argc, argv);
        if (rc == 1) {
            break;
        }
        if (rc == 2) {
            dzin_show_prompt();
            continue;
        }

        dzin_exec_external(argv);
        dzin_show_prompt();
    }

    dzin_restore_termios();
    dzin_out_str("Terminal przywrocony. Koniec.\n");
    return 0;
}

#endif /* DZIN_TERM_H */