#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

#define BUFSIZE 8192

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

uint32_t crc32_file(FILE *f) {
    uint32_t crc = 0xFFFFFFFF;
    unsigned char buf[BUFSIZE];
    size_t n;
    rewind(f);
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
        }
    }
    return crc ^ 0xFFFFFFFF;
}


int main(int argc, char** argv){
    if (argc != 4){
        fprintf(stderr, "Usage: %s <server_ip> <port> <filename>\n", argv[0]);
        exit(1);
    }
    char* server_ip = argv[1];
    int port = atoi(argv[2]);
    char *filename = argv[3];

        init_crc32_table();

    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror("fopen");
        exit(1);
    }
      // Вычисляем CRC файла
    uint32_t file_crc = crc32_file(f);
    printf("File CRC32: 0x%08X\n", file_crc);

    // Подключаемся к серверу
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        fclose(f);
        exit(1);
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        fclose(f);
        exit(1);
    }
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        fclose(f);
        exit(1);
    }
        // Отправляем файл
    rewind(f);
    unsigned char buf[BUFSIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        ssize_t sent = write(sock, buf, n);
        if (sent < 0) {
            perror("write");
            break;
        }
    }

    printf("File sent.\n");

    fclose(f);
    close(sock);
    return 0;
}