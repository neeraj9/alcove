/* Copyright (c) 2014-2015, Michael Santos <michael.santos@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include "alcove.h"

#include <sys/stat.h>

#ifdef __linux__
#pragma message "Support for namespaces using clone(2) enabled"
#else
#pragma message "Support for namespaces using clone(2) disabled"
#endif

static int alcove_signal_init();
static int alcove_rlimit_init();
static int alcove_fd_init(char *fifo);
int alcove_fdmove(int fd, int dst);

static void usage(alcove_state_t *);

extern char *__progname;

    void
sighandler(int sig)
{
    if (write(ALCOVE_SIGWRITE_FILENO, &sig, sizeof(sig)) != sizeof(sig))
        (void)close(ALCOVE_SIGWRITE_FILENO);
}

    int
main(int argc, char *argv[])
{
    alcove_state_t *ap = NULL;
    int ch = 0;
    char *fifo = NULL;
    int boot = 1;

    ap = calloc(1, sizeof(alcove_state_t));
    if (ap == NULL)
        err(EXIT_FAILURE, "calloc");

    ALCOVE_SETOPT(ap, alcove_opt_termsig, 1);
    ALCOVE_SETOPT(ap, alcove_opt_exit_status, 1);

    ap->maxfd = sysconf(_SC_OPEN_MAX);
    ap->maxchild = ap->maxfd / ALCOVE_MAXFILENO - ALCOVE_MAXFILENO;
    ap->maxforkdepth = MAXFORKDEPTH;

    if (alcove_signal_init() < 0)
        err(EXIT_FAILURE, "alcove_signal_init");

    if (alcove_rlimit_init() < 0)
        err(EXIT_FAILURE, "alcove_rlimit_init");

    while ( (ch = getopt(argc, argv, "c:d:m:hv")) != -1) {
        switch (ch) {
            case 'c':
                if (fifo) free(fifo);
                fifo = strdup(optarg);
                if (fifo == NULL)
                    err(EXIT_FAILURE, "strdup");
                break;
            case 'd':
                boot = 0;
                ap->depth = (u_int16_t)atoi(optarg);
                break;
            case 'm':
                ap->maxchild = (u_int16_t)atoi(optarg);
                break;
            case 'v':
                ap->verbose++;
                break;
            case 'h':
            default:
                usage(ap);
        }
    }

    ap->fdsetsize = ap->maxchild;

    ap->child = calloc(ap->fdsetsize, sizeof(alcove_child_t));
    if (ap->child == NULL)
        err(EXIT_FAILURE, "calloc");

    if (boot) {
        if (alcove_fd_init(fifo) < 0)
            err(EXIT_FAILURE, "alcove_fd_init");
    }

    free(fifo);

    alcove_event_init(ap, boot);
    exit(0);
}

    static int
alcove_signal_init()
{
    struct sigaction act;
    int sig = 0;

    (void)memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_DFL;
    (void)sigfillset(&act.sa_mask);

    for (sig = 1; sig < NSIG; sig++) {
        if (sigaction(sig, &act, NULL) < 0) {
            if (errno == EINVAL)
                continue;

            return -1;
        }
    }

    act.sa_handler = sighandler;

    if (sigaction(SIGCHLD, &act, NULL) < 0)
        return -1;

    return 0;
}

    static int
alcove_rlimit_init()
{
    struct rlimit stack_size = {0};

    if (getrlimit(RLIMIT_STACK, &stack_size) < 0)
        return -1;

    /* Reset an unlimited stack size to a default value. The default is
     * set to 8Mb, the default for linux (_STK_LIM in linux/resources.h).
     *
     * The current value of RLIMIT_STACK is used for allocating the
     * stack of cloned processes.
     *
     */

    if (stack_size.rlim_cur == RLIM_INFINITY) {
        stack_size.rlim_cur = 8 * 1024 * 1024;
        if (setrlimit(RLIMIT_STACK, &stack_size) < 0)
            return -1;
    }

    return 0;
}

    static int
