#define _POSIX_C_SOURCE 200809L

//std c library includes
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

//unix includes
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>

#define WELL_KNOWN_FIFO "REQUESTS"
#define PID_FILE        "daemon.pid"
#define MAX_ARGS 64
#define MAX_LINE 4096
enum { MAX_PID_LEN = 32 };

typedef long long Pid_Print;
#define PID_FMT "%lld"

static void fatal(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "fatal: ");
    vfprintf(stderr, fmt, ap);
    if (fmt[0] != '\0' && fmt[strlen(fmt) - 1] == ':') {
        fprintf(stderr, " %s", strerror(errno));
    }
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static void warn_msg(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "warn: ");
    vfprintf(stderr, fmt, ap);
    if (fmt[0] != '\0' && fmt[strlen(fmt) - 1] == ':') {
        fprintf(stderr, " %s", strerror(errno));
    }
    fprintf(stderr, "\n");
    va_end(ap);
}

static void do_redirect(int fd, int redir_fd) {
    if (fd != redir_fd) {
        if (dup2(fd, redir_fd) < 0) {
            fatal("cannot redirect %d to %d:", redir_fd, fd);
        }
        close(fd);
    }
}

static bool pid_is_alive(pid_t pid) {
    return kill(pid, 0) == 0 || errno == EPERM;
}

static pid_t read_pid_file(void) {
    FILE *f = fopen(PID_FILE, "r");
    if (!f) return -1;
    long long pid;
    int r = fscanf(f, "%lld", &pid);
    fclose(f);
    if (r != 1) return -1;
    return (pid_t)pid;
}

static void write_pid_file(void) {
    FILE *f = fopen(PID_FILE, "w");
    if (!f) fatal("cannot write PID file %s:", PID_FILE);
    fprintf(f, PID_FMT "\n", (Pid_Print)getpid());
    fclose(f);
}

static void remove_pid_file(void) {
    unlink(PID_FILE);
}

static void check_singleton(void) {
    pid_t existing = read_pid_file();
    if (existing < 0) return;
    if (pid_is_alive(existing)) {
        fprintf(stderr,
            "fatal: daemon already running as PID %lld\n"
            "       stop it first with: kill %lld\n",
            (Pid_Print)existing, (Pid_Print)existing);
        exit(1);
    }
    fprintf(stderr, "warn: removing stale PID file (PID %lld was dead)\n",
            (Pid_Print)existing);
    remove_pid_file();
}

static void handle_signal(int sig) {
    (void)sig;
    remove_pid_file();
    _exit(0);
}

static void install_signal_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);
}

static void get_args(const char *exe_path, char *args_line, char **argv_out) {
    int argc = 0;
    argv_out[argc++] = (char *)exe_path;
    char *p = args_line;
    while (*p != '\0' && *p != '\n') {
        if (argc >= MAX_ARGS - 1) fatal("too many arguments in request");
        char *q;
        long n = strtol(p, &q, 10);
        if (p == q || n < 0) fatal("cannot read argument length");
        if (*q != ' ') fatal("malformed argument line");
        p = q + 1;
        size_t remain = strlen(p);
        if ((long)remain < n) fatal("argument length exceeds remaining input");
        char *arg = p;
        p += n;
        if (*p != ' ' && *p != '\n' && *p != '\0') fatal("argument not followed by separator");
        if (*p == ' ' || *p == '\n') { *p = '\0'; p++; }
        argv_out[argc++] = arg;
    }
    argv_out[argc] = NULL;
}

static void do_serve(const char *fifo_path, const char *exe_path);
static void make_worker(const char *exe_path, const char *client_id);
static void do_work(const char *exe_path, const char *client_id);

int main(int argc, const char *argv[]) {
    if (argc != 3) fatal("usage: %s SERVER_DIR EXEC_PATH", argv[0]);

    const char *server_dir = argv[1];
    const char *exe_path = argv[2];

    if (chdir(server_dir) < 0) fatal("cannot chdir to %s:", server_dir);

    check_singleton();

    if (mkfifo(WELL_KNOWN_FIFO, 0666) < 0) {
        if (errno != EEXIST) fatal("cannot create FIFO %s:", WELL_KNOWN_FIFO);
    }

    write_pid_file();
    install_signal_handlers();

    fprintf(stderr, "daemon started, PID %lld\n", (Pid_Print)getpid());

    do_serve(WELL_KNOWN_FIFO, exe_path);

    remove_pid_file();
    return 0;
}

