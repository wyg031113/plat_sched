all:testA testB
#CC=arm-linux-gcc
CC=gcc
CFLAGS=-g3
LIBS=-lpthread
CORE_OBJS=circle_buf.o pres_tcp_client.o pres_tcp_server.o sig_voice.o
TESTA_OBJS=$(CORE_OBJS) testA.o pres_demo.o tcp_server.o spd_client.o
TESTB_OBJS=$(CORE_OBJS) testB.o web_demo.o tcp_client.o spd_server.o

testA:$(TESTA_OBJS)
	$(CC) $(CFLAGS) $(TESTA_OBJS) $(LIBS) -o $@
testB:$(TESTB_OBJS)
	$(CC) $(CFLAGS) $(TESTB_OBJS) $(LIBS) -o $@
%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf main $(OBJS) $(PRES_OBJS) pres_demo web_demo tcp_server tcp_client *.o spd_client spd_server
