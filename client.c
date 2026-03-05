/**
 * @file client.c
 * @brief TCP-клиент для отправки файла на сервер с предварительным вычислением CRC32.
 *
 * Программа открывает указанный файл, вычисляет его контрольную сумму CRC32,
 * подключается к серверу по заданному IP-адресу и порту, а затем отправляет
 * содержимое файла. CRC32 файла выводится на экран до отправки.
 *
 * @author pritychkass
 * @date 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

/** Размер буфера для чтения из файла и отправки через сокет. */
#define BUFSIZE 8192

/**
 * @brief Таблица предвычисленных значений CRC32.
 *
 * Используется для быстрого вычисления CRC32.
 * Полином: 0xEDB88320 (стандартный для CRC32).
 */
static uint32_t crc32_table[256];

/**
 * @brief Инициализирует таблицу CRC32.
 *
 * Заполняет глобальную таблицу crc32_table значениями,
 * предвычисленными для каждого байта (0..255).
 * Должна вызываться перед любыми вычислениями CRC32.
 */
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

/**
 * @brief Вычисляет CRC32 для всего содержимого файла.
 *
 * Функция сбрасывает указатель файла в начало, читает данные блоками
 * и вычисляет контрольную сумму CRC32 с использованием предварительно
 * инициализированной таблицы.
 *
 * @param f Указатель на открытый файл (в режиме чтения).
 * @return  Вычисленное значение CRC32 (32 бита).
 */
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

/**
 * @brief Точка входа в программу.
 *
 * Ожидает три аргумента: IP-адрес сервера, порт и имя файла для отправки.
 * Инициализирует таблицу CRC32, вычисляет CRC файла и выводит его на экран.
 * Затем создаёт TCP-сокет, подключается к серверу и отправляет содержимое файла.
 *
 * @param argc Количество аргументов командной строки.
 * @param argv Массив аргументов:
 *             argv[1] — IP-адрес сервера,
 *             argv[2] — порт сервера,
 *             argv[3] — имя файла для отправки.
 * @return     0 при успешном завершении, иначе 1.
 */
int main(int argc, char** argv) {
    if (argc != 4) {
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