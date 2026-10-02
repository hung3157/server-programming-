#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define REQUEST_SIZE 128
#define LISTEN_BACKLOG 10
#define RECEIVE_TIMEOUT_SECONDS 5

static int send_response(int client_fd, const char *response)
{
    size_t sent = 0;
    size_t response_length = strlen(response);

    while (sent < response_length) {
        // Send the remaining response bytes on the connected client socket.
        ssize_t bytes_sent = send(client_fd, response + sent,
                                  response_length - sent, MSG_NOSIGNAL);

        if (bytes_sent < 0) {
            perror("send");
            return -1;
        }

        sent += (size_t)bytes_sent;
    }

    return 0;
}

static void handle_client(int client_fd, int timeout_enabled)
{
    char request[REQUEST_SIZE];
    size_t used = 0;

    if (timeout_enabled) {
        struct timeval timeout;

        timeout.tv_sec = RECEIVE_TIMEOUT_SECONDS;
        timeout.tv_usec = 0;

        // Set a receive timeout on this accepted socket.
        if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO,
                       &timeout, sizeof(timeout)) < 0) {
            perror("setsockopt");
            return;
        }
    }

    // Read until the client sends a newline; each recv() may return only part.
    while (used < sizeof(request) - 1) {
        // Blocking recv() waits when no request bytes are available.
        ssize_t bytes_received = recv(client_fd, request + used,
                                      sizeof(request) - 1 - used, 0);

        if (bytes_received == 0) {
            printf("Client disconnected before completing a request.\n");
            return;
        }

        if (bytes_received < 0) {
            if (timeout_enabled &&
                (errno == EAGAIN || errno == EWOULDBLOCK)) {
                printf("Client timeout. Closing connection.\n");
            } else {
                perror("recv");
            }
            return;
        }

        used += (size_t)bytes_received;
        request[used] = '\0';

        if (strchr(request, '\n') != NULL) {
            break;
        }
    }

    if (strchr(request, '\n') == NULL) {
        printf("Request is too long. Closing connection.\n");
        return;
    }

    char *newline = strchr(request, '\n');
    *newline = '\0';
    if (newline > request && newline[-1] == '\r') {
        newline[-1] = '\0';
    }

    printf("Received request: %s\n", request);

    if (strcmp(request, "HELLO") == 0) {
        send_response(client_fd, "Hello from mini TCP server!\n");
    } else if (strcmp(request, "TIME") == 0) {
        time_t current_time = time(NULL);
        struct tm *local_time = localtime(&current_time);
        char time_text[64];

        if (local_time == NULL ||
            strftime(time_text, sizeof(time_text), "%Y-%m-%d %H:%M:%S",
                     local_time) == 0) {
            send_response(client_fd, "Could not read the current time.\n");
        } else {
            char response[80];
            snprintf(response, sizeof(response), "%s\n", time_text);
            send_response(client_fd, response);
        }
    } else if (strcmp(request, "QUIT") == 0) {
        send_response(client_fd, "Goodbye!\n");
    } else {
        send_response(client_fd, "Unknown command. Use HELLO, TIME, or QUIT.\n");
    }
}

static int create_server_socket(int port)
{
    // Create an IPv4 TCP socket; the return value is a file descriptor.
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    int reuse_address = 1;
    // Allow quick local restarts when an earlier connection is still closing.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                   &reuse_address, sizeof(reuse_address)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(server_fd);
        return -1;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons((unsigned short)port);
    server_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    // Assign the local loopback address and port to the socket.
    if (bind(server_fd, (struct sockaddr *)&server_address,
             sizeof(server_address)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }

    // Mark the socket as passive so it can accept incoming connections.
    if (listen(server_fd, LISTEN_BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }

    return server_fd;
}

int main(int argc, char *argv[])
{
    if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--timeout") != 0)) {
        fprintf(stderr, "Usage: %s <port> [--timeout]\n", argv[0]);
        return 1;
    }

    char *end = NULL;
    long port_value = strtol(argv[1], &end, 10);
    if (*argv[1] == '\0' || *end != '\0' || port_value < 1 || port_value > 65535) {
        fprintf(stderr, "Port must be a number from 1 to 65535.\n");
        return 1;
    }

    int timeout_enabled = argc == 3;
    int server_fd = create_server_socket((int)port_value);
    if (server_fd < 0) {
        return 1;
    }

    printf("Listening on 127.0.0.1:%ld (%s mode)\n", port_value,
           timeout_enabled ? "5-second receive timeout" : "blocking");

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_length = sizeof(client_address);

        // accept() returns a new file descriptor for this client connection.
        int client_fd = accept(server_fd, (struct sockaddr *)&client_address,
                               &client_address_length);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        printf("Client connected. Waiting for one newline-terminated command.\n");
        handle_client(client_fd, timeout_enabled);

        // Close the client socket before accepting the next client.
        close(client_fd);
    }

    close(server_fd);
    return 0;
}