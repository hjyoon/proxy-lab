/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"

int main(void) {
  char* buf;
  // char* p;
  char arg1[MAXLINE];
  char arg2[MAXLINE];
  char content[MAXLINE];
  int n1=0;
  int n2=0;

  /* Extract the two arguments */
  buf = getenv("QUERY_STRING");
  if (buf != NULL && strlen(buf) != 0) {
    // p = strchr(buf, '&');
    // *p = '\0';
    // strcpy(arg1, buf);
    // strcpy(arg2, p+1);
    // n1 = atoi(arg1+2);
    // n2 = atoi(arg2+2);

    sscanf(buf, "x=%[^&]&y=%s", arg1, arg2);
    n1 = atoi(arg1);
    n2 = atoi(arg2);
  }

  /* Make the response body */
  sprintf(content, "Welcome to add.com: THE Internet addition portal.");
  sprintf(content + strlen(content), "<p>The answer is: %d + %d = %d</p>", n1, n2, n1 + n2);
  sprintf(content + strlen(content), "<form action=\"/cgi-bin/adder\" method=\"GET\">");
  sprintf(content + strlen(content), "x: <input type=\"text\" name=\"x\"><br>");
  sprintf(content + strlen(content), "y: <input type=\"text\" name=\"y\"><br>");
  sprintf(content + strlen(content), "<input type=\"submit\" value=\"Add\">");
  sprintf(content + strlen(content), "</form>");

  /* Generate the HTTP response */
  printf("Connection: close\r\n");
  printf("Content-length: %d\r\n", (int)strlen(content));
  printf("Content-type: text/html\r\n\r\n");
  printf("%s", content);
  fflush(stdout);

  exit(0);
}
/* $end adder */
