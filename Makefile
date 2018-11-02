all: recvfile sendfile

recvfile: src/definitions.h src/definitions.cpp src/recvfile.cpp
	g++ -std=c++11 -pthread src/definitions.cpp src/recvfile.cpp -o recvfile

sendfile: src/definitions.h src/definitions.cpp src/sendfile.cpp
	g++ -std=c++11 -pthread src/definitions.cpp src/sendfile.cpp -o sendfile

clean: recvfile sendfile
	rm -f recvfile sendfile