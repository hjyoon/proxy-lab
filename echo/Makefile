CC = gcc
CFLAGS = -O2 -Wall

all: echoserverp echoserveri echoclient

echoserverp: echoserverp.c echo.o csapp.o
	$(CC) $(CFLAGS) -o echoserverp echoserverp.c echo.o csapp.o

echoserveri: echoserveri.c echo.o csapp.o
	$(CC) $(CFLAGS) -o echoserveri echoserveri.c echo.o csapp.o

echoclient: echoclient.c csapp.o
	$(CC) $(CFLAGS) -o echoclient echoclient.c csapp.o

echo.o: echo.c
	$(CC) $(CFLAGS) -o echo.o -c echo.c

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

clean:
	rm -f *.o echoserveri echoclient