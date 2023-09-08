#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <fcntl.h>

#include <sys/select.h>

#include <time.h>



#define MAX_MESSAGE_LEN 128

#define MAX_CLIENTS 10



struct Client {

    int client_sock;

    char buffer[MAX_MESSAGE_LEN];

};



int main(int argc, char *argv[]) {

    if (argc != 2) {

        fprintf(stderr, "Usage: %s <port>\n", argv[0]);

        return 1;

    }



    int port = atoi(argv[1]);



    int server_sock = socket(AF_INET, SOCK_STREAM, 0);

    if (server_sock == -1) {

        perror("socket");

        return 1;

    }



    struct sockaddr_in server_addr;

    memset(&server_addr, 0, sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    server_addr.sin_port = htons(port);



    int reuse = 1;

    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {

        perror("setsockopt");

        return 1;

    }



    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {

        perror("bind");

        return 1;

    }



    if (listen(server_sock, 5) == -1) {

        perror("listen");

        return 1;

    }



    struct Client clients[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {

        clients[i].client_sock = -1;

    }



    char buffer[MAX_MESSAGE_LEN];

    FILE *messageFile = fopen("messages.txt", "a");

    if (!messageFile) {

        perror("fopen");

        return 1;

    }



    while (1) {

        fd_set read_fds;

        FD_ZERO(&read_fds);

        FD_SET(server_sock, &read_fds);



        int max_fd = server_sock;



        for (int i = 0; i < MAX_CLIENTS; i++) {

            if (clients[i].client_sock != -1) {

                FD_SET(clients[i].client_sock, &read_fds);

                max_fd = (clients[i].client_sock > max_fd) ? clients[i].client_sock : max_fd;

            }

        }



        struct timeval timeout;

        timeout.tv_sec = 3;

        timeout.tv_usec = 0;



        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (activity == -1) {

            perror("select");

            continue;

        }



        if (FD_ISSET(server_sock, &read_fds)) {

            // New client connection

            struct sockaddr_in client_addr;

            socklen_t client_len = sizeof(client_addr);

            int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);

            if (client_sock == -1) {

                perror("accept");

                continue;

            }



            int client_index = -1;

            for (int i = 0; i < MAX_CLIENTS; i++) {

                if (clients[i].client_sock == -1) {

                    client_index = i;

                    clients[i].client_sock = client_sock;

                    break;

                }

            }



            if (client_index == -1) {

                fprintf(stderr, "Too many clients\n");

                close(client_sock);

                continue;

            }

        }



        for (int i = 0; i < MAX_CLIENTS; i++) {

            if (clients[i].client_sock != -1 && FD_ISSET(clients[i].client_sock, &read_fds)) {

                // Data received from a client

                int bytes_read = recv(clients[i].client_sock, clients[i].buffer, MAX_MESSAGE_LEN, 0);

                if (bytes_read <= 0) {

                    // Handle client disconnect

                    close(clients[i].client_sock);

                    clients[i].client_sock = -1;

                } else {

                    // Save the received message to the file

                    fwrite(clients[i].buffer, sizeof(char), bytes_read, messageFile);

                    fflush(messageFile);

                    sleep(3);

                    // Send "Accepted" message to the client

                    char accepted_message[] = "Accepted\n";

                    if (send(clients[i].client_sock, accepted_message, strlen(accepted_message), 0) == -1) {

                        perror("send");

                        close(clients[i].client_sock);

                        clients[i].client_sock = -1;

                    }

                }

            }

        }

    }



    fclose(messageFile);

    close(server_sock);



    return 0;

}


