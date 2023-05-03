#include "chat.pb-c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>

// Connected user structure
typedef struct ConnectedUser {
    ChatSistOS__User user;
    int client_socket;
    char user_ip[INET_ADDRSTRLEN];
    uint16_t user_port; // Add the port number field
    struct ConnectedUser *next;
} ConnectedUser;
// Function prototypes
void add_broadcast_message(ChatSistOS__Message *message);
bool add_connected_user(ChatSistOS__User *user, int client_socket, struct sockaddr_in *client_addr);
void print_connected_users();
void remove_connected_user(int client_socket);
ConnectedUser *find_user_by_name(const char *name);
ChatSistOS__Message *create_message(const char *text);
char *get_user_list(bool list_all, const char *specific_user);
void handle_error(const char *message, int client_socket);
void *client_handler(void *client_data_ptr);
void send_message_to_all_clients(ChatSistOS__Message *message);
void send_message_to_specific_client(ChatSistOS__Message *message, ConnectedUser *target_user);

// Mutex for shared data
pthread_mutex_t shared_data_mutex;

// Broadcast message structure
typedef struct BroadcastMessage {
    ChatSistOS__Message message;
    struct BroadcastMessage *next;
} BroadcastMessage;

BroadcastMessage *broadcast_messages_head = NULL;

ConnectedUser *connected_users_head = NULL;

typedef struct ClientData {
    int client_socket;
    struct sockaddr_in client_addr;
} ClientData;

int main(int argc, char *argv[]) {
    if (argc != 2){
    fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
    exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pthread_t thread_id;
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error al crear el socket del servidor");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error al enlazar el socket del servidor");
        return 1;
    }

    if (listen(server_socket, 5) < 0) {
        perror("Error al escuchar en el socket del servidor");
        return 1;
    }

    printf("Servidor iniciado en el puerto %d...\n", port);
    if (pthread_mutex_init(&shared_data_mutex, NULL) != 0) {
        perror("Error al inicializar el mutex");
        return 1;
    }
    while (1) {
        addr_size = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size);

        if (client_socket < 0) {
            perror("Error al aceptar conexión del cliente");
            continue;
        }
        printf("Cliente conectado desde %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        ClientData *client_data_ptr = (ClientData *)malloc(sizeof(ClientData));
        if (client_data_ptr == NULL) {
            perror("Error al asignar memoria para el puntero de datos del cliente");
            continue;
        }

        client_data_ptr->client_socket = client_socket;
        client_data_ptr->client_addr = client_addr;

        if (pthread_create(&thread_id, NULL, client_handler, (void *)client_data_ptr) != 0) {
            perror("Error al crear el hilo para el cliente");
            continue;
        }

        pthread_detach(thread_id);
    }

    pthread_mutex_destroy(&shared_data_mutex);
    if(close(server_socket)<0){
        perror("Error al cerrar el socket del servidor");
    }

    return 0;
}

// Function implementations
void add_broadcast_message(ChatSistOS__Message *message) {
    BroadcastMessage *new_node = (BroadcastMessage *)malloc(sizeof(BroadcastMessage));
    if (new_node == NULL) {
        perror("Error al asignar memoria para el nuevo mensaje");
        return;
    }

    new_node->message = *message;
    new_node->next = broadcast_messages_head;
    broadcast_messages_head = new_node;

    printf("Broadcast message: %s\n", message->message_content);
}

bool add_connected_user(ChatSistOS__User *user, int client_socket, struct sockaddr_in *client_addr) {
    ConnectedUser *new_node = (ConnectedUser *)malloc(sizeof(ConnectedUser));
    if (new_node == NULL) {
        return false;
    }

    new_node->user = *user;
    new_node->client_socket = client_socket;
    inet_ntop(AF_INET, &(client_addr->sin_addr), new_node->user_ip, INET_ADDRSTRLEN);
    new_node->user_port = ntohs(client_addr->sin_port); // Store the port number
    new_node->next = connected_users_head;
    connected_users_head = new_node;

    return true;
}

