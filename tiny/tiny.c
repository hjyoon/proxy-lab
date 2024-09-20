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

void sigchld_handler(int sig) {
  while (waitpid(-1, 0, WNOHANG) > 0);
  return;
}

int main(int argc, char* argv[]) {
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGCHLD, sigchld_handler);
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    if (Fork() == 0) {
      Close(listenfd);
      doit(connfd);
      printf("Disconnected from (%s, %s)\n", hostname, port);
      Close(connfd);
      exit(0);
    }
    Close(connfd);  // line:netp:tiny:close
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
  // printf("Request headers:\n");
  // printf("%s", buf);
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
    // printf("%s", buf);
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
  // printf("Response headers:\n");
  // printf("%s", buf);

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

// void serve_static(int fd, char* filename, int filesize, int no_body) {
//     int srcfd;
//     char* srcp;
//     char filetype[MAXLINE];
//     char buf[MAXBUF];

//     /* Send response headers to client */
//     get_filetype(filename, filetype);
//     buf[0] = '\0'; // 버퍼를 빈 문자열로 초기화

//     // "HTTP/1.0 200 OK\r\n" 헤더 추가
//     strncpy(buf, "HTTP/1.0 200 OK\r\n", MAXBUF - 1);
//     buf[MAXBUF - 1] = '\0'; // null 종료 문자 보장

//     // "Server: Tiny Web Server\r\n" 헤더 추가
//     strncat(buf, "Server: Tiny Web Server\r\n", MAXBUF - strlen(buf) - 1);

//     // "Connection: close\r\n" 헤더 추가
//     strncat(buf, "Connection: close\r\n", MAXBUF - strlen(buf) - 1);

//     // "Content-length: <filesize>\r\n" 헤더 추가
//     char content_length[MAXLINE];
//     snprintf(content_length, sizeof(content_length), "Content-length: %d\r\n", filesize);
//     strncat(buf, content_length, MAXBUF - strlen(buf) - 1);

//     // "Content-type: <filetype>\r\n\r\n" 헤더 추가
//     char content_type[MAXLINE];
//     snprintf(content_type, sizeof(content_type), "Content-type: %s\r\n\r\n", filetype);
//     strncat(buf, content_type, MAXBUF - strlen(buf) - 1);

//     // 클라이언트로 헤더 전송
//     Rio_writen(fd, buf, strlen(buf));

//     // 헤더 로그 출력
//     printf("Response headers:\n");
//     printf("%s", buf);

//     if (no_body) {
//         return;
//     }

//     /* Send response body to client */
//     srcfd = Open(filename, O_RDONLY, 0);
//     // 에러 처리 (주석 처리된 부분)
//     // if (srcfd < 0) {
//     //     perror("Open failed");
//     //     return;
//     // }

//     // 파일 내용을 메모리로 읽기
//     srcp = (char *)Malloc(filesize);
//     if (Rio_readn(srcfd, srcp, filesize) < 0) {
//         perror("Read failed");
//         Free(srcp);
//         Close(srcfd);
//         return;
//     }

//     Close(srcfd);

//     // 파일 내용을 클라이언트로 전송
//     Rio_writen(fd, srcp, filesize);

//     // 메모리 해제
//     Free(srcp);
// }

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