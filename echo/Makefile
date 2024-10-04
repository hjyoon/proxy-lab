CC = gcc
CFLAGS = -O2 -Wall

all: echoservert-pre echoservert echoservers select echoserverp echoserveri echoclient

echoservert-pre: echoservert-pre.c sbuf.o echo-cnt.o csapp.o
	$(CC) $(CFLAGS) -o echoservert-pre echoservert-pre.c sbuf.o echo-cnt.o csapp.o

echoservert: echoservert.c echo.o csapp.o
	$(CC) $(CFLAGS) -o echoservert echoservert.c echo.o csapp.o

echoservers: echoservers.c echo.o csapp.o
	$(CC) $(CFLAGS) -o echoservers echoservers.c echo.o csapp.o

select: select.c echo.o csapp.o
	$(CC) $(CFLAGS) -o select select.c echo.o csapp.o

echoserverp: echoserverp.c echo.o csapp.o
	$(CC) $(CFLAGS) -o echoserverp echoserverp.c echo.o csapp.o

echoserveri: echoserveri.c echo.o csapp.o
	$(CC) $(CFLAGS) -o echoserveri echoserveri.c echo.o csapp.o

echoclient: echoclient.c csapp.o
	$(CC) $(CFLAGS) -o echoclient echoclient.c csapp.o

echo.o: echo.c
	$(CC) $(CFLAGS) -o echo.o -c echo.c

echo-cnt.o: echo-cnt.c
	$(CC) $(CFLAGS) -o echo-cnt.o -c echo-cnt.c

sbuf.o: sbuf.c
	$(CC) $(CFLAGS) -o sbuf.o -c sbuf.c

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

clean:
	rm -f *.o echoservert-pre echoservert echoservers select echoserverp echoserveri echoclient