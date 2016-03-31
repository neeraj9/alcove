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
#include <sys/ioctl.h>

#include <net/if.h>
#if defined(__linux__)
#include <linux/if_tun.h>
#elif defined(__sunos__)
#else
#include <net/if_tun.h>
#endif

static const alcove_constant_t alcove_ioctl_constants[] = {
#ifdef SIOCADDRT
    ALCOVE_CONSTANT(SIOCADDRT),
#endif
#ifdef SIOCDELRT
    ALCOVE_CONSTANT(SIOCDELRT),
#endif
#ifdef SIOCRTMSG
    ALCOVE_CONSTANT(SIOCRTMSG),
#endif
#ifdef SIOCGIFNAME
    ALCOVE_CONSTANT(SIOCGIFNAME),
#endif
#ifdef SIOCSIFLINK
    ALCOVE_CONSTANT(SIOCSIFLINK),
#endif
#ifdef SIOCGIFCONF
    ALCOVE_CONSTANT(SIOCGIFCONF),
#endif
#ifdef SIOCGIFFLAGS
    ALCOVE_CONSTANT(SIOCGIFFLAGS),
#endif
#ifdef SIOCSIFFLAGS
    ALCOVE_CONSTANT(SIOCSIFFLAGS),
#endif
#ifdef SIOCGIFADDR
    ALCOVE_CONSTANT(SIOCGIFADDR),
#endif
#ifdef SIOCSIFADDR
    ALCOVE_CONSTANT(SIOCSIFADDR),
#endif
#ifdef SIOCGIFDSTADDR
    ALCOVE_CONSTANT(SIOCGIFDSTADDR),
#endif
#ifdef SIOCSIFDSTADDR
    ALCOVE_CONSTANT(SIOCSIFDSTADDR),
#endif
#ifdef SIOCGIFBRDADDR
    ALCOVE_CONSTANT(SIOCGIFBRDADDR),
#endif
#ifdef SIOCSIFBRDADDR
    ALCOVE_CONSTANT(SIOCSIFBRDADDR),
#endif
#ifdef SIOCGIFNETMASK
    ALCOVE_CONSTANT(SIOCGIFNETMASK),
#endif
#ifdef SIOCSIFNETMASK
    ALCOVE_CONSTANT(SIOCSIFNETMASK),
#endif
#ifdef SIOCGIFMETRIC
    ALCOVE_CONSTANT(SIOCGIFMETRIC),
#endif
#ifdef SIOCSIFMETRIC
    ALCOVE_CONSTANT(SIOCSIFMETRIC),
#endif
#ifdef SIOCGIFMEM
    ALCOVE_CONSTANT(SIOCGIFMEM),
#endif
#ifdef SIOCSIFMEM
    ALCOVE_CONSTANT(SIOCSIFMEM),
#endif
#ifdef SIOCGIFMTU
    ALCOVE_CONSTANT(SIOCGIFMTU),
#endif
#ifdef SIOCSIFMTU
    ALCOVE_CONSTANT(SIOCSIFMTU),
#endif
#ifdef SIOCSIFNAME
    ALCOVE_CONSTANT(SIOCSIFNAME),
#endif
#ifdef SIOCSIFHWADDR
    ALCOVE_CONSTANT(SIOCSIFHWADDR),
#endif
#ifdef SIOCGIFENCAP
    ALCOVE_CONSTANT(SIOCGIFENCAP),
#endif
#ifdef SIOCSIFENCAP
    ALCOVE_CONSTANT(SIOCSIFENCAP),
#endif
#ifdef SIOCGIFHWADDR
    ALCOVE_CONSTANT(SIOCGIFHWADDR),
#endif
#ifdef SIOCGIFSLAVE
    ALCOVE_CONSTANT(SIOCGIFSLAVE),
#endif
#ifdef SIOCSIFSLAVE
    ALCOVE_CONSTANT(SIOCSIFSLAVE),
#endif
#ifdef SIOCADDMULTI
    ALCOVE_CONSTANT(SIOCADDMULTI),
#endif
#ifdef SIOCDELMULTI
    ALCOVE_CONSTANT(SIOCDELMULTI),
#endif
#ifdef SIOCGIFINDEX
    ALCOVE_CONSTANT(SIOCGIFINDEX),
#endif
#ifdef SIOGIFINDEX
    ALCOVE_CONSTANT(SIOGIFINDEX),
#endif
#ifdef SIOCSIFPFLAGS
    ALCOVE_CONSTANT(SIOCSIFPFLAGS),
#endif
#ifdef SIOCGIFPFLAGS
    ALCOVE_CONSTANT(SIOCGIFPFLAGS),
#endif
#ifdef SIOCDIFADDR
    ALCOVE_CONSTANT(SIOCDIFADDR),
#endif
#ifdef SIOCSIFHWBROADCAST
    ALCOVE_CONSTANT(SIOCSIFHWBROADCAST),
#endif
#ifdef SIOCGIFCOUNT
    ALCOVE_CONSTANT(SIOCGIFCOUNT),
#endif
#ifdef SIOCGIFBR
    ALCOVE_CONSTANT(SIOCGIFBR),
#endif
#ifdef SIOCSIFBR
    ALCOVE_CONSTANT(SIOCSIFBR),
#endif
#ifdef SIOCGIFTXQLEN
    ALCOVE_CONSTANT(SIOCGIFTXQLEN),
#endif
#ifdef SIOCSIFTXQLEN
    ALCOVE_CONSTANT(SIOCSIFTXQLEN),
#endif
#ifdef SIOCDARP
    ALCOVE_CONSTANT(SIOCDARP),
#endif
#ifdef SIOCGARP
    ALCOVE_CONSTANT(SIOCGARP),
#endif
#ifdef SIOCSARP
    ALCOVE_CONSTANT(SIOCSARP),
#endif
#ifdef SIOCDRARP
    ALCOVE_CONSTANT(SIOCDRARP),
#endif
#ifdef SIOCGRARP
    ALCOVE_CONSTANT(SIOCGRARP),
#endif
#ifdef SIOCSRARP
    ALCOVE_CONSTANT(SIOCSRARP),
#endif
#ifdef SIOCGIFMAP
    ALCOVE_CONSTANT(SIOCGIFMAP),
#endif
#ifdef SIOCSIFMAP
    ALCOVE_CONSTANT(SIOCSIFMAP),
#endif
#ifdef SIOCADDDLCI
    ALCOVE_CONSTANT(SIOCADDDLCI),
#endif
#ifdef SIOCDELDLCI
    ALCOVE_CONSTANT(SIOCDELDLCI),
#endif
#ifdef SIOCDEVPRIVATE
    ALCOVE_CONSTANT(SIOCDEVPRIVATE),
#endif
#ifdef SIOCPROTOPRIVATE
    ALCOVE_CONSTANT(SIOCPROTOPRIVATE),
#endif
#ifdef FIOQSIZE
    ALCOVE_CONSTANT(FIOQSIZE),
#endif
#ifdef TCGETS
    ALCOVE_CONSTANT(TCGETS),
#endif
#ifdef TCSETS
    ALCOVE_CONSTANT(TCSETS),
#endif
#ifdef TCSETSW
    ALCOVE_CONSTANT(TCSETSW),
#endif
#ifdef TCSETSF
    ALCOVE_CONSTANT(TCSETSF),
#endif
#ifdef TCGETA
    ALCOVE_CONSTANT(TCGETA),
#endif
#ifdef TCSETA
    ALCOVE_CONSTANT(TCSETA),
#endif
#ifdef TCSETAW
    ALCOVE_CONSTANT(TCSETAW),
#endif
#ifdef TCSETAF
    ALCOVE_CONSTANT(TCSETAF),
#endif
#ifdef TCSBRK
    ALCOVE_CONSTANT(TCSBRK),
#endif
#ifdef TCXONC
    ALCOVE_CONSTANT(TCXONC),
#endif
#ifdef TCFLSH
    ALCOVE_CONSTANT(TCFLSH),
#endif
#ifdef TIOCEXCL
    ALCOVE_CONSTANT(TIOCEXCL),
#endif
#ifdef TIOCNXCL
    ALCOVE_CONSTANT(TIOCNXCL),
#endif
#ifdef TIOCFLUSH
    ALCOVE_CONSTANT(TIOCFLUSH),
#endif
#ifdef TIOCSCTTY
    ALCOVE_CONSTANT(TIOCSCTTY),
#endif
#ifdef TIOCGPGRP
    ALCOVE_CONSTANT(TIOCGPGRP),
#endif
#ifdef TIOCSPGRP
    ALCOVE_CONSTANT(TIOCSPGRP),
#endif
#ifdef TIOCOUTQ
    ALCOVE_CONSTANT(TIOCOUTQ),
#endif
#ifdef TIOCSTI
    ALCOVE_CONSTANT(TIOCSTI),
#endif
#ifdef TIOCGWINSZ
    ALCOVE_CONSTANT(TIOCGWINSZ),
#endif
#ifdef TIOCSWINSZ
    ALCOVE_CONSTANT(TIOCSWINSZ),
#endif
#ifdef TIOCMGET
    ALCOVE_CONSTANT(TIOCMGET),
#endif
#ifdef TIOCMBIS
    ALCOVE_CONSTANT(TIOCMBIS),
#endif
#ifdef TIOCMBIC
    ALCOVE_CONSTANT(TIOCMBIC),
#endif
#ifdef TIOCMSET
    ALCOVE_CONSTANT(TIOCMSET),
#endif
#ifdef TIOCGSOFTCAR
    ALCOVE_CONSTANT(TIOCGSOFTCAR),
#endif
#ifdef TIOCSSOFTCAR
    ALCOVE_CONSTANT(TIOCSSOFTCAR),
#endif
#ifdef FIONREAD
    ALCOVE_CONSTANT(FIONREAD),
#endif
#ifdef TIOCINQ
    ALCOVE_CONSTANT(TIOCINQ),
#endif
#ifdef TIOCLINUX
    ALCOVE_CONSTANT(TIOCLINUX),
#endif
#ifdef TIOCCONS
    ALCOVE_CONSTANT(TIOCCONS),
#endif
#ifdef TIOCGSERIAL
    ALCOVE_CONSTANT(TIOCGSERIAL),
#endif
#ifdef TIOCSSERIAL
    ALCOVE_CONSTANT(TIOCSSERIAL),
#endif
#ifdef TIOCPKT
    ALCOVE_CONSTANT(TIOCPKT),
#endif
#ifdef FIONBIO
    ALCOVE_CONSTANT(FIONBIO),
#endif
#ifdef TIOCNOTTY
    ALCOVE_CONSTANT(TIOCNOTTY),
#endif
#ifdef TIOCSETD
    ALCOVE_CONSTANT(TIOCSETD),
#endif
#ifdef TIOCGETD
    ALCOVE_CONSTANT(TIOCGETD),
#endif
#ifdef TCSBRKP
    ALCOVE_CONSTANT(TCSBRKP),
#endif
#ifdef TIOCSBRK
    ALCOVE_CONSTANT(TIOCSBRK),
#endif
#ifdef TIOCCBRK
    ALCOVE_CONSTANT(TIOCCBRK),
#endif
#ifdef TIOCGSID
    ALCOVE_CONSTANT(TIOCGSID),
#endif
#ifdef TCGETS2
    ALCOVE_CONSTANT(TCGETS2),
#endif
#ifdef TCSETS2
    ALCOVE_CONSTANT(TCSETS2),
#endif
#ifdef TCSETSW2
    ALCOVE_CONSTANT(TCSETSW2),
#endif
#ifdef TCSETSF2
    ALCOVE_CONSTANT(TCSETSF2),
#endif
#ifdef TIOCGRS485
    ALCOVE_CONSTANT(TIOCGRS485),
#endif
#ifdef TIOCSRS485
    ALCOVE_CONSTANT(TIOCSRS485),
#endif
#ifdef TIOCGPTN
    ALCOVE_CONSTANT(TIOCGPTN),
#endif
#ifdef TIOCSPTLCK
    ALCOVE_CONSTANT(TIOCSPTLCK),
#endif
#ifdef TIOCGDEV
    ALCOVE_CONSTANT(TIOCGDEV),
#endif
#ifdef TCGETX
    ALCOVE_CONSTANT(TCGETX),
#endif
#ifdef TCSETX
    ALCOVE_CONSTANT(TCSETX),
#endif
#ifdef TCSETXF
    ALCOVE_CONSTANT(TCSETXF),
#endif
#ifdef TCSETXW
    ALCOVE_CONSTANT(TCSETXW),
#endif
#ifdef TIOCSIG
    ALCOVE_CONSTANT(TIOCSIG),
#endif
#ifdef TIOCVHANGUP
    ALCOVE_CONSTANT(TIOCVHANGUP),
#endif
#ifdef FIONCLEX
    ALCOVE_CONSTANT(FIONCLEX),
#endif
#ifdef FIOCLEX
    ALCOVE_CONSTANT(FIOCLEX),
#endif
#ifdef FIOASYNC
    ALCOVE_CONSTANT(FIOASYNC),
#endif
#ifdef TIOCSERCONFIG
    ALCOVE_CONSTANT(TIOCSERCONFIG),
#endif
#ifdef TIOCSERGWILD
    ALCOVE_CONSTANT(TIOCSERGWILD),
#endif
#ifdef TIOCSERSWILD
    ALCOVE_CONSTANT(TIOCSERSWILD),
#endif
#ifdef TIOCGLCKTRMIOS
    ALCOVE_CONSTANT(TIOCGLCKTRMIOS),
#endif
#ifdef TIOCSLCKTRMIOS
    ALCOVE_CONSTANT(TIOCSLCKTRMIOS),
#endif
#ifdef TIOCSERGSTRUCT
    ALCOVE_CONSTANT(TIOCSERGSTRUCT),
#endif
#ifdef TIOCSERGETLSR
    ALCOVE_CONSTANT(TIOCSERGETLSR),
#endif
#ifdef TIOCSERGETMULTI
    ALCOVE_CONSTANT(TIOCSERGETMULTI),
#endif
#ifdef TIOCSERSETMULTI
    ALCOVE_CONSTANT(TIOCSERSETMULTI),
#endif
#ifdef TIOCMIWAIT
    ALCOVE_CONSTANT(TIOCMIWAIT),
#endif
#ifdef TIOCGICOUNT
    ALCOVE_CONSTANT(TIOCGICOUNT),
#endif
#ifdef TIOCPKT_DATA
    ALCOVE_CONSTANT(TIOCPKT_DATA),
#endif
#ifdef TIOCPKT_FLUSHREAD
    ALCOVE_CONSTANT(TIOCPKT_FLUSHREAD),
#endif
#ifdef TIOCPKT_FLUSHWRITE
    ALCOVE_CONSTANT(TIOCPKT_FLUSHWRITE),
#endif
#ifdef TIOCPKT_STOP
    ALCOVE_CONSTANT(TIOCPKT_STOP),
#endif
#ifdef TIOCPKT_START
    ALCOVE_CONSTANT(TIOCPKT_START),
#endif
#ifdef TIOCPKT_NOSTOP
    ALCOVE_CONSTANT(TIOCPKT_NOSTOP),
#endif
#ifdef TIOCPKT_DOSTOP
    ALCOVE_CONSTANT(TIOCPKT_DOSTOP),
#endif
#ifdef TIOCPKT_IOCTL
    ALCOVE_CONSTANT(TIOCPKT_IOCTL),
#endif
#ifdef TIOCSER_TEMT
    ALCOVE_CONSTANT(TIOCSER_TEMT),
#endif

#ifdef TUNSETNOCSUM
    ALCOVE_CONSTANT(TUNSETNOCSUM),
#endif
#ifdef TUNSETDEBUG
    ALCOVE_CONSTANT(TUNSETDEBUG),
#endif
#ifdef TUNSETIFF
    ALCOVE_CONSTANT(TUNSETIFF),
#endif
#ifdef TUNSETPERSIST
    ALCOVE_CONSTANT(TUNSETPERSIST),
#endif
#ifdef TUNSETOWNER
    ALCOVE_CONSTANT(TUNSETOWNER),
#endif
#ifdef TUNSETLINK
    ALCOVE_CONSTANT(TUNSETLINK),
#endif
#ifdef TUNSETGROUP
    ALCOVE_CONSTANT(TUNSETGROUP),
#endif
#ifdef TUNGETFEATURES
    ALCOVE_CONSTANT(TUNGETFEATURES),
#endif
#ifdef TUNSETOFFLOAD
    ALCOVE_CONSTANT(TUNSETOFFLOAD),
#endif
#ifdef TUNSETTXFILTER
    ALCOVE_CONSTANT(TUNSETTXFILTER),
#endif
#ifdef TUNGETIFF
    ALCOVE_CONSTANT(TUNGETIFF),
#endif
#ifdef TUNGETSNDBUF
    ALCOVE_CONSTANT(TUNGETSNDBUF),
#endif
#ifdef TUNSETSNDBUF
    ALCOVE_CONSTANT(TUNSETSNDBUF),
#endif
#ifdef TUNATTACHFILTER
    ALCOVE_CONSTANT(TUNATTACHFILTER),
#endif
#ifdef TUNDETACHFILTER
    ALCOVE_CONSTANT(TUNDETACHFILTER),
#endif
#ifdef TUNGETVNETHDRSZ
    ALCOVE_CONSTANT(TUNGETVNETHDRSZ),
#endif
#ifdef TUNSETVNETHDRSZ
    ALCOVE_CONSTANT(TUNSETVNETHDRSZ),
#endif

    {NULL, 0}
};
