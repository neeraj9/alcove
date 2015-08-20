/* Copyright (c) 2015, Michael Santos <michael.santos@gmail.com>
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
#include "sys/ioctl.h"

typedef struct {
    u_char type;
    int arg;
    char data[MAXMSGLEN];
    size_t len;
} alcove_ioctl_arg_t;

enum {
    ALCOVE_IOARG_INT,
    ALCOVE_IOARG_CSTRUCT,
    ALCOVE_IOARG_BINARY
};

#define IOARG(x) (((x).type) ? (x).data : (char *)(x).arg)

/*
 * ioctl(2)
 *
 */
    ssize_t
alcove_sys_ioctl(alcove_state_t *ap, const char *arg, size_t len,
        char *reply, size_t rlen)
{
    int index = 0;
    int rindex = 0;
    int type = 0;
    int arity = 0;
    int d = 0;
    int request = 0;
    alcove_ioctl_arg_t argp = {0};
    alcove_alloc_t *elem = NULL;
    ssize_t nelem = 0;

    int rv = 0;

    /* file descriptor */
    if (alcove_decode_int(arg, len, &index, &d) < 0)
        return -1;

    /* request */
    if (alcove_decode_int(arg, len, &index, &request) < 0)
        return -1;

    /* argp */
    if (alcove_get_type(arg, len, &index, &type, &arity) < 0)
        return -1;

    switch (type) {
        case ERL_SMALL_INTEGER_EXT:
        case ERL_INTEGER_EXT:
            argp.type = ALCOVE_IOARG_INT;
            if (alcove_decode_int(arg, len, &index, &argp.arg) < 0)
                return -1;

            break;

        case ERL_LIST_EXT:
            argp.type = ALCOVE_IOARG_CSTRUCT;
            argp.len = sizeof(argp.data);
            if (alcove_decode_list_to_buf(arg, len, &index, argp.data,
                &(argp.len), &elem, &nelem) < 0)
                return -1;

            break;

        case ERL_BINARY_EXT:
            argp.type = ALCOVE_IOARG_BINARY;
            if (arity > sizeof(argp.data))
                return -1;
            if (alcove_decode_binary(arg, len, &index, argp.data, &argp.len) < 0)
                return -1;

            break;

        default:
            return -1;
    }

    rv = ioctl(d, request, IOARG(argp));

    if (rv < 0)
        return alcove_mk_errno(reply, rlen, errno);

    ALCOVE_ERR(alcove_encode_version(reply, rlen, &rindex));
    ALCOVE_ERR(alcove_encode_tuple_header(reply, rlen, &rindex, 2));
    ALCOVE_ERR(alcove_encode_atom(reply, rlen, &rindex, "ok"));

    switch (argp.type) {
        case ALCOVE_IOARG_CSTRUCT:
            ALCOVE_ERR(alcove_encode_buf_to_list(reply, rlen, &rindex,
                        argp.data, argp.len, elem, nelem));
            break;
        case ALCOVE_IOARG_INT: /* return an empty binary */
        case ALCOVE_IOARG_BINARY:
            ALCOVE_ERR(alcove_encode_binary(reply, rlen, &rindex, argp.data,
                        argp.len));
            break;
        default:
            return -1;
    }

    return rindex;
}