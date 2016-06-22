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

    void
alcove_sig_info(int sig, siginfo_t *info, void *context)
{
    UNUSED(context);

    if (write(ALCOVE_SIGWRITE_FILENO, info, sizeof(siginfo_t)) != sizeof(siginfo_t))
        (void)close(ALCOVE_SIGWRITE_FILENO);
}

    void
alcove_sig_dfl(int sig, siginfo_t *info, void *context)
{
    UNUSED(context);

    if (write(ALCOVE_SIGWRITE_FILENO, info, sizeof(siginfo_t)) != sizeof(siginfo_t))
        (void)close(ALCOVE_SIGWRITE_FILENO);
}
