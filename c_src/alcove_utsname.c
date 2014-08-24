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

#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX MAXHOSTNAMELEN
#endif

/*
 * gethostname(2)
 *
 */
    ssize_t
alcove_gethostname(alcove_state_t *ap, const char *arg, size_t len,
        char *reply, size_t rlen)
{
    int rindex = 0;

    char name[HOST_NAME_MAX] = {0};
    int rv = 0;

    rv = gethostname(name, HOST_NAME_MAX-1);

    if (rv < 0)
       return alcove_errno(reply, rlen, errno);

    ALCOVE_OK(reply, &rindex,
        alcove_encode_binary(reply, rlen, &rindex, name, strlen(name)));

    return rindex;
}

/*
 * sethostname(2)
 *
 */
    ssize_t
alcove_sethostname(alcove_state_t *ap, const char *arg, size_t len,
        char *reply, size_t rlen)
{
    int index = 0;

    char name[HOST_NAME_MAX] = {0};
    size_t nlen = sizeof(name)-1;
    int rv = 0;
    int errnum = 0;

    /* hostname */
    if (alcove_decode_iolist(arg, len, &index, name, &nlen) < 0 ||
            nlen == 0)
        return -1;

    rv = sethostname(name, strlen(name));

    return (rv < 0)
        ? alcove_errno(reply, rlen, errnum)
        : alcove_mk_atom(reply, rlen, "ok");
}