void print_connected_users() {
    ConnectedUser *current_node = connected_users_head;
    printf("Connected users:\n");

    if (current_node == NULL) {
        printf("No users connected.\n");
        return;
    }

    while (current_node != NULL) {
        printf("User: %s, State: %d, IP: %s, Port: %u\n", current_node->user.user_name, current_node->user.user_state, current_node->user_ip, current_node->user_port);
        current_node = current_node->next;
    }
}

void remove_connected_user(int client_socket) {
    ConnectedUser *current_node = connected_users_head;
    ConnectedUser *previous_node = NULL;

    while (current_node != NULL) {
        if (current_node->client_socket == client_socket) {
            if (previous_node == NULL) {
                connected_users_head = current_node->next;
            } else {
                previous_node->next = current_node->next;
            }

            free(current_node);
            break;
        }

        previous_node = current_node;
        current_node = current_node->next;
    }
}

ConnectedUser *find_user_by_name(const char *name) {
    ConnectedUser *current_node = connected_users_head;

    while (current_node != NULL) {
        if (strcmp(current_node->user.user_name, name) == 0) {
            return current_node;
        }
        current_node = current_node->next;
    }

    return NULL;
}

ChatSistOS__Message *create_message(const char *text) {
    ChatSistOS__Message *message = malloc(sizeof(ChatSistOS__Message));
    chat_sist_os__message__init(message);

    message->message_content = strdup(text);

    return message;
}

char *get_user_list(bool list_all, const char *specific_user) {
    ConnectedUser *current_node = connected_users_head;
    size_t buffer_size = 1024;
    char *buffer = (char *)malloc(buffer_size);
    size_t used_buffer = 0;

    if (connected_users_head == NULL) {
        snprintf(buffer, buffer_size, "No hay usuarios conectados\n");
        return buffer;
    }

    while (current_node != NULL) {
        if (list_all || strcmp(current_node->user.user_name, specific_user) == 0) {
            size_t needed_space = strlen(current_node->user.user_name) + 4;
            if (used_buffer + needed_space >= buffer_size) {
                buffer_size *= 2;
                buffer = (char *)realloc(buffer, buffer_size);
            }
            used_buffer += snprintf(buffer + used_buffer, needed_space, "%s [%d]\n", current_node->user.user_name, current_node->user.user_state);
        }
        current_node = current_node->next;
    }

    return buffer;
}

void handle_error(const char *message, int client_socket) {
    perror(message);
    close(client_socket);
}

void send_message_to_all_clients(ChatSistOS__Message *message) {
    ConnectedUser *current_node = connected_users_head;

    while (current_node != NULL) {
        send_message_to_specific_client(message, current_node);
        current_node = current_node->next;
    }
}

void send_message_to_specific_client(ChatSistOS__Message *message, ConnectedUser *target_user) {
    size_t packed_size = chat_sist_os__message__get_packed_size(message);
    uint8_t packed[packed_size];

    chat_sist_os__message__pack(message, packed);
    send(target_user->client_socket, packed, packed_size, 0);
}

