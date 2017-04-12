CC = g++
LDFLAGS = -l json-c

all : server.exe

server.exe: server.o
	$(CC) -o server.exe server.o $(LDFLAGS)

server.o: server.cpp
	$(CC) -c server.cpp $(LDFLAGS)

clean:
	rm server.o server.exe

