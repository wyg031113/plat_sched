all:main pres_demo web_demo tcp_server tcp_client spd_client spd_server
#CC=arm-linux-gcc
CC=gcc
CFLAGS=-g3
LIBS=-lpthread
OBJS=main.o sig_voice.o circle_buf.o pres_tcp_client.o pres_tcp_server.o
PRES_OBJS=sig_voice.o circle_buf.o pres_demo.o
WEB_OBJS=sig_voice.o circle_buf.o web_demo.o
TCP_SERVER_OBJS=tcp_server.o pres_tcp_server.o circle_buf.o sig_voice.o
TCP_CLIENT_OBJS=tcp_client.o

SPD_CLIENT_OBJS=spd_client.o sig_voice.o circle_buf.o pres_tcp_client.o
SPD_SERVER_OBJS=spd_server.o sig_voice.o circle_buf.o pres_tcp_client.o

main:$(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIBS) -o $@
pres_demo:$(PRES_OBJS)
	$(CC) $(CFLAGS) $(PRES_OBJS) $(LIBS) -o $@
web_demo:$(WEB_OBJS)
	$(CC) $(CFLAGS) $(WEB_OBJS) $(LIBS) -o $@
tcp_server:$(TCP_SERVER_OBJS)
	$(CC) $(CFLAGS) $(TCP_SERVER_OBJS) $(LIBS) -o $@
tcp_client:$(TCP_CLIENT_OBJS)
	$(CC) $(CFLAGS) $(TCP_CLIENT_OBJS) $(LIBS) -o $@

spd_client:$(SPD_CLIENT_OBJS)
	$(CC) $(CFLAGS) $(SPD_CLIENT_OBJS) $(LIBS) -o $@
spd_server:$(SPD_SERVER_OBJS)
	$(CC) $(CFLAGS) $(SPD_SERVER_OBJS) $(LIBS) -o $@

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf main $(OBJS) $(PRES_OBJS) pres_demo web_demo tcp_server tcp_client *.o spd_client spd_server
