/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, int no_body);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, int no_body);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

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
      doit(connfd);
      Close(connfd);
      FD_CLR(connfd, &p->read_set);
      p->clientfd[i] = -1;
    }
  }
}

int main(int argc, char* argv[]) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  static pool pool;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  init_pool(listenfd, &pool);
  while (1) {
    /* Wait for listening/connected descriptor(s) to become ready */
    pool.ready_set = pool.read_set;
    pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

    // clientlen = sizeof(clientaddr);
    // connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    // Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    // printf("Accepted connection from (%s, %s)\n", hostname, port);

    /* If listening descriptor ready, add new client to pool */
    if(FD_ISSET(listenfd, &pool.ready_set)) {
      clientlen = sizeof(clientlen);
      connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
      Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
      printf("Accepted connection from (%s, %s)\n", hostname, port);
      add_client(connfd, &pool);
    }

    /* Echo a text line from each ready connected descriptor */
    check_clients(&pool);

    // if (Fork() == 0) {
    //   Close(listenfd);
    //   doit(connfd);
    //   printf("Disconnected from (%s, %s)\n", hostname, port);
    //   Close(connfd);
    //   exit(0);
    // }
    // Close(connfd);  // line:netp:tiny:close
  }
  exit(0);
}

void doit(int fd) {
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE];
  char method[MAXLINE];
  char uri[MAXLINE];
  char version[MAXLINE];
  char filename[MAXLINE];
  char cgiargs[MAXLINE];
  rio_t rio;
  int no_body = 0;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  Rio_readlineb(&rio, buf, MAXLINE);
  printf("Request headers:\n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET") == 0) {
    no_body = 0;
  } else if (strcasecmp(method, "HEAD") == 0) {
    no_body = 1;
  } else {
    clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) { /* Serve static content */
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, no_body);
  } else { /* Serve dynamic content */
    if(!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs, no_body);
  }
}

void clienterror(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg) {
  char buf[MAXLINE];
  char body[MAXLINE];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body + strlen(body), "<body bgcolor=""ffffff"">\r\n");
  sprintf(body + strlen(body), "%s: %s\r\n", errnum, shortmsg);
  sprintf(body + strlen(body), "<p>%s: %s\r\n", longmsg, cause);
  sprintf(body + strlen(body), "<hr><em>The Tiny Web server</em>\r\n");

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  sprintf(buf + strlen(buf), "Content-type: text/html\r\n");
  sprintf(buf + strlen(buf), "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t* rp) {
  char buf[MAXLINE];

  while(Rio_readlineb(rp, buf, MAXLINE) > 0) {
    if (strcmp(buf, "\r\n") == 0) {
      break;
    }
    printf("%s", buf);
  }
}

int parse_uri(char* uri, char* filename, char* cgiargs) {
  char* ptr;

  if(!strstr(uri, "cgi-bin")) { /* Static content */
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/') {
      strcat(filename, "home.html");
    }
    return 1;
  } else { /* Dynamic content */
    ptr = index(uri, '?');
    if (ptr) {
      strcpy(cgiargs, ptr+1);
      *ptr = '\0';
    } else {
      strcpy(cgiargs, "");
    }
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

void serve_static(int fd, char* filename, int filesize, int no_body) {
  int srcfd;
  char* srcp;
  char filetype[MAXLINE];
  char buf[MAXBUF];

  /* Send response headers to client */
  get_filetype(filename, filetype);

  int len = snprintf(buf, MAXBUF,
                       "HTTP/1.0 200 OK\r\n"
                       "Server: Tiny Web Server\r\n"
                       "Connection: close\r\n"
                       "Content-length: %d\r\n"
                       "Content-type: %s\r\n\r\n",
                       filesize, filetype);

  if (len >= MAXBUF) {
    fprintf(stderr, "Error: Response headers truncated.\n");
    return;
  }

  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  if (no_body) {
    return;
  }

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  // if (srcfd < 0) {
  //   perror("Open failed");
  //   return;
  // }

  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);

  srcp = (char *)Malloc(filesize);
  if (Rio_readn(srcfd, srcp, filesize) < 0) {
    perror("Read failed");
    Free(srcp);
    Close(srcfd);
    return;
  }

  // Rio_readn(srcfd, srcp, filesize); // Malloc 사용 시 추가


  Close(srcfd);
  Rio_writen(fd, srcp, filesize);

  // Munmap(srcp, filesize); // Mmap 사용 시
  Free(srcp); // Malloc 사용 시
}

/*
 * get_filetype - Derive file type from filename
 */
void get_filetype(char* filename, char* filetype) {
  if (strstr(filename, ".html")) {
    strcpy(filetype, "text/html");
  } else if (strstr(filename, ".gif")) {
    strcpy(filetype, "image/gif");
  } else if (strstr(filename, ".png")) {
    strcpy(filetype, "image/png");
  } else if (strstr(filename, ".jpg")) {
    strcpy(filetype, "image/jpeg");
  } else if (strstr(filename, ".mp4")) {
    strcpy(filetype, "video/mp4");
  } else {
    strcpy(filetype, "text/plain");
  }
}

void serve_dynamic(int fd, char* filename, char* cgiargs, int no_body) {
  char buf[MAXLINE];
  char* emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (no_body) {
    return;
  }

  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL); /* Parent waits for and reaps child */
}