#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

#define BUFSIZE 8192

// Таблица CRC32 (полином 0xEDB88320)
static uint32_t crc32_table[256];

static void init_crc32_table() {
    uint32_t crc, poly = 0xEDB88320;
    for (int i = 0; i < 256; i++) {
        crc = i;
        for (int j = 8; j > 0; j--) {
            if (crc & 1)
                crc = (crc >> 1) ^ poly;
            else
                crc >>= 1;
        }
        crc32_table[i] = crc;
    }
}

uint32_t crc32_file(FILE *f, long *size) {
    uint32_t crc = 0xFFFFFFFF;
    unsigned char buf[BUFSIZE];
    size_t n;
    *size = 0;
    rewind(f);
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        *size += n;
        for (size_t i = 0; i < n; i++) {
            crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
        }
    }
    return crc ^ 0xFFFFFFFF;
}

uint32_t crc32_socket(int fd) {
    uint32_t crc = 0xFFFFFFFF;
    unsigned char buf[BUFSIZE];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
        }
    }
    return crc ^ 0xFFFFFFFF;
}

int main(int argc, char**argv){
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }
    int port = atoi(argv[1]); //получение порта через аргументы и факты
    
    init_crc32_table();

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(1);
    }
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr,sizeof(addr)) < 0){
        perror("bind");
        close(listen_fd);
        exit(1);
    }
    printf("Server listening on port %d\n", port);

    while(1){
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        listen(listen_fd,5);
        int conn_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (conn_fd < 0) {
            perror("accept");
            continue;
        }
        struct sockaddr_in server_addr;
        socklen_t server_len = sizeof(server_addr);
        getsockname(conn_fd, (struct sockaddr*)&server_addr, &server_len);
        
        char filename[256];
        snprintf(filename, sizeof(filename), "%s_%d__%s_%d.dat",
                    inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
                    inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port));
        FILE *f = fopen(filename, "wb");
        if (!f){
            perror("fopen");
            close(conn_fd);
            continue;
        }
        uint32_t crc = 0xFFFFFFFF;
        unsigned char buf[BUFSIZE];
        ssize_t n;
        while ((n = read(conn_fd, buf, sizeof(buf))) > 0) {
            fwrite(buf, 1, n, f);
            for (ssize_t i = 0; i < n; i++) {
                crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
            }
        }
        crc ^= 0xFFFFFFFF;

        printf("Received file from %s:%d -> %s, CRC32: 0x%08X\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),
               filename, crc);

        fclose(f);
        close(conn_fd);
    }

    close(listen_fd);
    return 0;
}
   