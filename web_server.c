#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define PORT 80
#define BUFFER_SIZE 1024
#define STATIC_DIR "static"

static int request_count = 0;
static long bytes_recieved = 0, bytes_sent = 0;
pthread_mutex_t lock;

void handle_static(int client_socket, const char *path);
void handle_stats(int client_socket);
void handle_calc(int client_socket, const char *query);
void *handle_request(void *arg);
void *handle_client(void *arg);

int main(int argc, char *argv[]) {
    int port = PORT, server_socket;
    struct sockaddr_in server_addr;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            break;
        }
    }
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_socket, 1);
    printf("Server running on port %d...\n", port);
    pthread_mutex_init(&lock, NULL);
    while (1) {
        int *client_socket = malloc(sizeof(int));
        if ((*client_socket = accept(server_socket, NULL, NULL)) < 0) {
            perror("Accept failed");
            free(client_socket);
            continue;
        }
        printf("Client connected\n");
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_request, client_socket);
        pthread_detach(thread_id);
    }
    pthread_mutex_destroy(&lock);
    close(server_socket);
    return EXIT_SUCCESS;
}

void *handle_request(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return NULL;
    }
    buffer[bytes_received] = '\0';
    printf("Received: %s", buffer);
    pthread_mutex_lock(&lock);
    bytes_recieved += bytes_received;
    request_count++;
    pthread_mutex_unlock(&lock);
    char method[10], path[256], version[10];
    sscanf(buffer, "%s %s %s", method, path, version);
    if (strcmp(method, "GET") == 0) {
        if (strncmp(path, "/static", 7) == 0) {
            handle_static(client_socket, path);
        } else if (strcmp(path, "/stats") == 0) {
            handle_stats(client_socket);
        } else if (strncmp(path, "/calc", 5) == 0) {
            handle_calc(client_socket, strchr(path, '?') + 1);
        } 
    }
    close(client_socket);
    return NULL;
}

void handle_static(int client_socket, const char *path) {
    char full_path[BUFFER_SIZE];
    snprintf(full_path, sizeof(full_path), "%s%s", STATIC_DIR, path + 7);
    FILE *file = fopen(full_path, "rb");
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", file_size);
    send(client_socket, header, strlen(header), 0);
    char *file_buffer = malloc(file_size);
    fread(file_buffer, 1, file_size, file);
    send(client_socket, file_buffer, file_size, 0);
    pthread_mutex_lock(&lock);
    bytes_sent += file_size;
    pthread_mutex_unlock(&lock);
    free(file_buffer);
    fclose(file);
}

void handle_stats(int client_socket) {
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
                                         "<html><head><title>Server Stats</title></head>"
                                         "<body><h1>Server Stats</h1>"
                                         "<p>Requests received: %d</p>"
                                         "<p>Total received bytes: %ld</p>"
                                         "<p>Total sent bytes: %ld</p>"
                                         "</body></html>",
                                         request_count, bytes_recieved, bytes_sent);
    send(client_socket, response, strlen(response), 0);
}

void handle_calc(int client_socket, const char *query) {
    float a = 0, b = 0;
    sscanf(query, "a=%f&b=%f", &a, &b);
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n"
                                         "The sum of %f and %f is %f.", a, b, a + b);
    send(client_socket, response, strlen(response), 0);
}