void *client_handler(void *client_data_ptr) {
    int client_socket = ((ClientData *)client_data_ptr)->client_socket;
    struct sockaddr_in client_addr = ((ClientData *)client_data_ptr)->client_addr;
    
    free(client_data_ptr);
    ssize_t len;
    uint8_t buf[1024];

    // Receive the client's message and deserialize it using protobuf
    len = recv(client_socket, buf, sizeof(buf), 0);
    if (len <= 0) {
        handle_error("Error al recibir datos del cliente", client_socket);
        remove_connected_user(client_socket);
        return NULL;
    }
    ChatSistOS__UserOption *user_option = chat_sist_os__user_option__unpack(NULL, len, buf);
    if (user_option == NULL) {
        handle_error("Error al deserializar el mensaje UserOption", client_socket);
        remove_connected_user(client_socket);
        return NULL;
    }
    // Check if the client's option is to create a new user
    if (user_option->op == 1) {
        pthread_mutex_lock(&shared_data_mutex);
        ChatSistOS__NewUser *new_user = user_option->createuser;
        ConnectedUser *existing_connected_user = NULL;//find_user_by_name(new_user->username);
        ChatSistOS__User *existing_user = NULL;

        if (existing_connected_user != NULL) {
            existing_user = &(existing_connected_user->user);
        }
        ChatSistOS__Answer answer = CHAT_SIST_OS__ANSWER__INIT;

        if (existing_user != NULL) {
            answer.response_status_code = 400;
            answer.message = create_message("El usuario ya existe");
        } else {
            ChatSistOS__User new_user_to_add = CHAT_SIST_OS__USER__INIT;
            new_user_to_add.user_name = new_user->username;
            new_user_to_add.user_state = 1;
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            new_user_to_add.user_ip = (char *) malloc(strlen(client_ip) + 1); // Allocate memory for the IP address string
            strcpy(new_user_to_add.user_ip, client_ip); // Copy the IP address string to the allocated memory

            if (add_connected_user(&new_user_to_add, client_socket, &client_addr)) {
                answer.response_status_code = 200;
                answer.message = create_message("Usuario creado exitosamente");

                print_connected_users();
            } else {
                answer.response_status_code = 400;
                answer.message = create_message("Error al crear el usuario");
            }
        }

        // Send response to the client
        size_t packed_size = chat_sist_os__answer__get_packed_size(&answer);
        uint8_t packed[packed_size];

        chat_sist_os__answer__pack(&answer, packed);
        send(client_socket, packed, packed_size, 0);

        free(answer.message->message_content);
        free(answer.message);
        pthread_mutex_unlock(&shared_data_mutex);

    } else if (user_option->op == 2) {

        ChatSistOS__UserList *user_list_query = user_option->userlist;
        char *user_list;

        if (user_list_query->list == false) {
            user_list = get_user_list(false, user_list_query->user_name);
        } else {
            user_list = get_user_list(true, NULL);
        }
        ChatSistOS__Message *user_list_message = create_message(user_list);
        ChatSistOS__Answer answer = CHAT_SIST_OS__ANSWER__INIT;
        answer.response_status_code = 1;
        answer.message = user_list_message;

        // Send response to the client
        size_t packed_size = chat_sist_os__answer__get_packed_size(&answer);
        uint8_t packed[packed_size];

        chat_sist_os__answer__pack(&answer, packed);
        send(client_socket, packed, packed_size, 0);

        free(user_list);
        free(answer.message->message_content);
        free(answer.message);
    } else if (user_option->op == 3) {
        // Handle the option to disconnect a user here
        remove_connected_user(client_socket);
    } else if (user_option->op == 4) {
        ChatSistOS__Message *broadcast_message = user_option->message;
        if (!broadcast_message->message_private) {
            // Add the message to the broadcast messages list
            add_broadcast_message(broadcast_message);

            // Send a response to the client
            ChatSistOS__Answer answer = CHAT_SIST_OS__ANSWER__INIT;
            answer.response_status_code = 200;
            answer.message = create_message("Mensaje enviado a todos los usuarios");

            size_t packed_size = chat_sist_os__answer__get_packed_size(&answer);
            uint8_t packed[packed_size];

            chat_sist_os__answer__pack(&answer, packed);
            send(client_socket, packed, packed_size, 0);

            // Send the message to all connected clients
            send_message_to_all_clients(broadcast_message);
        }
    }
    // Cleanup and close
    pthread_mutex_unlock(&shared_data_mutex);
    chat_sist_os__user_option__free_unpacked(user_option, NULL);
    close(client_socket);

    return NULL;
}