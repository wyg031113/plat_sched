all:main
CC=gcc
CFLAGS=-g
LIBS=-lpthread
OBJS=main.o sig_voice.o
main:$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $@
%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf main $(OBJS)
