#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define BAIL(...) do { syslog(LOG_ERR, __VA_ARGS__); status = -1; goto exit; } while (0)
#define BAIL_IF(cond, ...) do { if (cond) { BAIL(__VA_ARGS__); } } while (0)

#define INIT_BUF_SIZE 256
#define DATAFILE "/var/tmp/aesdsocketdata"

static bool do_shutdown = false;

static void handle_signal(int sig) {
    (void)sig;
    do_shutdown = 1;
}

/// transmit full data file to client
static int send_datafile(int clientfd) {
    int status = 0;
    
    int readfd = open(DATAFILE, O_RDONLY);
    BAIL_IF(readfd < 0, "open error: %s\n", strerror(errno));

    char buf[1024] = { 0 };
    while (1) {
        ssize_t bytes_read = read(readfd, buf, sizeof(buf));
        BAIL_IF(bytes_read < 0, "read error: %s\n", strerror(errno));
        if (bytes_read == 0)
            break;
        
        ssize_t total_sent = 0;
        while (total_sent < bytes_read) {
            ssize_t bytes_sent = send(clientfd, buf, bytes_read, 0);
            BAIL_IF(bytes_sent < 0, "send error: %s\n", strerror(errno));
            total_sent += bytes_sent;
        }
    }

exit:
    if (readfd > 0)
        close(readfd);
    return status;
}

/// read from client until newline, append to data file
static int receive_packet(int clientfd, int datafd) {
    int status = 0;

    size_t capacity = INIT_BUF_SIZE;
    char* buf = malloc(capacity);
    BAIL_IF(buf == NULL, "out of memory");
    size_t i = 0;

    // read packet until newline
    ssize_t bytes_read;
    char c;
    while ((bytes_read = recv(clientfd, &c, sizeof(c), 0)) >= 0) {
        if (bytes_read == 0)
            continue; // TODO potential for disconnect
        
        if (i >= capacity) {
            char* newbuf = realloc(buf, capacity * 2);
            BAIL_IF(newbuf == NULL, "out of memory"); // TODO should ignore line on out-of-memory
            buf = newbuf;
            capacity *= 2;
        }

        buf[i++] = c; // include newline
        if (c == '\n')
            break;

    }
    BAIL_IF(bytes_read < 0, "recv error: %s\n", strerror(errno));

    // write to data file
    size_t total = 0;
    while (total < i) {
        ssize_t bytes_written = write(datafd, buf + total, i - total);
        BAIL_IF(bytes_written < 0, "write error: %s\n", strerror(errno));
        total += bytes_written;
    }
    fsync(datafd); // flush to disk

exit:
    if (buf != NULL)
        free(buf);
    return status;
}

/// accept new client connection, read packet, append to data file,
/// respond with full data file
static int accept_connection(int sockfd, int datafd) {
    int clientfd = -1;

    int status = listen(sockfd, 20);
    BAIL_IF(status != 0, "listen error: %s\n", strerror(errno));

    struct sockaddr clientaddr = { 0 };
    socklen_t clientaddr_size = sizeof(clientaddr);
    clientfd = accept(sockfd, &clientaddr, &clientaddr_size);
    BAIL_IF(clientfd < 0, "accept error: %s\n", strerror(errno));

    char ip[256] = { 0 };
    if (inet_ntop(clientaddr.sa_family, &clientaddr, ip, (socklen_t)sizeof(ip)) == NULL)
        BAIL("failure reading IP: %s\n", strerror(errno));
    syslog(LOG_INFO, "Accepted connection from %s\n", ip);

    status = receive_packet(clientfd, datafd);
    if (status != 0)
        goto exit;

    status = send_datafile(clientfd);
    if (status != 0)
        goto exit;

    syslog(LOG_INFO, "Closed connection from %s\n", ip);

exit:
    if (clientfd != -1)
        close(clientfd);
    return status;
}

int main(int argc, const char* argv[]) {
    int status = 0;
    int datafd = -1;

    struct addrinfo hints = { 0 };
    struct addrinfo* servinfo = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    openlog("aesdsocket", LOG_PERROR, LOG_USER);

    // register signal handler for clean shutdown on SIGINT and SIGTERM
    sigaction(SIGINT, &(struct sigaction){ .sa_handler = handle_signal }, NULL);
    sigaction(SIGTERM, &(struct sigaction){ .sa_handler = handle_signal }, NULL);

    // open socket and bind to port 9000
    status = getaddrinfo(NULL, "9000", &hints, &servinfo);
    BAIL_IF(status != 0, "getaddrinfo error: %s\n", gai_strerror(status));

    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    BAIL_IF(sockfd < 0, "socket error: %s\n", strerror(errno));
    
    int yes = 1;
    status = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    BAIL_IF(status != 0, "setsockopt error: %s\n", strerror(errno));
    
    status = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    BAIL_IF(status != 0, "bind error: %s\n", strerror(errno));

    // fork daemon if requested
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        int child = fork();
        BAIL_IF(child == -1, "fork error: %s\n", strerror(errno));
        if (child != 0) {
            syslog(LOG_INFO, "Forked daemon process %d. Exitting\n", child);
            goto exit;
        }
    }
    
    // open data file for writing
    datafd = open(
        DATAFILE,
        O_CREAT | O_APPEND | O_WRONLY,
        S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    BAIL_IF(datafd < 0, "open error: %s\n", strerror(errno));
    
    while (!do_shutdown) {
        status = accept_connection(sockfd, datafd);
        if (status < 0)
            goto exit;
    }

exit:
    if (datafd != -1) {
        close(datafd);
        unlink(DATAFILE);
    }
    if (sockfd != -1)
        close(sockfd);
    if (servinfo != NULL)
        freeaddrinfo(servinfo);
    return status;
}