alcove_fd_init(char *fifo)
{
    int sigpipe[2] = {0};
    int fdctl = 0;

    if (!fifo)
        return -1;

     /* The fd may have been opened by another program. For example,
      * valgrind will use the first available fd for the log file. */
    if (alcove_fdmove(ALCOVE_SIGREAD_FILENO, 8) < 0)
        return -1;

    if (alcove_fdmove(ALCOVE_SIGWRITE_FILENO, 8) < 0)
        return -1;

    if (alcove_fdmove(ALCOVE_FDCTL_FILENO, 8) < 0)
        return -1;

    if (pipe(sigpipe) < 0)
        return -1;

    /* XXX fd's will overlap */
    if (sigpipe[0] != ALCOVE_SIGREAD_FILENO) {
        if (dup2(sigpipe[0], ALCOVE_SIGREAD_FILENO) < 0)
            return -1;
        if (close(sigpipe[0]) < 0)
            return -1;
    }

    if (sigpipe[1] != ALCOVE_SIGWRITE_FILENO) {
        if (dup2(sigpipe[1], ALCOVE_SIGWRITE_FILENO) < 0)
            return -1;
        if (close(sigpipe[1]) < 0)
            return -1;
    }

    if ( (alcove_setfd(ALCOVE_SIGREAD_FILENO, FD_CLOEXEC|O_NONBLOCK) < 0)
            || (alcove_setfd(ALCOVE_SIGWRITE_FILENO, FD_CLOEXEC|O_NONBLOCK) < 0))
        return -1;

    /* The control fd used to signal that the port has called exec(). The
     * control fd is a fifo. beam opens the fd in read-only mode.  When all
     * the writers of a fifo call close(), the reader receives EOF.
     *
     * If the fifo is opened O_WRONLY, the port will deadlock: the open
     * call in the port will block for a reader and the erlang side will
     * be blocked waiting for the port.
     *
     * Some OS'es (e.g., Linux, see fifo(7)) allow a fifo to be opened
     * in non-blocking mode but the behaviour is unspecified. In practice:
     *
     * - opening the fifo read-only returns immediately
     *
     * - opening the fifo write-only fifo returns ENXIO
     *
     * - opening the fifo read-write with or without O_NONBLOCK will
     *   return immediately but the control fd cannot be used for sending
     *   data because each of the readers will race to receive any writes
     *
     */
    if (mkfifo(fifo, S_IWUSR|S_IRUSR|S_IRGRP|S_IROTH) < 0)
        return -1;

    fdctl = open(fifo, O_RDWR|O_CLOEXEC|O_NONBLOCK);
    if (fdctl < 0)
        return -1;

    if (fdctl != ALCOVE_FDCTL_FILENO) {
        if (dup2(fdctl, ALCOVE_FDCTL_FILENO) < 0)
            return -1;
        if (close(fdctl) < 0)
            return -1;
    }

    return 0;
}

    int
alcove_fdmove(int fd, int dst)
{
    int flags = 0;

    flags = fcntl(fd, F_GETFD);

    if (flags < 0)
        return 0;

    if (fcntl(fd, F_DUPFD, dst) < 0)
        return -1;

    /* According to fcntl(2) on FreeBSD, the close-on-exec flag is reset
     * by F_DUPFD:
     *
     * The close-on-exec flag FD_CLOEXEC associated
     * with the new file descriptor is cleared, so the
     * file descriptor is to remain open across
     * execve(2) system calls.
     *
     * Restore the flag if it was set:
     *
     */
    return fcntl(dst, F_SETFD, flags);
}

    static void
usage(alcove_state_t *ap)
{
    (void)fprintf(stderr, "%s %s\n",
            __progname, ALCOVE_VERSION);
    (void)fprintf(stderr,
            "usage: %s -c <path> [<options>]\n"
            "   -m <num>        max children\n"
            "   -v              verbose mode\n",
            __progname
            );

    exit (EXIT_FAILURE);
}
