#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <liburing.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_MESSAGE_LEN 128

// Структура для хранения информации о клиентском соединении
struct client_info {
    int client_sock;
    int file_fd;
    struct sockaddr_in client_addr;
};

// Функция для обработки отключения клиента
void handle_client_disconnect(struct client_info *client) {
    printf("Client disconnected\n");
    close(client->file_fd);
    close(client->client_sock);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    struct io_uring ring;
    if (io_uring_queue_init(256, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        return 1;
    }

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror("socket");
        return 1;
    }

    int optval = 1;
    if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(listen_sock, 5) < 0) {
        perror("listen");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);

    struct io_uring_cqe *cqe;
    struct io_uring_sqe *sqe;
    struct client_info clients[256]; // Массив информации о клиентах
    memset(clients, 0, sizeof(clients));

    while (1) {
        sqe = io_uring_get_sqe(&ring);

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);

        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        int file_fd;
        char filename[128];
        snprintf(filename, sizeof(filename), "%d.txt", port);

        file_fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (file_fd < 0) {
            perror("open");
            close(client_sock);
            continue;
        }

        // Заполняем информацию о клиенте
        int client_index = -1;
        for (int i = 0; i < 256; i++) {
            if (clients[i].client_sock == 0) {
                client_index = i;
                break;
            }
        }

        if (client_index == -1) {
            fprintf(stderr, "Too many clients\n");
            close(file_fd);
            close(client_sock);
            continue;
        }

        clients[client_index].client_sock = client_sock;
        clients[client_index].file_fd = file_fd;
        clients[client_index].client_addr = client_addr;

        // Ожидание 3 секунд
        sqe->opcode = IORING_OP_TIMEOUT;
        sqe->flags = 0;
        sqe->ioprio = 0;
        sqe->fd = 0;
        sqe->off = 0;
        sqe->addr = 0;
        sqe->len = 0;
        sqe->timeout_flags = 0;
        sqe->timeout_spec.tv_sec = 3;
        sqe->timeout_spec.tv_nsec = 0;

        io_uring_submit(&ring);

        // Ожидаем завершения операции ожидания тайм-аута
        while (1) {
            int ret = io_uring_wait_cqe(&ring, &cqe);
            if (ret < 0) {
                perror("io_uring_wait_cqe");
                exit(1);
            }

            if (cqe->user_data == (unsigned long)sqe) {
                io_uring_cqe_seen(&ring, cqe);
                break;
            }

            io_uring_cqe_seen(&ring, cqe);
        }

        // Отправляем "ACCEPTED" клиенту
        const char *response = "ACCEPTED\n";
        sqe = io_uring_get_sqe(&ring);
        sqe->opcode = IORING_OP_SEND;
        sqe->flags = 0;
        sqe->ioprio = 0;
        sqe->fd = client_sock;
        sqe->off = 0;
        sqe->addr = (unsigned long)response;
        sqe->len = strlen(response);
        sqe->timeout_flags = 0;
        sqe->timeout_spec.tv_sec = 0;
        sqe->timeout_spec.tv_nsec = 0;

        io_uring_submit(&ring);

        // Сброс информации о клиенте
        clients[client_index].client_sock = 0;
        clients[client_index].file_fd = 0;

        io_uring_wait_cqe(&ring, &cqe);
        io_uring_cqe_seen(&ring, cqe);
    }

    io_uring_queue_exit(&ring);

    return 0;
}
