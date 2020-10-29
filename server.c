#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

#include <time.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
const char *get_content_type(const char* path) {
    //printf("content - %s\n",path);
    const char *last_dot = strrchr(path, '.');
    if (last_dot) {
        if (strcmp(last_dot, ".css") == 0) return "text/css";
        if (strcmp(last_dot, ".csv") == 0) return "text/csv";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".htm") == 0) return "text/html";
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".js") == 0) return "application/javascript";
        if (strcmp(last_dot, ".json") == 0) return "application/json";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".pdf") == 0) return "application/pdf";
        if (strcmp(last_dot, ".svg") == 0) return "image/svg+xml";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }

    return "application/octet-stream";
}


SOCKET create_socket(const char* host, const char *port) {
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(host, port, &hints, &bind_address);

    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(bind_address);

    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return socket_listen;
}



#define MAX_REQUEST_SIZE 60000

struct client_info {
    socklen_t address_length;
    struct sockaddr_storage address;
    char address_buffer[128];
    SOCKET socket;
    char request[MAX_REQUEST_SIZE + 1];
    int received;
    struct client_info *next;
};


struct client_info *get_client(struct client_info **client_list,
        SOCKET s) {
    
    struct client_info *ci = *client_list;
    while(ci) {
        if (ci->socket == s)
            break;
        ci = ci->next;
    }

    if (ci) return ci;
    struct client_info *n =
        (struct client_info*) calloc(1, sizeof(struct client_info));

    if (!n) {
        fprintf(stderr, "Out of memory.\n");
        exit(1);
    }

    n->address_length = sizeof(n->address);
    n->next = *client_list;
    *client_list = n;
    return n;
}


void drop_client(struct client_info **client_list,
        struct client_info *client) {
    CLOSESOCKET(client->socket);

    struct client_info **p = client_list;

    while(*p) {
        if (*p == client) {
            *p = client->next;
            free(client);
            return;
        }
        p = &(*p)->next;
    }

    fprintf(stderr, "drop_client not found.\n");
    exit(1);
}


const char *get_client_address(struct client_info *ci) {
    getnameinfo((struct sockaddr*)&ci->address,
            ci->address_length,
            ci->address_buffer, sizeof(ci->address_buffer), 0, 0,
            NI_NUMERICHOST);
    return ci->address_buffer;
}




fd_set wait_on_clients(struct client_info **client_list, SOCKET server) {
    fd_set reads;
    FD_ZERO(&reads);
    FD_SET(server, &reads);
    SOCKET max_socket = server;

    struct client_info *ci = *client_list;

    while(ci) {
        FD_SET(ci->socket, &reads);
        if (ci->socket > max_socket)
            max_socket = ci->socket;
        ci = ci->next;
    }

    if (select(max_socket+1, &reads, 0, 0, 0) < 0) {
        fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }

    return reads;
}


void send_400(struct client_info **client_list,
        struct client_info *client) {
    const char *c400 = "HTTP/1.1 400 Bad Request\r\n"
        "Connection: close\r\n"
        "Content-Length: 11\r\n\r\nBad Request";
    send(client->socket, c400, strlen(c400), 0);
    drop_client(client_list, client);
}

void send_404(struct client_info **client_list,
        struct client_info *client) {
    const char *c404 = "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "Content-Length: 9\r\n\r\nNot Found";
    send(client->socket, c404, strlen(c404), 0);
    drop_client(client_list, client);
}
//if file too big , return error
void send_413(struct client_info **client_list,
        struct client_info *client) {
    const char *c413 = "HTTP/1.1 413 File Size Too Large\r\n"
        "Connection: close\r\n"
        "Content-Length: 19\r\n\r\nFile Size Too Large";
    send(client->socket, c413, strlen(c413), 0);
    drop_client(client_list, client);
}
//if file exist , return error
void send_501(struct client_info **client_list,
        struct client_info *client) {
    const char *c501 = "HTTP/1.1 501 File name is exist\r\n"
        "Connection: close\r\n"
        "Content-Length: 18\r\n\r\nFile Name Is Exist";
    send(client->socket, c501, strlen(c501), 0);
    drop_client(client_list, client);
}
//no file be selected , return error
void send_502(struct client_info **client_list,
        struct client_info *client) {
    const char *c502 = "HTTP/1.1 502 No File Be Selected\r\n"
        "Connection: close\r\n"
        "Content-Length: 19\r\n\r\nNo File Be Selected";
    send(client->socket, c502, strlen(c502), 0);
    drop_client(client_list, client);
}


