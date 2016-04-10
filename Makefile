all:main pres_demo web_demo
CC=gcc
CFLAGS=-g3
LIBS=-lpthread
OBJS=main.o sig_voice.o circle_buf.o pres_tcp_client.o pres_tcp_server.o
PRES_OBJS=sig_voice.o circle_buf.o pres_demo.o
WEB_OBJS=sig_voice.o circle_buf.o web_demo.o
main:$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $@
pres_demo:$(PRES_OBJS)
	$(CC) $(CFLAGS) $(PRES_OBJS) $(LIBS) -o $@
web_demo:$(WEB_OBJS)
	$(CC) $(CFLAGS) $(WEB_OBJS) $(LIBS) -o $@
%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf main $(OBJS) $(PRES_OBJS) pres_demo web_demo
