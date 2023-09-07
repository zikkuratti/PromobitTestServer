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

// Функция для обработки отключения клиента
void handle_client_disconnect(int client_sock) {
    printf("Client disconnected\n");
    close(client_sock);
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

    signal(SIGPIPE, SIG_IGN); // Игнорируем сигнал SIGPIPE

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        // Создаем новый процесс для обработки клиента
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            close(client_sock);
            continue;
        } else if (pid == 0) { // Дочерний процесс
            close(listen_sock); // Закрываем слушающий сокет в дочернем процессе

            char filename[128];
            snprintf(filename, sizeof(filename), "%d.txt", port);
            int file_fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (file_fd < 0) {
                perror("open");
                close(client_sock);
                exit(1);
            }

            char buffer[MAX_MESSAGE_LEN];
            ssize_t bytes_received;
            while ((bytes_received = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
                // Сохраняем сообщение в файл
                write(file_fd, buffer, bytes_received);

                // Ожидание 3 секунд
                sleep(3);

                // Отправляем "ACCEPTED" клиенту
                const char *response = "ACCEPTED\n";
                send(client_sock, response, strlen(response), 0);
            }

            // Обработка отключения клиента по инициативе клиента
            if (bytes_received == 0) {
                handle_client_disconnect(client_sock);
            } else {
                perror("recv");
            }

            close(file_fd);
            close(client_sock);
            exit(0);
        } else { // Родительский процесс
            close(client_sock); // Закрываем сокет в родительском процессе
        }
    }

    io_uring_queue_exit(&ring);

    return 0;
}
