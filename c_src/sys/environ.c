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
#include "alcove.h"
#include "alcove_call.h"

extern char **environ;

/*
 * environ(7)
 *
 */
    ssize_t
alcove_sys_environ(alcove_state_t *ap, const char *arg, size_t len,
        char *reply, size_t rlen)
{
    int rindex = 0;
    char **envp = environ;

    UNUSED(ap);
    UNUSED(arg);
    UNUSED(len);

    ALCOVE_ERR(alcove_encode_version(reply, rlen, &rindex));

    for ( ; envp && *envp; envp++) {
        ALCOVE_ERR(alcove_encode_list_header(reply, rlen, &rindex, 1));
        ALCOVE_ERR(alcove_encode_binary(reply, rlen, &rindex, *envp, strlen(*envp)));
    }

    ALCOVE_ERR(alcove_encode_empty_list(reply, rlen, &rindex));

    return rindex;
}
