#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

// cau truc luu thong tin clien
struct Client {
    int fd;
    char id[50];
    char name[50];
    int is_auth;
};

int main(int argc, char *argv[]) {
    // check tham sso dau vao
    if (argc != 2) {
        printf("Cu phap: %s <cong>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Loi tao scket");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Loi bind");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Loi listen");
        exit(1);
    }

    // khoi tao mang poll
    struct pollfd fds[MAX_CLIENTS + 1];
    struct Client clients[MAX_CLIENTS];
    int nfds = 1;

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("Server chat dang chay tren cong %d...\n", port);

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) {
            perror("Loi pol");
            break;
        }

        // co ket nooi moy
        if (fds[0].revents & POLLIN) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

            if (client_fd >= 0 && nfds < MAX_CLIENTS + 1) {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                clients[nfds - 1].fd = client_fd;
                clients[nfds - 1].is_auth = 0; // chua dang nhap
                nfds++;

                char *msg = "Vui long nhap theo cu phap: client_id: client_name\n";
                send(client_fd, msg, strlen(msg), 0);
            }
        }

        // xu li data tu cac clien
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                char buf[BUFFER_SIZE];
                int bytes_recv = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);

                if (bytes_recv <= 0) {
                    // clien ngat ket nôi
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    clients[i - 1] = clients[nfds - 2];
                    nfds--;
                    i--;
                    continue;
                }

                buf[bytes_recv] = '\0';
                buf[strcspn(buf, "\r\n")] = 0;

                if (clients[i - 1].is_auth == 0) {
                    // dang cho xac nhan id va ten
                    char id[50], name[50];
                    if (sscanf(buf, "%49[^:]: %49[^\n]", id, name) == 2) {
                        strcpy(clients[i - 1].id, id);
                        strcpy(clients[i - 1].name, name);
                        clients[i - 1].is_auth = 1;
                        char *ok_msg = "Dang nhap thah cong!\n";
                        send(fds[i].fd, ok_msg, strlen(ok_msg), 0);
                    } else {
                        char *err_msg = "Sai cu phapp! Vui long nhap lai: client_id: client_name\n";
                        send(fds[i].fd, err_msg, strlen(err_msg), 0);
                    }
                } else {
                    // broadcast tn
                    time_t t = time(NULL);
                    struct tm *tm_info = localtime(&t);
                    char time_str[30];
                    strftime(time_str, sizeof(time_str), "%Y/%m/%d %I:%M:%S%p", tm_info);

                    char send_buf[BUFFER_SIZE + 100];
                    snprintf(send_buf, sizeof(send_buf), "%s %s: %s\n", time_str, clients[i - 1].id, buf);

                    for (int j = 1; j < nfds; j++) {
                        if (j != i && clients[j - 1].is_auth == 1) {
                            send(fds[j].fd, send_buf, strlen(send_buf), 0);
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}