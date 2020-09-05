CC = gcc
CCFLAGS = -Wall -Wpedantic -Wextra -std=gnu99 -g

BINARIES = server client

all: $(BINARIES)

server: util.c protocol.c pgmread.c servapp.c server.c
	$(CC) $^ $(CCFLAGS) -o $@

client: util.c protocol.c send_packet.c client.c
	$(CC) $^ $(CCFLAGS) -o $@

clean:
	rm -f $(BINARIES)
