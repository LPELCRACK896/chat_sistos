#include "chat.pb-c.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// Function prototypes
void *receive_message_thread(void *socket);
int display_menu();
void change_status(int client_socket);
void send_private_message(int client_socket, const char* user, const char* message_text);
void broadcast_message(int client_socket, const char* user, const char *message_text);
void list_connected_users(int client_socket);
void display_user_info(int client_socket);
void display_help();
void create_user(int client_socket,  char* user);


int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <username>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int server_port = atoi(argv[2]);
    int client_socket;
    struct sockaddr_in server_addr;
    char *username = argv[3];

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        perror("Error creating client socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        return 1;
    }

    printf("Connected to server %s:%d\n", argv[1], server_port);

    // Start the thread to receive messages
    pthread_t receive_thread_id;
    if (pthread_create(&receive_thread_id, NULL, receive_message_thread, (void *)&client_socket) != 0) {
        perror("Error creating receive message thread");
        return 1;
    }
    create_user(client_socket, username);
    int choice;

    while (true) {
        choice = display_menu();

        switch (choice) {
            case 1: {
                char message[256];
                printf("Enter your message: ");
                fgets(message, sizeof(message), stdin);
                message[strcspn(message, "\n")] = 0;
                broadcast_message(client_socket, username, message);
                break;
            }
            case 2: {
                char message[256];
                printf("Enter your message: ");
                fgets(message, sizeof(message), stdin);
                message[strcspn(message, "\n")] = 0;
                send_private_message(client_socket, username, message);
                break;
            }
            case 3:
                change_status(client_socket);
                break;
            case 4:
                list_connected_users(client_socket);
                break;
            case 5:
                display_user_info(client_socket);
                break;
            case 6:
                display_help();
                break;
            case 7:
                // Exit the application
                printf("Saliendo...\n");
                return 0;
            default:
                printf("Opción inválida. Por favor, intente de nuevo.\n");
        }
    }

    pthread_join(receive_thread_id, NULL);

    close(client_socket);

    return 0;
}
int display_menu() {
    int choice;

    printf("\nMenú:\n");
    printf("1. Chatear con todos los usuarios (broadcasting)\n");
    printf("2. Enviar y recibir mensajes directos, privados, aparte del chat general\n");
    printf("3. Cambiar de status\n");
    printf("4. Listar los usuarios conectados al sistema de chat\n");
    printf("5. Desplegar información de un usuario en particular\n");
    printf("6. Ayuda\n");
    printf("7. Salir\n");
    printf("Ingrese su opción: ");
    scanf("%d", &choice);
    getchar(); // Clear newline character from input buffer

    return choice;
}
void broadcast_message(int client_socket, const char* user, const char *message_text) {
    // Create a new message
    ChatSistOS__Message message = CHAT_SIST_OS__MESSAGE__INIT;
    message.message_sender = (char *)user;
    message.message_content = (char *)message_text;
    message.message_private = false;

    // Serialize the message
    size_t len = chat_sist_os__message__get_packed_size(&message);
    uint8_t *buf = (uint8_t *)malloc(len);
    chat_sist_os__message__pack(&message, buf);

    // Send the message to the server
    send(client_socket, buf, len, 0);

    // Clean up
    free(buf);
}

void send_private_message(int client_socket, const char* user, const char *message_text) {
    // Get the recipient's username
    char recipient[256];
    printf("Enter the recipient's username: ");
    fgets(recipient, sizeof(recipient), stdin);
    recipient[strcspn(recipient, "\n")] = 0;

    // Create a new message
    ChatSistOS__Message message = CHAT_SIST_OS__MESSAGE__INIT;
    message.message_sender = (char *)user;
    message.message_content = (char *)message_text;
    message.message_private = true;
    message.message_destination = recipient;


    // Serialize the message
    size_t len = chat_sist_os__message__get_packed_size(&message);
    uint8_t *buf = (uint8_t *)malloc(len);
    chat_sist_os__message__pack(&message, buf);

    // Send the message to the server
    send(client_socket, buf, len, 0);

    // Clean up
    free(buf);
}


void change_status(int client_socket) {
    // Implement the logic for changing user status
}
void *receive_message_thread(void *socket) {
    int client_socket = *(int *)socket;
    ssize_t len;
    uint8_t buf[1024];

    while (true) {
        len = recv(client_socket, buf, sizeof(buf), 0);
        if (len <= 0) {
            perror("Error receiving data from server");
            break;
        }

        // Deserialize the received message
        ChatSistOS__Answer *answer = chat_sist_os__answer__unpack(NULL, len, buf);
        if (answer == NULL) {
            perror("Error deserializing the received message");
            continue;
        }

        // Display the received message
        printf("Received message: %s\n", answer->message->message_content);

        chat_sist_os__answer__free_unpacked(answer, NULL);
    }

    return NULL;
}

void list_connected_users(int client_socket) {
    // Implement the logic for listing connected users
}

void display_user_info(int client_socket) {
    // Implement the logic for displaying information about a specific user
}

void display_help() {
    // Implement the logic for displaying help
}

void create_user(int client_socket, char* username){
        // Create a NewUser instance
    ChatSistOS__NewUser new_user = CHAT_SIST_OS__NEW_USER__INIT;

    // Set the username field
    new_user.username = username;

    // Create a UserOption instance
    ChatSistOS__UserOption user_option = CHAT_SIST_OS__USER_OPTION__INIT;

    // Set the op field to 1 (new user registration)
    user_option.op = 1;
    user_option.createuser = &new_user;

    // Pack and send the UserOption instance
    size_t packed_size = chat_sist_os__user_option__get_packed_size(&user_option);
    uint8_t packed[packed_size];
    chat_sist_os__user_option__pack(&user_option, packed);
    send(client_socket, packed, packed_size, 0);
}
