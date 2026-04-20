#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024

struct ClientState {
    int fd;
    int is_logged_in;
};

// ham check tk trong file
int check_login(const char *db_file, const char *user, const char *pass) {
    FILE *f = fopen(db_file, "r");
    if (!f) return 0;

    char line[256];
    char f_user[100], f_pass[100];
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%s %s", f_user, f_pass) == 2) {
            if (strcmp(user, f_user) == 0 && strcmp(pass, f_pass) == 0) {
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    // check tso
    if (argc != 3) {
        printf("Cu phap: %s <cong> <file_csdl>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    char *db_file = argv[2];

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Loi ttao sockket");
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
        perror("loi binnd");
        exit(1);
    }

    if (listen(server_fd, 5) < 0) {
        perror("loi listten");
        exit(1);
    }

    struct pollfd fds[MAX_CLIENTS + 1];
    struct ClientState clients[MAX_CLIENTS];
    int nfds = 1;

    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("Telnet server dang doi o cong %d...\n", port);

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) break;

        // xu li ng dung ket noi
        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd >= 0 && nfds < MAX_CLIENTS + 1) {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                clients[nfds - 1].fd = client_fd;
                clients[nfds - 1].is_logged_in = 0;
                nfds++;

                char *msg = "Vui long nhap tai khoan va mat khau (user pass):\n";
                send(client_fd, msg, strlen(msg), 0);
            }
        }

        // xu ly cmd
        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                char buf[BUFFER_SIZE];
                int bytes = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);

                if (bytes <= 0) {
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    clients[i - 1] = clients[nfds - 2];
                    nfds--;
                    i--;
                    continue;
                }

                buf[bytes] = '\0';
                buf[strcspn(buf, "\r\n")] = 0;

                if (!clients[i - 1].is_logged_in) {
                    char user[100], pass[100];
                    if (sscanf(buf, "%s %s", user, pass) == 2) {
                        if (check_login(db_file, user, pass)) {
                            clients[i - 1].is_logged_in = 1;
                            char *ok = "Dang nhapp thah cong. Nhap lenh:\n";
                            send(fds[i].fd, ok, strlen(ok), 0);
                        } else {
                            char *err = "Sai thong tin, nhapp lai:\n";
                            send(fds[i].fd, err, strlen(err), 0);
                        }
                    } else {
                        char *err = "Cu phapp sai. Nhap: user pass\n";
                        send(fds[i].fd, err, strlen(err), 0);
                    }
                } else {
                    // thuc thi leenh va ghi vao file text
                    char cmd[BUFFER_SIZE + 20];
                    char *out_file = "out.txt";
                    
                    // dung chuyen huong ca loi 2>&1 de clien thay loi neu go sai leenh
                    sprintf(cmd, "%s > %s 2>&1", buf, out_file); 
                    
                    system(cmd);

                    // mo file text ra de tra kq cho clien
                    FILE *f_out = fopen(out_file, "r");
                    if (f_out) {
                        char out_buf[BUFFER_SIZE];
                        size_t n;
                        while ((n = fread(out_buf, 1, sizeof(out_buf), f_out)) > 0) {
                            send(fds[i].fd, out_buf, n, 0);
                        }
                        fclose(f_out);
                        
                        // xoa tep nay sau khi doc xong hoac de lai cung dcuoc
                        // remove(out_file); 
                    }
                    send(fds[i].fd, "\n> ", 3, 0);
                }
            }
        }
    }
    return 0;
}