static void do_serve(const char *fifo_path, const char *exe_path) {
    int req_fd = open(fifo_path, O_RDWR);
    if (req_fd < 0) fatal("cannot open request FIFO %s:", fifo_path);

    char buf[PIPE_BUF * 4];
    char leftover[PIPE_BUF * 4] = {0};
    size_t leftover_len = 0;

    for (;;) {
        ssize_t n = read(req_fd, buf, sizeof(buf) - 1);
        if (n < 0) { warn_msg("read error on request FIFO:"); continue; }
        if (n == 0) continue;
        buf[n] = '\0';

        char combined[PIPE_BUF * 8];
        memcpy(combined, leftover, leftover_len);
        memcpy(combined + leftover_len, buf, n + 1);
        size_t combined_len = leftover_len + n;
        leftover_len = 0;

        char *line = combined;
        while (*line != '\0') {
            char *newline = strchr(line, '\n');
            if (!newline) {
                leftover_len = strlen(line);
                memcpy(leftover, line, leftover_len);
                leftover[leftover_len] = '\0';
                break;
            }

            *newline = '\0';

            if (*line == '\0') {
                close(req_fd);
                return;
            }

            fprintf(stderr, "dispatching worker for client: %s\n", line);
            make_worker(exe_path, line);

            line = newline + 1;
        }
        (void)combined_len;
    }

    close(req_fd);
}

static void make_worker(const char *exe_path, const char *client_id) {
    pid_t pid = fork();
    if (pid < 0) {
        warn_msg("fork failed in make_worker:");
        return;
    }
    if (pid == 0) {
        do_work(exe_path, client_id);
        _exit(1);
    }
    fprintf(stderr, "spawned worker pid=%lld for client=%s\n", (Pid_Print)pid, client_id);

    // Reap any exited workers opportunistically to avoid zombies.
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

static void do_work(const char *exe_path, const char *client_id) {
    size_t id_len = strlen(client_id);
    size_t path_len = id_len + 3;

    char fifo_in[path_len];
    char fifo_out[path_len];

    snprintf(fifo_in, path_len, "%s.0", client_id);
    snprintf(fifo_out, path_len, "%s.1", client_id);

    // Use blocking FIFO opens: this waits for the client side to connect
    // and avoids racing into an EOF before args are written.
    int fd_in = open(fifo_in, O_RDONLY);
    if (fd_in < 0) fatal("cannot open private read FIFO %s:", fifo_in);

    // Open write-only so the client will see EOF when the worker exits.
    // (If we open RDWR here, the FIFO can stay "open for write" and the
    // client blocks forever waiting for EOF.)
    int fd_out = open(fifo_out, O_WRONLY);
    if (fd_out < 0) fatal("cannot open private write FIFO %s:", fifo_out);
    signal(SIGPIPE, SIG_IGN);

    // Read request args until newline.
    char args_line[MAX_LINE];
    size_t total = 0;
    while (total < sizeof(args_line) - 1) {
        ssize_t nread = read(fd_in, args_line + total, sizeof(args_line) - 1 - total);
        if (nread < 0) {
            fatal("read error on args from %s:", fifo_in);
        }
        if (nread == 0) break;
        total += nread;
        if (args_line[total - 1] == '\n') break;
    }
    if (total == 0) fatal("empty args line from %s:", fifo_in);
    args_line[total] = '\0';

    pid_t self = getpid();
    char pid_buf[MAX_PID_LEN];
    int pid_len = snprintf(pid_buf, sizeof(pid_buf), PID_FMT "\n", (Pid_Print)self);
    if (pid_len < 0 || pid_len >= (int)sizeof(pid_buf))
        fatal("failed to format worker PID");

    // Write PID to the client before streaming worker output.
    size_t written = 0;
    while (written < (size_t)pid_len) {
        ssize_t w = write(fd_out, pid_buf + written, pid_len - written);
        if (w < 0) {
            fatal("failed to write worker PID to %s:", fifo_out);
        }
        written += w;
    }

    do_redirect(fd_in, STDIN_FILENO);
    do_redirect(fd_out, STDOUT_FILENO);

    char *argv_exec[MAX_ARGS];
    get_args(exe_path, args_line, argv_exec);

    execv(exe_path, argv_exec);
    fatal("execv failed for %s:", exe_path);
}