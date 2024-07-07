#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_PORT 3900
#define SERVER_IP "127.0.0.1"
#define MAX_COMMAND_SIZE 1024
#define MAX_BUFFER_SIZE 4096

void handle_command(int sockfd, const char *command) {
    if (strlen(command) == 0) return;  // Skip empty input.

    if (strcmp(command, "quitc") == 0) {
        send(sockfd, command, strlen(command), 0);  // Send quit command to server.
        return;  // Exit function.
    }

    if (send(sockfd, command, strlen(command), 0) < 0) {
        perror("Error on send");
        return;
    }

    char buffer[MAX_BUFFER_SIZE];
    int bytes_received;
    memset(buffer, 0, sizeof(buffer));  // Clear buffer for each response.

    // Continuously read until no more data.
    while ((bytes_received = read(sockfd, buffer, sizeof(buffer) - 1)) > 0) {
        printf("%s", buffer);
        memset(buffer, 0, sizeof(buffer));  // Clear buffer after printing.
    }
    if (bytes_received < 0) {
        perror("Read failed");
        exit(EXIT_FAILURE);
    }
}

int main() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char command[MAX_COMMAND_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        exit(1);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));  // Clear structure.
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Error on inet_pton");
        close(sockfd);
        exit(1);
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Error on connect");
        close(sockfd);
        exit(1);
    }

    printf("Connected to the server. Type commands:\n");

    while (1) {
        printf("> ");
        memset(command, 0, MAX_COMMAND_SIZE);  // Clear command buffer.
        fgets(command, MAX_COMMAND_SIZE, stdin);
        command[strcspn(command, "\n")] = 0;  // Remove newline character.

        handle_command(sockfd, command);
    }

    close(sockfd);  // Close socket.
    return 0;
}
