#include <stdio.h>
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include "events.h"

#pragma comment(lib, "ws2_32.lib")

#define IDLE_SLEEP_MS 10

#define MAX_CLIENTS 1
#define RECV_BUFFER_SIZE (2 * 1024 * 1024)

const char *server_ip = "x.x.x.x";
const unsigned short server_port = 80;

const char *listen_ip = "127.0.0.1";
const unsigned short listen_port = 80;

SOCKET listening_socket = INVALID_SOCKET;

char recv_buffer[RECV_BUFFER_SIZE];

SOCKET client_list[MAX_CLIENTS];
SOCKET server_list[MAX_CLIENTS];
int client_count = 0;

fd_set readfds;
fd_set writefds;

int should_cleanup = 0;

SOCKET socket_initialize() {
	SOCKET result = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	return result;
}
void sockets_close() {
	for (int socket_index = 0; socket_index < MAX_CLIENTS; socket_index++) {
		if (client_list[socket_index] != INVALID_SOCKET) {
			if (!socket_close(client_list[socket_index])) {
				printf("Couldn't close the client socket (%d).\n", socket_index);
			}
		}
		if (server_list[socket_index] != INVALID_SOCKET) {
			if (!socket_close(server_list[socket_index])) {
				printf("Couldn't close the server socket (%d).\n", socket_index);
			}
		}
	}

	if (!socket_close(listening_socket)) {
		printf("Couldn't close listening socket.\n");
	}
}

int proxy_initialize() {
	for (int socket_index = 0; socket_index < MAX_CLIENTS; socket_index++) {
		client_list[socket_index] = INVALID_SOCKET;
		server_list[socket_index] = INVALID_SOCKET;
	}

	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
		printf("Couldn't initialize winsock.\n");
		return -1;
	}

	listening_socket = socket_initialize();
	if (listening_socket == INVALID_SOCKET) {
		printf("Couldn't initialize socket.\n");
		return -1;
	}

	if (!socket_set_unblocking(listening_socket)) {
		printf("Couldn't change listening socket to unblocking.\n");
		socket_close(listening_socket);
		return -1;
	}

	if (!socket_bind(listening_socket, listen_port, listen_ip)) {
		printf("Couldn't bind socket.\n");
		socket_close(listening_socket);
		return -1;
	}

	if (!socket_listen(listening_socket, MAX_CLIENTS)) {
		printf("Couldn't start listening from socket.\n");
		socket_close(listening_socket);
		return -1;
	}

	return 1;
}
int proxy_cleanup() {
	sockets_close();
	return WSACleanup() == 0;
}

int socket_bind(SOCKET sock, const unsigned short port, const char *ip) {
	if (sock == INVALID_SOCKET || ip == NULL) return 0;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
		printf("Couldn't convert the ip address: %s\n", ip);
		return 0;
	}
	return bind(sock, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != SOCKET_ERROR;
}
int socket_connect(SOCKET sock, const unsigned short port, const char *ip) {
	if (sock == INVALID_SOCKET || ip == NULL) return 0;
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1) {
		printf("Couldn't convert the ip address: %s\n", ip);
		return 0;
	}
	return connect(sock, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in)) != SOCKET_ERROR;
}
int socket_listen(SOCKET sock, const unsigned int max_clients) {
	return listen(sock, max_clients) == 0;
}
int socket_close(SOCKET sock) {
	if (sock == INVALID_SOCKET) return 1;
	return closesocket(sock) == 0;
}
int socket_change_control(SOCKET sock, long cmd, u_long argp) {
	return ioctlsocket(sock, cmd, &argp) == 0;
}
int socket_change_option(SOCKET sock, int level, int optname, const char *optval, int optlen) {
	return setsockopt(sock, level, optname, optval, optlen) == 0;
}
int socket_set_unblocking(SOCKET sock) {
	return socket_change_control(sock, FIONBIO, 1);
}

int socket_send(SOCKET sock, const char *buffer, int length) {
	int total_sent = 0;
	while (total_sent < length) {
		int bytes_sent = send(sock, recv_buffer + total_sent, length - total_sent, 0);
		if (bytes_sent == SOCKET_ERROR) {
			return -1;
		}
		total_sent += bytes_sent;
	}
	return 1;
}

int sockets_check_ready() {
	FD_ZERO(&readfds);
	FD_SET(listening_socket, &readfds);
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (client_list[i] == INVALID_SOCKET || server_list[i] == INVALID_SOCKET) continue;
		FD_SET(client_list[i], &readfds);
		FD_SET(server_list[i], &readfds);
	}
	writefds = readfds;

	struct timeval timeout = { 0, 0 };
	int result = select(0, &readfds, &writefds, NULL, &timeout);
	return result > 0 ? 1 : (result == 0 ? 0 : -1);
}

