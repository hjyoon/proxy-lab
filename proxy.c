#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* User-Agent header */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

/* Function prototypes */
int parse_uri(const char *uri, char *hostname, char *port, char *path);
void forward_request(int clientfd);
void handle_response(int serverfd, int clientfd);
void send_error(int clientfd, int status, const char *short_msg, const char *long_msg);

typedef struct { /* Represents a pool of connected descriptors */
    int maxfd; /* Largest descriptor in read_set */
    fd_set read_set; /* Set of all active descriptors */
    fd_set ready_set; /* Subset of descriptors ready for reading */
    int nready; /* Number of ready descriptors from select */
    int maxi; /* High water index into client array */
    int clientfd[FD_SETSIZE]; /* Set of active descriptors */
    rio_t clientrio[FD_SETSIZE]; /* Set of active read buffers */
} pool;

void init_pool(int listenfd, pool* p) {
    /* Initially, there are no connected descriptors */
    int i;
    p->maxi = -1;
    for(i=0; i<FD_SETSIZE; i++) {
        p->clientfd[i] = -1;
    }

    /* Initially, listenfd is only member of select read set */
    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool* p) {
    int i;
    p->nready--;
    for(i=0; i<FD_SETSIZE; i++) { /* Find an available slot */
        if(p->clientfd[i] < 0) {
        /* Add connected descriptor to the pool */
        p->clientfd[i] = connfd;
        Rio_readinitb(&p->clientrio[i], connfd);

        /* Add the descriptor to descriptor set */
        FD_SET(connfd, &p->read_set);

        /* Update max descriptor and pool highwater mark */
        if(connfd > p->maxfd) {
            p->maxfd = connfd;
        }
        if(i > p->maxi) {
            p->maxi = i;
        }

        break;
        }
    }

    if(i == FD_SETSIZE) { /* Couldn't find an empty slot */
        app_error("add_client error: Too many clients");
    }
}

void check_clients(pool* p) {
    int i, connfd, n;
    char buf[MAXLINE];
    rio_t rio;

    for(i=0; i<=(p->maxi) && (p->nready>0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        /* If the descriptor is ready, echo a text line from it */
        if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
        p->nready--;
        forward_request(connfd);
        Close(connfd);
        FD_CLR(connfd, &p->read_set);
        p->clientfd[i] = -1;
        }
    }
}

/* Main function: listens for incoming connections and forwards requests */
int main(int argc, char* argv[]) {
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    static pool pool;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    /* Ignore SIGPIPE to prevent server from terminating when writing to a closed socket */
    // Signal(SIGPIPE, SIG_IGN);

    // Signal(SIGCHLD, sigchld_handler);
    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);
    if (listenfd < 0) {
        perror("Open_listenfd failed");
        exit(1);
    }

    while (1) {
        /* Wait for listening/connected descriptor(s) to become ready */
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        /* If listening descriptor ready, add new client to pool */
        if(FD_ISSET(listenfd, &pool.ready_set)) {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
            printf("Accepted connection from (%s, %s)\n", hostname, port);
            add_client(connfd, &pool);
        }

        check_clients(&pool);

        // clientlen = sizeof(clientaddr);
        // connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        // if (connfd < 0) {
        //     perror("Accept failed");
        //     continue;
        // }

        // Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        // printf("Accepted connection from (%s, %s)\n", hostname, port);

        // forward_request(connfd);

        // if (Fork() == 0) {
        //     Close(listenfd);
        //     forward_request(connfd);
        //     printf("Disconnected from (%s, %s)\n", hostname, port);
        //     Close(connfd);
        //     exit(0);
        // }
        // Close(connfd);
    }

    /* Close(listenfd); */ /* Unreachable 코드 */
    return 0;
}

/* Function to parse URI and extract hostname, port, and path */
int parse_uri(const char *uri, char *hostname, char *port, char *path) {
    const char *ptr;
    if (strncasecmp(uri, "http://", 7) != 0) {
        return -1;
    }

    ptr = uri + 7; /* Skip "http://" */

    /* Find the end of the hostname */
    const char *slash = strchr(ptr, '/');
    if (slash) {
        size_t host_len = slash - ptr;
        if (host_len >= MAXLINE) return -1;
        strncpy(hostname, ptr, host_len);
        hostname[host_len] = '\0';
        strncpy(path, slash, MAXLINE - 1);
        path[MAXLINE - 1] = '\0';
    } else {
        strncpy(hostname, ptr, MAXLINE - 1);
        hostname[MAXLINE - 1] = '\0';
        strncpy(path, "/", MAXLINE - 1);
        path[MAXLINE - 1] = '\0';
    }

    /* Check if port is specified */
    char *colon = strchr(hostname, ':');
    if (colon) {
        *colon = '\0';
        strncpy(port, colon + 1, MAXLINE - 1);
        port[MAXLINE - 1] = '\0';
    } else {
        strncpy(port, "80", MAXLINE - 1);
        port[MAXLINE - 1] = '\0';
    }

    return 0;
}