void serve_resource(struct client_info **client_list, struct client_info *client, const char *path, int signal ) {
    //size error
    if(signal == -1){
        fprintf(stderr,"send 413\n");
        send_413(client_list, client);
        return;
    }
    //filename error
    if(signal == -2){
        fprintf(stderr,"send 501\n");
        send_501(client_list, client);
        return;
    }
    if(signal == -3){
        fprintf(stderr,"send 502\n");
        send_502(client_list, client);
        return;
    }
    if (strcmp(path, "/") == 0) path = "/index.html";
    if (strcmp(path, "/time") == 0){
        path = "/index.html";
        signal = -4;
    } 
    if (strlen(path) > 100) {
        fprintf(stderr,"send 400\n");
        send_400(client_list, client);
        return;
    }

    if (strstr(path, "..")) {
        fprintf(stderr,"send 404\n");
        send_404(client_list, client);
        return;
    }

    char full_path[128];
    sprintf(full_path, "public%s", path);

    FILE *fp = fopen(full_path, "rb");

    if (!fp) {
        fprintf(stderr,"send 404\n");
        send_404(client_list, client);
        return;
    }

    fseek(fp, 0L, SEEK_END);
    size_t cl = ftell(fp);
    rewind(fp);

    const char *ct = get_content_type(full_path);
    char buffer[1024];char input[1024];

    sprintf(buffer, "HTTP/1.1 200 OK\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Connection: close\r\n");
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Length: %lu\r\n", cl);
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Type: %s\r\n", ct);
    send(client->socket, buffer, strlen(buffer), 0);

    sprintf(buffer, "\r\n");
    send(client->socket, buffer, strlen(buffer), 0);
    //memset(buffer,'\0',1024);
    if(signal == -4){
        time_t timer;
        time(&timer);
        char *time_msg = ctime(&timer);
        send(client->socket, time_msg, strlen(time_msg), 0);
    }
    else{
        int r = fread(buffer, 1, 1024, fp);
        while (r) {
            send(client->socket, buffer, r, 0);
            r = fread(buffer, 1, 1024, fp);
        }
    }
    
    fclose(fp);
    drop_client(client_list, client);
}

int file_upload(char * package ,int size) {
    int limit=0;
    //get file type
    char* type = strstr(package,"filename=\"");
    if(type != NULL){
        char* type_start = strstr(type,"\"");
        int i=1;
        char TYPE[MAX_REQUEST_SIZE],c; 
        memset(TYPE,'\0',MAX_REQUEST_SIZE);
        while(c = type_start[i]){
            if(c == '\"')
                break;
            TYPE[i-1] = c;
            i++;
        }
        if(strcmp(TYPE,"") == 0)
            return -3;
        const char *last_dot = strrchr(TYPE, '.');
        char filemane[MAX_REQUEST_SIZE];                       //create file name+path
        memset(filemane,'\0',MAX_REQUEST_SIZE);
        FILE *fp;
        if (last_dot) {   //create a file
            if(strcmp(last_dot,".png") == 0 || strcmp(last_dot,".PNG") == 0){
                strcat(filemane,"public/upload");
                strcat(filemane,".png");
                fp = fopen(filemane, "wb");
            }
            else{
                strcat(filemane,"public/");
                strcat(filemane,TYPE);
                fp = fopen(filemane, "wbx");
            }
            if(!fp) return -2;
                             
            char* temp_ty = strstr(type,"Content-Type");
            if(temp_ty){
                char* temp = strstr(temp_ty,"\r\n");
                i = 0;
                char temp2[MAX_REQUEST_SIZE];
                memset(temp2,'\0',MAX_REQUEST_SIZE);
                int i=0; 
                // ignore /r/n/r/n    
                while (i<size){
                    if(i==0||i==1||i==2||i==3){
                        i++;
                        continue;
                    }
                    // find boundry
                    if(temp[i] =='-'&&temp[i+1] == '-'&&temp[i+2] == '-' &&temp[i+3] == '-'&&temp[i+4] == '-' &&temp[i+5] == '-'){
                        limit=1;break;
                    }
                        
                    temp2[i-4] = temp[i];
                    i++;  
                }
                // write inti new file
                int n = fwrite(temp2, 1,i-4,fp);
                if(n <= 0 )
                    fprintf(stderr,"store file error\n"); 
                fclose(fp);
            }
        }
        //work done
        printf("\ndone\n");
        
    }
    printf("%d\n",limit);
    return limit;
}