SOCKET socket_create_connect(const unsigned short port, const char *ip) {
	SOCKET server_socket = socket_initialize();
	if (server_socket == INVALID_SOCKET) {
		printf("Couldn't initialize socket.\n");
		return INVALID_SOCKET;
	}

	if (!socket_connect(server_socket, port, ip)) {
		printf("Couldn't connect with socket.\n");
		socket_close(server_socket);
		return INVALID_SOCKET;
	}

	if (!socket_set_unblocking(server_socket)) {
		printf("Couldn't change server socket to unblocking mode.\n");
		socket_close(server_socket);
		return INVALID_SOCKET;
	}

	return server_socket;
}
int remove_client(int idx) {
	if (idx < 0 || idx >= MAX_CLIENTS) return 0;
	socket_close(client_list[idx]);
	socket_close(server_list[idx]);
	client_list[idx] = INVALID_SOCKET;
	server_list[idx] = INVALID_SOCKET;
	client_count--;
}
int add_new_client(int idx, SOCKET client, SOCKET server) {
	if (idx < 0 || idx >= MAX_CLIENTS) return 0;
	server_list[idx] = server;
	client_list[idx] = client;
	client_count++;
	return 1;
}

void accept_new_client() {
	int socket_index = 0;
	while (client_count < MAX_CLIENTS) {
		SOCKET incoming_client = accept(listening_socket, NULL, NULL);
		if (incoming_client == INVALID_SOCKET) continue;

		for (; socket_index < MAX_CLIENTS; socket_index++) {
			if (client_list[socket_index] != INVALID_SOCKET) continue;

			SOCKET server_socket = socket_create_connect(server_port, server_ip);
			if (server_socket == INVALID_SOCKET) {
				socket_close(incoming_client);
				break;
			}

			if (!socket_set_unblocking(incoming_client)) {
				printf("Couldn't change server socket to unblocking mode.\n");
				socket_close(server_socket);
				socket_close(incoming_client);
				break;
			}

			if (!add_new_client(socket_index, incoming_client, server_socket)) {
				printf("Couldn't set the client to idx: (%d).\n", socket_index);
				socket_close(server_socket);
				socket_close(incoming_client);
				break;
			}
			socket_index++;
			break;
		}
	}
}

int socket_forward(int idx, int from_client) {
	SOCKET client = client_list[idx];
	SOCKET server = server_list[idx];
	if (!FD_ISSET(from_client ? client : server, &readfds) || !FD_ISSET(from_client ? server : client, &writefds)) {
		return 1;
	}

	int bytes = recv(from_client ? client : server, recv_buffer, RECV_BUFFER_SIZE, 0);

	if (bytes > 0 && handle_packet_event(client, server, from_client, recv_buffer, bytes)) {
		if (socket_send(from_client ? server : client, recv_buffer, bytes) == -1) {
			return -1;
		}
	}
	else {
		if (bytes == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
			return -1;
		}
	}
	return 1;
}

int __stdcall ConsoleHandler(DWORD signal) {
	should_cleanup = 1;
	return TRUE;
}

int main(int argc, char *argv[], char *env[]) {
	if (!SetConsoleCtrlHandler(ConsoleHandler, 1)) {
		printf("Couldn't set control handler\n");
		return 1;
	}

	if (!proxy_initialize()) {
		return -1;
	}

	while (!should_cleanup) {
		if (sockets_check_ready() <= 0) {
			Sleep(IDLE_SLEEP_MS);
			continue;
		}

		if (FD_ISSET(listening_socket, &readfds)) {
			accept_new_client();
		}

		if (client_count <= 0) {
			Sleep(IDLE_SLEEP_MS);
			continue;
		}

		for (int socket_index = 0; socket_index < MAX_CLIENTS; socket_index++) {
			if (client_list[socket_index] == INVALID_SOCKET || server_list[socket_index] == INVALID_SOCKET) continue;

			if (socket_forward(socket_index, 1) != 1) {
				remove_client(socket_index);
				continue;
			}

			if (socket_forward(socket_index, 0) != 1) {
				remove_client(socket_index);
				continue;
			}
		}
	}

	if (!proxy_cleanup()) {
		printf("Couldn't cleanup the proxy.\n");
		return -1;
	}
	return 0;
}