/* Function to send an error response to the client */
void send_error(int clientfd, int status, const char *short_msg, const char *long_msg) {
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    snprintf(body, MAXBUF, "<html><title>%d %s</title>", status, short_msg);
    snprintf(body + strlen(body), MAXBUF - strlen(body), "<body bgcolor=\"ffffff\">\r\n");
    snprintf(body + strlen(body), MAXBUF - strlen(body), "%d %s\r\n", status, short_msg);
    snprintf(body + strlen(body), MAXBUF - strlen(body), "<p>%s\r\n", long_msg);
    snprintf(body + strlen(body), MAXBUF - strlen(body), "</body></html>\r\n");

    /* Print the HTTP response */
    snprintf(buf, MAXLINE, "HTTP/1.0 %d %s\r\n", status, short_msg);
    Rio_writen(clientfd, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-Type: text/html\r\n");
    Rio_writen(clientfd, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-Length: %lu\r\n\r\n", strlen(body));
    Rio_writen(clientfd, buf, strlen(buf));
    Rio_writen(clientfd, body, strlen(body));
}

/* Function to forward the client's request to the target server */
void forward_request(int clientfd) {
    char buf[MAXLINE];
    char method_buf[MAXLINE], uri[MAXLINE], version_buf[MAXLINE];
    char host[MAXLINE], port_num[MAXLINE], path_buf[MAXLINE];
    rio_t rio_client;
    int serverfd;

    /* Initialize rio for client */
    Rio_readinitb(&rio_client, clientfd);

    /* Read request line */
    if (!Rio_readlineb(&rio_client, buf, MAXLINE)) {
        fprintf(stderr, "Failed to read request line\n");
        send_error(clientfd, 400, "Bad Request", "Failed to read request line");
        return;
    }

    /* Parse request line */
    if (sscanf(buf, "%s %s %s", method_buf, uri, version_buf) != 3) {
        fprintf(stderr, "Malformed request line\n");
        send_error(clientfd, 400, "Bad Request", "Malformed request line");
        return;
    }

    /* Only handle GET method */
    if (strcasecmp(method_buf, "GET")) {
        fprintf(stderr, "Unsupported method: %s\n", method_buf);
        send_error(clientfd, 501, "Not Implemented", "Proxy does not implement this method");
        return;
    }

    /* Parse URI to get hostname, port, and path */
    if (parse_uri(uri, host, port_num, path_buf) < 0) {
        fprintf(stderr, "Failed to parse URI: %s\n", uri);
        send_error(clientfd, 400, "Bad Request", "Failed to parse URI");
        return;
    }

    /* Connect to the target server */
    serverfd = Open_clientfd(host, port_num);
    if (serverfd < 0) {
        fprintf(stderr, "Connection to server failed.\n");
        send_error(clientfd, 502, "Bad Gateway", "Failed to connect to server");
        return;
    }

    /* Initialize rio for server */
    rio_t rio_server;
    Rio_readinitb(&rio_server, serverfd);

    /* Write the request line to the server */
    snprintf(buf, MAXLINE, "%s %s %s\r\n", method_buf, path_buf, version_buf);
    Rio_writen(serverfd, buf, strlen(buf));

    /* Forward headers */
    int host_present = 0;
    while (Rio_readlineb(&rio_client, buf, MAXLINE) > 0) {
        /* End of headers */
        if (strcmp(buf, "\r\n") == 0) {
            break;
        }

        /* Skip headers that need to be replaced */
        if (strncasecmp(buf, "User-Agent:", 11) == 0 ||
            strncasecmp(buf, "Connection:", 11) == 0 ||
            strncasecmp(buf, "Proxy-Connection:", 17) == 0) {
            continue;
        }

        /* Check if Host header is present */
        if (strncasecmp(buf, "Host:", 5) == 0) {
            host_present = 1;
        }

        /* Forward other headers */
        Rio_writen(serverfd, buf, strlen(buf));
    }

    /* Add required headers */
    Rio_writen(serverfd, user_agent_hdr, strlen(user_agent_hdr));
    if (!host_present) {
        char host_hdr[MAXLINE];
        snprintf(host_hdr, MAXLINE, "Host: %s\r\n", host);
        Rio_writen(serverfd, host_hdr, strlen(host_hdr));
    }
    Rio_writen(serverfd, "Connection: close\r\n", 19);
    Rio_writen(serverfd, "Proxy-Connection: close\r\n", 25);
    Rio_writen(serverfd, "\r\n", 2); /* End of headers */

    /* Handle the response from the server and send it back to the client */
    handle_response(serverfd, clientfd);

    Close(serverfd);
}

/* Function to handle the server's response and forward it to the client */
void handle_response(int serverfd, int clientfd) {
    rio_t rio;
    char buf[MAXLINE];
    int n;
    int content_length = -1;

    Rio_readinitb(&rio, serverfd);

    /* Forward response headers */
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) > 0) {
        Rio_writen(clientfd, buf, n);

        /* Check for Content-Length header */
        if (strncasecmp(buf, "Content-Length:", 15) == 0) {
            content_length = atoi(buf + 15);
        }

        /* Check for Transfer-Encoding header */
        if (strncasecmp(buf, "Transfer-Encoding:", 18) == 0) {
            // Currently not handling chunked encoding
            // Could set a flag or handle accordingly
        }

        if (strcmp(buf, "\r\n") == 0) {
            break; /* End of headers */
        }
    }

    /* Forward response body */
    if (content_length > 0) {
        int remaining = content_length;
        while (remaining > 0) {
            int to_read = MIN(remaining, MAXLINE);
            n = Rio_readnb(&rio, buf, to_read);
            if (n <= 0) break;
            Rio_writen(clientfd, buf, n);
            remaining -= n;
        }
    } else {
        /* If Content-Length is not present, read until EOF */
        while ((n = Rio_readnb(&rio, buf, MAXLINE)) > 0) {
            Rio_writen(clientfd, buf, n);
        }
    }
}