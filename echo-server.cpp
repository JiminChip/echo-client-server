#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef __linux__
#include <arpa/inet.h>
#include <sys/socket.h>
#endif // __linux
#ifdef WIN32
#include <winsock2.h>
#include "mingw_net.h"
#endif // WIN32
#include <thread>
#include <mutex>
#include <vector>
#include <algorithm>

#ifdef WIN32
void myerror(const char* msg) { fprintf(stderr, "%s %lu\n", msg, GetLastError()); }
#else
void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
#endif

#define SOCKET_MAX 1000
#define CLIENT_MAX 1000

int socket_cnt = 0;
std::vector<int> sock;
std::mutex m;

void usage() {
	printf("tcp server\n");
	printf("syntax: ts <port> [-e[-b]]\n");
	printf("  -e : echo\n");
	printf("  -b : broadcast\n");
	printf("sample: ts 1234\n");
}

struct Param {
	bool echo{false};
	bool broad{false};
	uint16_t port{0};

	bool parse(int argc, char* argv[]) {
		for (int i = 1; i < argc;) {
			if (strcmp(argv[i], "-e") == 0) {
				echo = true;
				i++;
				continue;
			}

			if (strcmp(argv[i], "-b") == 0) {
				broad = true;
				i++;
				continue;
			}

			if (i < argc) port = atoi(argv[i++]);
		}
		return port != 0;
	}
} param;

void recvThread(int sd) {
	printf("connected\n");
	fflush(stdout);
	static const int BUFSIZE = 65536;
	char buf[BUFSIZE];
	while (true) {
		ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
		if (res == 0 || res == -1) {
			fprintf(stderr, "recv return %zd", res);
			myerror(" ");
			break;
		}
		buf[res] = '\0';
		printf("%s", buf);
		fflush(stdout);
		if (param.echo) {
			if (param.broad) {
				m.lock();
				bool break_flag{false};
				for (int i = 0; i < socket_cnt; i++) {
					res = ::send(sock[i], buf, res, 0);
					if (res == 0 || res == -1) {
						fprintf(stderr, "send return %zd", res);
						myerror(" ");
						break_flag = true;
						break;
					}
				}
				m.unlock();
				if (break_flag)
					break;
			}
			else {
				res = ::send(sd, buf, res, 0);
				if (res == 0 || res == -1) {
					fprintf(stderr, "send return %zd", res);
					myerror(" ");
					break;
				}
			}
		}
	}
	m.lock();
	auto newEnd = std::remove_if(sock.begin(), sock.end(), [sd](int value) {
        return value == sd;
    });
	sock.erase(newEnd, sock.end());
	socket_cnt--;
	m.unlock();
	fflush(stdout);
	::close(sd);
	printf("disconnected\n");
}

int main(int argc, char* argv[]) {
	if (!param.parse(argc, argv)) {
		usage();
		return -1;
	}

#ifdef WIN32
	WSAData wsaData;
	WSAStartup(0x0202, &wsaData);
#endif // WIN32

	//
	// socket
	//
	int sd = ::socket(AF_INET, SOCK_STREAM, 0);
	if (sd == -1) {
		myerror("socket");
		return -1;
	}

#ifdef __linux__
	//
	// setsockopt
	//
	{
		int optval = 1;
		int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
		if (res == -1) {
			myerror("setsockopt");
			return -1;
		}
	}
#endif // __linux

	//
	// bind
	//
	{
		struct sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(param.port);

		ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
		if (res == -1) {
			myerror("bind");
			return -1;
		}
	}

	//
	// listen
	//
	{
		int res = listen(sd, 5);
		if (res == -1) {
			myerror("listen");
			return -1;
		}
	}

	while (true) {
		struct sockaddr_in addr;
		socklen_t len = sizeof(addr);
		int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
		if (newsd == -1) {
			myerror("accept");
			break;
		}

		m.lock();
		socket_cnt++;
		sock.push_back(newsd);
		m.unlock();
		

		std::thread* t = new std::thread(recvThread, newsd);
		t->detach();
	}
	::close(sd);
}
