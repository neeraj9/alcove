/* Copyright (c) 2014, Michael Santos <michael.santos@gmail.com>
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
alcove_define_t alcove_signal_constants[] = {
#ifdef SIGHUP
    ALCOVE_DEFINE(SIGHUP),
#endif
#ifdef SIGINT
    ALCOVE_DEFINE(SIGINT),
#endif
#ifdef SIGQUIT
    ALCOVE_DEFINE(SIGQUIT),
#endif
#ifdef SIGILL
    ALCOVE_DEFINE(SIGILL),
#endif
#ifdef SIGTRAP
    ALCOVE_DEFINE(SIGTRAP),
#endif
#ifdef SIGABRT
    ALCOVE_DEFINE(SIGABRT),
#endif
#ifdef SIGIOT
    ALCOVE_DEFINE(SIGIOT),
#endif
#ifdef SIGBUS
    ALCOVE_DEFINE(SIGBUS),
#endif
#ifdef SIGFPE
    ALCOVE_DEFINE(SIGFPE),
#endif
#ifdef SIGKILL
    ALCOVE_DEFINE(SIGKILL),
#endif
#ifdef SIGUSR1
    ALCOVE_DEFINE(SIGUSR1),
#endif
#ifdef SIGSEGV
    ALCOVE_DEFINE(SIGSEGV),
#endif
#ifdef SIGUSR2
    ALCOVE_DEFINE(SIGUSR2),
#endif
#ifdef SIGPIPE
    ALCOVE_DEFINE(SIGPIPE),
#endif
#ifdef SIGALRM
    ALCOVE_DEFINE(SIGALRM),
#endif
#ifdef SIGTERM
    ALCOVE_DEFINE(SIGTERM),
#endif
#ifdef SIGSTKFLT
    ALCOVE_DEFINE(SIGSTKFLT),
#endif
#ifdef SIGCHLD
    ALCOVE_DEFINE(SIGCHLD),
#endif
#ifdef SIGCONT
    ALCOVE_DEFINE(SIGCONT),
#endif
#ifdef SIGSTOP
    ALCOVE_DEFINE(SIGSTOP),
#endif
#ifdef SIGTSTP
    ALCOVE_DEFINE(SIGTSTP),
#endif
#ifdef SIGTTIN
    ALCOVE_DEFINE(SIGTTIN),
#endif
#ifdef SIGTTOU
    ALCOVE_DEFINE(SIGTTOU),
#endif
#ifdef SIGURG
    ALCOVE_DEFINE(SIGURG),
#endif
#ifdef SIGXCPU
    ALCOVE_DEFINE(SIGXCPU),
#endif
#ifdef SIGXFSZ
    ALCOVE_DEFINE(SIGXFSZ),
#endif
#ifdef SIGVTALRM
    ALCOVE_DEFINE(SIGVTALRM),
#endif
#ifdef SIGPROF
    ALCOVE_DEFINE(SIGPROF),
#endif
#ifdef SIGWINCH
    ALCOVE_DEFINE(SIGWINCH),
#endif
#ifdef SIGIO
    ALCOVE_DEFINE(SIGIO),
#endif
#ifdef SIGPOLL
    ALCOVE_DEFINE(SIGPOLL),
#endif
#ifdef SIGLOST
    ALCOVE_DEFINE(SIGLOST),
#endif
#ifdef SIGPWR
    ALCOVE_DEFINE(SIGPWR),
#endif
#ifdef SIGSYS
    ALCOVE_DEFINE(SIGSYS),
#endif
#ifdef SIGSTKSZ
    ALCOVE_DEFINE(SIGSTKSZ),
#endif
    {NULL, 0}
};
