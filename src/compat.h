// copyright (c) 2009-2010 satoshi nakamoto
// copyright (c) 2009-2014 the moorecoin core developers
// distributed under the mit software license, see the accompanying
// file copying or http://www.opensource.org/licenses/mit-license.php.

#ifndef moorecoin_compat_h
#define moorecoin_compat_h

#if defined(have_config_h)
#include "config/moorecoin-config.h"
#endif

#ifdef win32
#ifdef _win32_winnt
#undef _win32_winnt
#endif
#define _win32_winnt 0x0501
#ifndef win32_lean_and_mean
#define win32_lean_and_mean 1
#endif
#ifndef nominmax
#define nominmax
#endif
#ifdef fd_setsize
#undef fd_setsize // prevent redefinition compiler warning
#endif
#define fd_setsize 1024 // max number of fds in fd_set

#include <winsock2.h>     // must be included before mswsock.h and windows.h

#include <mswsock.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <limits.h>
#include <netdb.h>
#include <unistd.h>
#endif

#ifdef win32
#define msg_dontwait        0
#else
typedef u_int socket;
#include "errno.h"
#define wsagetlasterror()   errno
#define wsaeinval           einval
#define wsaealready         ealready
#define wsaewouldblock      ewouldblock
#define wsaemsgsize         emsgsize
#define wsaeintr            eintr
#define wsaeinprogress      einprogress
#define wsaeaddrinuse       eaddrinuse
#define wsaenotsock         ebadf
#define invalid_socket      (socket)(~0)
#define socket_error        -1
#endif

#ifdef win32
#ifndef s_irusr
#define s_irusr             0400
#define s_iwusr             0200
#endif
#else
#define max_path            1024
#endif

// as solaris does not have the msg_nosignal flag for send(2) syscall, it is defined as 0
#if !defined(have_msg_nosignal) && !defined(msg_nosignal)
#define msg_nosignal 0
#endif

#ifndef win32
// prio_max is not defined on solaris
#ifndef prio_max
#define prio_max 20
#endif
#define thread_priority_lowest          prio_max
#define thread_priority_below_normal    2
#define thread_priority_normal          0
#define thread_priority_above_normal    (-2)
#endif

#if have_decl_strnlen == 0
size_t strnlen( const char *start, size_t max_len);
#endif // have_decl_strnlen

#endif // moorecoin_compat_h
