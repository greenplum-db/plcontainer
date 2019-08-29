#include "common/comm_dummy.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>

static char *split_address(const char *address, char *node_service)
{
    char *p;
    int n = strlen(address);
    if (n>=600 || n<5)
        return NULL;
    strcpy(node_service, address);
    p = node_service + n-1;
    while (*p != ':' && p>node_service)
        --p;
    if (*p != ':')
        return NULL;
    *p = '\0';
    return p+1;
}

int ListenTCP(const char *network, const char *address)
{
    if (!network || !address) {
        plc_elog(ERROR, "invalid parameters: network(%s), address(%s)",
                network, address);
        return -1;
    }
    char node_service[600], *node, *service;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int rc, fd=-1;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_V4MAPPED;
    if (strcmp(network, "tcp") == 0) {
        hints.ai_family = AF_UNSPEC;
    } else if (strcmp(network, "tcp4") == 0) {
        hints.ai_family = AF_INET;
    } else if (strcmp(network, "tcp6") == 0) {
        hints.ai_family = AF_INET6;
    } else {
        plc_elog(ERROR, "invalid network:%s\n", network);
        return -1;
    }
    service = split_address(address, node_service);
    if (!service) {
        plc_elog(ERROR, "bad address(%s)\n", address);
        return -1;
    }
    node = node_service[0]=='\0' ? NULL : node_service;

    rc = getaddrinfo(node, service, &hints, &result);
    if (rc != 0) {
        plc_elog(ERROR, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype,
                    rp->ai_protocol);
        if (fd == -1)
            continue;

        if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            if (listen(fd, 128) == 0)
                break;
        }
        close(fd);
    }

    freeaddrinfo(result);           /* No longer needed */
    return fd;
}

int ListenUnix(const char *network, const char *address)
{
    int fd, sotype;
    struct sockaddr_un sockaddr;
    if (!network || !address) {
        plc_elog(ERROR, "null parameters: network(%s), address(%s)", network, address);
        return -1;
    }
    if (strcmp(network, "unix") == 0)
        sotype = SOCK_STREAM;
    else if (strcmp(network, "unixpacket") == 0)
        sotype = SOCK_SEQPACKET;
//    else if (strcmp(network, "unixgram") == 0)
//        socktype = SOCK_DGRAM;
    else {
        plc_elog(ERROR, "unknown network(%s)", network);
        return -1;
    }
    if (strlen(address) >= sizeof(sockaddr.sun_path)) {
        plc_elog(ERROR, "address is too long(%d)", (int)strlen(address));
        return -1;
    }
    fd = socket(AF_UNIX, sotype, 0);
    if (fd == -1) {
        plc_elog(ERROR, "socket failed(%s)", strerror(errno));
        return -1;
    }
    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, address);
    if (bind(fd, (const struct sockaddr*)&sockaddr, (socklen_t) sizeof(sockaddr)) == -1) {
        plc_elog(ERROR, "bind error(%s)", strerror(errno));
        goto out;
    }
    if (listen(fd, 128) == -1) {
        plc_elog(ERROR, "listen error(%s)", strerror(errno));
        goto out;
    }
    return fd;

out:
    close(fd);
    return -1;
}

int plcListenServer(const char *network, const char *address)
{
	if (!network) {
        plc_elog(ERROR, "null network");
        return -1;
    }
    switch(network[0]) {
        case 't': return ListenTCP(network, address);
        case 'u': return ListenUnix(network, address);
        default:
            break;
    }
    return -1;
}

static int DialTCP(const char *network, const char *address)
{
    char node_service[600], *node, *service;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int fd, rc;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_V4MAPPED;
    if (strcmp(network, "tcp") == 0) {
        hints.ai_family = AF_UNSPEC;
    } else if (strcmp(network, "tcp4") == 0) {
        hints.ai_family = AF_INET;
    } else if (strcmp(network, "tcp6") == 0) {
        hints.ai_family = AF_INET6;
    } else {
        plc_elog(ERROR, "invalid network:%s\n", network);
        return -1;
    }
    service = split_address(address, node_service);
    if (!service) {
        plc_elog(ERROR, "bad address(%s)\n", address);
        return -1;
    }
    node = node_service[0]=='\0' ? NULL : node_service;

    rc = getaddrinfo(node, service, &hints, &result);
    if (rc != 0) {
        plc_elog(ERROR, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }
    fd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype,
                     rp->ai_protocol);
        if (fd == -1)
            continue;

        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        close(fd);
    }
    freeaddrinfo(result);           /* No longer needed */

    if (rp == NULL) {               /* No address succeeded */
        plc_elog(ERROR, "Could not bind(%s)\n", strerror(errno));
        return -1;
    }

    return fd;
}

static int DialUnix(const char *network, const char *address)
{
    struct sockaddr_un sockaddr;
    int fd, sotype;
    socklen_t socklen;
    if (!network || !address) {
        plc_elog(ERROR, "null network or address");
        return -1;
    }
    if (strcmp(network, "unix")==0)
        sotype = SOCK_STREAM;
    else if (strcmp(network, "unixgram")==0)
        sotype = SOCK_DGRAM;
    else if (strcmp(network, "unixpacket")==0)
        sotype = SOCK_SEQPACKET;
    else {
        plc_elog(ERROR, "unknown network for unix-socket(%s)", network);
        return -1;
    }
    if (strlen(address) >= sizeof(sockaddr.sun_path)) {
        plc_elog(ERROR, "address(%s) is too long(%d)", address, (int)strlen(address));
        return -1;
    }
    fd = socket(AF_UNIX, sotype, 0);
    if (fd == -1) {
        plc_elog(ERROR, "socket failed:%s", strerror(errno));
        return -1;
    }

    memset(&sockaddr, 0, sizeof(sockaddr));
    sockaddr.sun_family = AF_UNIX;
    strcpy(sockaddr.sun_path, address);
    socklen = sizeof(sockaddr);
    if (connect(fd, (const struct sockaddr*)&sockaddr, socklen)) {
        plc_elog(ERROR, "connect error(%s)", strerror(errno));
        goto out;
    }
    return fd;

out:
    close(fd);
    return -1;
}

// FIXME: is there necessary to set a timeout for plcontainer/container ???
int plcDialToServer(const char *network, const char *address)
{

    if (!network)
        return -1;
    switch(network[0]) {
        case 't': return DialTCP(network, address);
        case 'u': return DialUnix(network, address);
        default:
            break;
    }
    return -1;
}