int main() {
    SOCKET server = create_socket(0, "8080");

    struct client_info *client_list = 0;

    while(1) {
        fd_set reads;
        reads = wait_on_clients(&client_list, server);

        if (FD_ISSET(server, &reads)) {
            struct client_info *client = get_client(&client_list, -1);
            client->socket = accept(server,
                    (struct sockaddr*) &(client->address),
                    &(client->address_length));

            if (!ISVALIDSOCKET(client->socket)) {
                fprintf(stderr, "accept() failed. (%d)\n",
                        GETSOCKETERRNO());
                return 1;
            }


            printf("New connection from %s.\n",
                    get_client_address(client));
        }


        struct client_info *client = client_list;
        while(client) {
            struct client_info *next = client->next;

            if (FD_ISSET(client->socket, &reads)) {
                printf("add %s \n\n", client->address_buffer);
                if (MAX_REQUEST_SIZE == client->received) {
                    send_400(&client_list, client);
                    client = next;
                    continue;
                }

                int r = recv(client->socket,
                        client->request + client->received,
                        MAX_REQUEST_SIZE - client->received, 0);
                if (r < 1) {
                    printf("Unexpected disconnect from %s.\n",
                            get_client_address(client));
                    drop_client(&client_list, client);

                } else {
                    client->received += r;
                    client->request[client->received] = 0;
                    uint8_t *q = strstr(client->request, "\r\n\r\n");
                    //int t = fwrite(q,client->received,1,stderr)
                    // store upload file
                    int state = file_upload(q,client->received);
                    if (q) {
                        *q = 0;
                        if (strncmp("GET /", client->request, 5) == 0 ) { //GET
                            char *path = client->request + 4;
                            char *end_path = strstr(path, " ");
                            if (!end_path) {
                                send_400(&client_list, client);
                            } 
                            else {
                                *end_path = 0;
                                serve_resource(&client_list, client, path,0);
                            }
                        } 
                        else if (strncmp("POST /", client->request, 6) == 0){  //POST
                            char *path = client->request + 5;
                            char *end_path = strstr(path, " ");
                            if (!end_path) {
                                send_400(&client_list, client);
                            } 
                            else {
                                *end_path = 0;
                                if(state == 0)
                                    serve_resource(&client_list, client, path , -1);
                                else if(state == -2)
                                    serve_resource(&client_list, client, path , -2);
                                else if(state == -3)
                                    serve_resource(&client_list, client, path , -3);
                                else
                                    serve_resource(&client_list, client, path,0);
                            }
                        }
                        else {
                            send_400(&client_list, client); 
                        }
                    } //if (q)
                }
            }
            client = next;
        }
    } //while(1)


    printf("\nClosing socket...\n");
    CLOSESOCKET(server);


#if defined(_WIN32)
    WSACleanup();
#endif

    printf("Finished.\n");
    return 0;
}