#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>

#define PORT 8080
#define BUFFER_SIZE 1024

int client_socket; // Global variable for the client socket

// Signal handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    printf("\nKlient zaraz się rozłączy.\n");

    // Notify the server before disconnecting
    const char *disconnect_message = "DISCONNECT";
    send(client_socket, disconnect_message, strlen(disconnect_message), 0);

    close(client_socket); // Close the client socket
    exit(0);              // Exit the program
}

// Function to get input from the user
void get_equation_input(char *buffer) {
    char input[BUFFER_SIZE];
    float a, b;
    char operator;
    int valid = 0;

    while (!valid) {
        printf("Wprowadź równanie a operator b: ");
        if (fgets(input, BUFFER_SIZE, stdin) != NULL) {
            size_t len = strlen(input);
            if (input[len - 1] == '\n') input[len - 1] = '\0';

            if (sscanf(input, "%f %c %f", &a, &operator, &b) == 3) {
                if ((operator == '+' || operator == '-' || operator == '*' || operator == '/') &&
                    !(operator == '/' && b == 0)) {
                    snprintf(buffer, BUFFER_SIZE, "%.2f %c %.2f", a, operator, b);
                    valid = 1;
                } else {
                    printf("Błąd: Nieprawidłowy operator lub dzielenie przez zero.\n");
                }
            } else {
                printf("Błąd: Nieprawidłowy format. Spróbuj ponownie.\n");
            }
        }
    }
}

int main() {
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    fd_set read_fds;
    int max_fd;

    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Nie udało się utworzyć socketu");
        exit(EXIT_FAILURE);
    }

    // Register SIGINT handler
    signal(SIGINT, handle_sigint);

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Nie udało się połączyć z serwerem");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Połączono z serwerem.\n");

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket, &read_fds);
        FD_SET(fileno(stdin), &read_fds);
        max_fd = client_socket > fileno(stdin) ? client_socket : fileno(stdin);

        // Use select to wait for input from server or user
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            perror("Błąd w select");
            break;
        }

        // Check if there's data from the server
        if (FD_ISSET(client_socket, &read_fds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

            if (bytes_received <= 0) {
                printf("Serwer zamknął połączenie.\n");
                break;
            }

            buffer[bytes_received] = '\0'; // Null-terminate the received string
            if (strcmp(buffer, "Serwer został zamknięty") == 0) {
                printf("Serwer zamknął połączenie.\n");
                break;
            }
            printf("Odpowiedź serwera: %s\n", buffer);
        }

        // Check if there's user input
        if (FD_ISSET(fileno(stdin), &read_fds)) {
            get_equation_input(buffer);

            // Send the input to the server
            if (send(client_socket, buffer, strlen(buffer), 0) == -1) {
                perror("Nie udało się wysłać danych");
                break;
            }
        }
    }

    close(client_socket); // Close the socket
    printf("Klient zakończył pracę.\n");
    return 0;
}
