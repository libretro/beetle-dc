#ifndef _NET_PLATFORM_H
#define _NET_PLATFORM_H

#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_LIBNX
#include <switch.h>
#define INADDR_NONE 0xffffffff
#define INET_ADDRSTRLEN sizeof(struct sockaddr_in)
#define SOL_TCP 6 // Shrug
#elif defined(VITA)
#include <vitasdk.h>
#define SOL_TCP 6 // Shrug
#define inet_ntop sceNetInetNtop
#else
#include <netinet/ip.h>
#endif // HAVE_LIBNX

#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>
#else
#include <ws2tcpip.h>
#endif

#ifndef _WIN32
#define closesocket close
typedef int sock_t;
#define VALID(s) ((s) >= 0)
#define L_EWOULDBLOCK EWOULDBLOCK
#define L_EAGAIN EAGAIN
#define get_last_error() (errno)
#define INVALID_SOCKET (-1)
#define perror(s) do { INFO_LOG(MODEM, "%s: %s", (s) != NULL ? (s) : "", strerror(get_last_error())); } while (false)
#else
typedef SOCKET sock_t;
#define VALID(s) ((s) != INVALID_SOCKET)
#define L_EWOULDBLOCK WSAEWOULDBLOCK
#define L_EAGAIN WSAEWOULDBLOCK
#define get_last_error() (WSAGetLastError())
#define perror(s) do { INFO_LOG(MODEM, "%s: Winsock error: %d\n", (s) != NULL ? (s) : "", WSAGetLastError()); } while (false)
#define SHUT_WR SD_SEND
#define SHUT_RD SD_RECEIVE
#define SHUT_RDWR SD_BOTH
#endif

static inline void set_non_blocking(sock_t fd)
{
#ifndef _WIN32
	fcntl(fd, F_SETFL, O_NONBLOCK);
#else
	u_long optl = 1;
	ioctlsocket(fd, FIONBIO, &optl);
#endif
}

static inline void set_tcp_nodelay(sock_t fd)
{
	int optval = 1;
	socklen_t optlen = sizeof(optval);
#if defined(_WIN32)
	struct protoent *tcp_proto = getprotobyname("TCP");
	setsockopt(fd, tcp_proto->p_proto, TCP_NODELAY, (const char *)&optval, optlen);
#elif !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__NetBSD__)
	setsockopt(fd, SOL_TCP, TCP_NODELAY, (const void *)&optval, optlen);
#else
	struct protoent *tcp_proto = getprotobyname("TCP");
	setsockopt(fd, tcp_proto->p_proto, TCP_NODELAY, &optval, optlen);
#endif
}

static inline bool set_recv_timeout(sock_t fd, int delayms)
{
#ifdef _WIN32
    const DWORD dwDelay = delayms;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&dwDelay, sizeof(DWORD)) == 0;
#else
    struct timeval tv;
    tv.tv_sec = delayms / 1000;
    tv.tv_usec = (delayms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
#endif
}

#if defined(_WIN32) && _WIN32_WINNT < 0x0600
static inline const char *inet_ntop(int af, const void* src, char* dst, int cnt)
{
    struct sockaddr_in srcaddr;

    memset(&srcaddr, 0, sizeof(struct sockaddr_in));
    memcpy(&srcaddr.sin_addr, src, sizeof(srcaddr.sin_addr));

    srcaddr.sin_family = af;
    if (WSAAddressToString((struct sockaddr *)&srcaddr, sizeof(struct sockaddr_in), 0, dst, (LPDWORD)&cnt) != 0)
        return nullptr;
    else
    	return dst;
}
#endif

#endif
