#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <signal.h>
#include <sys/select.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

// Global variables
int server_socket;
int client_sockets[MAX_CLIENTS] = {0};

// Function to validate the equation
int is_valid_equation(const char *input, float *a, char *operator, float *b) {
    char clean_input[BUFFER_SIZE] = {0};
    int j = 0;

    // Remove spaces from input
    for (int i = 0; i < strlen(input); i++) {
        if (!isspace(input[i])) clean_input[j++] = input[i];
    }

    // Check if the format is valid (a operator b)
    if (sscanf(clean_input, "%f%c%f", a, operator, b) == 3) {
        if (*operator == '+' || *operator == '-' || *operator == '*' || *operator == '/') {
            if (*operator == '/' && *b == 0) {
                return 0; // Division by zero
            }
            return 1; // Valid equation
        }
    }
    return 0; // Invalid equation
}

// Signal handler for SIGINT
void handle_sigint(int sig) {
    printf("\nSerwer zamyka się...\n");

    // Notify all clients about the shutdown
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] > 0) {
            send(client_sockets[i], "Serwer został zamknięty", 23, 0);
            close(client_sockets[i]);
            client_sockets[i] = 0;
        }
    }

    // Wait for 2 seconds before closing the server
    sleep(2);
    close(server_socket);
    printf("Serwer zamknięty.\n");
    exit(0);
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    fd_set read_fds;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    // Register SIGINT handler
    signal(SIGINT, handle_sigint);

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Nie udało się utworzyć socketu");
        exit(EXIT_FAILURE);
    }

    // Set SO_REUSEADDR option for the server socket
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configure the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Nie udało się przypisać adresu");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CLIENTS) == -1) {
        perror("Nie udało się ustawić trybu nasłuchiwania");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Serwer nasłuchuje na porcie %d...\n", PORT);

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);
        int max_fd = server_socket;

        // Add clients to the read set
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_sockets[i] > 0) {
                FD_SET(client_sockets[i], &read_fds);
            }
            if (client_sockets[i] > max_fd) {
                max_fd = client_sockets[i];
            }
        }

        // Use select to monitor sockets for activity
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("Błąd w select");
            continue;
        }

        // Handle new connection
        if (FD_ISSET(server_socket, &read_fds)) {
            int new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
            if (new_socket < 0) {
                perror("Błąd przy akceptacji połączenia");
                continue;
            }

            int assigned = 0;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    assigned = 1;
                    break;
                }
            }

            if (!assigned) {
                printf("Maksymalna liczba klientów osiągnięta. Nowe połączenie odrzucone.\n");
                close(new_socket);
            } else {
                printf("Nowe połączenie zaakceptowane.\n");
            }
        }

        // Handle active clients
        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sock = client_sockets[i];
            if (FD_ISSET(sock, &read_fds)) {
                memset(buffer, 0, BUFFER_SIZE);
                int bytes_received = recv(sock, buffer, sizeof(buffer) - 1, 0);

                if (bytes_received <= 0) {
                    printf("Klient rozłączony.\n");
                    close(sock);
                    client_sockets[i] = 0;
                } else {
                    buffer[bytes_received] = '\0'; // Null-terminate the received string
                    printf("Otrzymano od klienta: %s\n", buffer);

                    if (strcmp(buffer, "DISCONNECT") == 0) {
                        printf("Klient poprosił o rozłączenie.\n");
                        close(sock);
                        client_sockets[i] = 0;
                    } else {
                        float a, b, result;
                        char operator;

                        if (is_valid_equation(buffer, &a, &operator, &b)) {
                            switch (operator) {
                                case '+':
                                    result = a + b;
                                    break;
                                case '-':
                                    result = a - b;
                                    break;
                                case '*':
                                    result = a * b;
                                    break;
                                case '/':
                                    result = a / b;
                                    break;
                            }
                            snprintf(buffer, BUFFER_SIZE, "%.2f", result);
                        } else {
                            snprintf(buffer, BUFFER_SIZE, "Błąd: Nieprawidłowe równanie.");
                        }

                        send(sock, buffer, strlen(buffer), 0);
                    }
                }
            }
        }
    }

    close(server_socket);
    return 0;
}
