#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#define ISspace(x) isspace((int)(x))
#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
void error_die(const char *msg) {
    perror(msg);
    exit(1);
}
int get_line(int sock, char *buf, int size) {
    int i = 0, n; char c = '\0';
    while((i < size - 1) && (c != '\n')) {
        // 读一个字节的数据存放在 c 中
        n = recv(sock, &c, 1, 0);
        if(n > 0) {
            if(c == '\r') {
                n = recv(sock, &c, 1, MSG_PEEK);
                if((n > 0) && (c == '\n')) {
                    recv(sock, &c, 1, 0);
                }
                else {
                    c = '\n';
                }
            }
            buf[i] = c;
            i++;
        }
        else {
            c = '\n';
        }
    }
    buf[i] = '\0';
    return i;
}
// **************************** //

void unimplemented(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void not_found(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

void bad_request(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

void cannot_execute(int client) {
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

void headers(int client, const char * filename) {
    char buf[1024]; (void)filename;
    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

void cat(int client, FILE * resource) {
    char buf[1024];
    fgets(buf, sizeof(buf), resource);
    while(!feof(resource)) {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

void serve_file(int client, const char * filename) {
    FILE * resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while((numchars > 0) && strcmp("\n", buf)) {
        numchars = get_line(client, buf, sizeof(buf));
    }

    resource = fopen(filename, "r");
    if(resource == NULL) {
        not_found(client);
    }
    else {
        headers(client, filename);
        cat(client, resource);
    }
    fclose(resource);
}

void execute_cgi(int client, const char * path, const char * method, const char * query_string) {
    for(int j = 0; path[j] != '\0'; j++) {
        printf("%c\n", path[j]);
    }
    char buf[1024];
    int cgi_output[2], cgi_input[2];
    pid_t pid;
    int status; int i; char c; int numchars = 1; int content_length = -1;
    buf[0] = 'A'; buf[1] = '\0';
    if(strcasecmp(method, "GET") == 0) {
        while((numchars > 0) && strcmp("\n", buf)) {
            numchars = get_line(client, buf, sizeof buf);
        }
    }
    else {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf)) {
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                content_length = atoi(&(buf[16])); //记录 body 的长度大小
            numchars = get_line(client, buf, sizeof(buf));
        }
        if(content_length == -1) {
            bad_request(client);
            return;
        }
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);

    if(pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    if(pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }

    if((pid = fork()) < 0) {
        cannot_execute(client);
        return;
    }
    if(pid == 0) {
        char meth_env[255];
        char query_env[255];
        char length_env[255];

        dup2(cgi_output[1], 1);
        dup2(cgi_input[0], 0);
        close(cgi_output[0]);
        close(cgi_input[1]);

        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);

        if(strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        }
        else {
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }

        for(int j = 0; path[j] != '\0'; j++) {
            printf("%c\n", path[j]);
        }

        execl(path, path, NULL);
        exit(0);
    }
    else {
        close(cgi_output[1]);
        close(cgi_input[0]);

        if(strcasecmp(method, "POST") == 0) {
            for(i = 0; i < content_length; i++) {
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);
            }
        }
        while(read(cgi_output[0], &c, 1) > 0) {
            send(client, &c, 1, 0);
        }
        close(cgi_output[0]); 
        close(cgi_input[1]);
        waitpid(pid, &status, 0);
    }
}

void accept_request(int client) {
    int numchars;
    char buf[1024], method[255], url[255], path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;
    char *query_string = NULL;
    numchars = get_line(client, buf, sizeof(buf));
    i = 0, j = 0;
    while(!ISspace(buf[j]) && (i < sizeof(method) - 1)) {
        method[i] = buf[j];
        i++; j++;
    } 
    method[i] = '\0';
    if(strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
        unimplemented(client);
        return;
    }
    if(strcasecmp(method, "POST") == 0) {
        cgi = 1;
    }
    i = 0;
    while(ISspace(buf[j]) && (j < sizeof(buf))) j++;
    while(!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < sizeof(buf))) {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';
    if(strcasecmp(method, "GET") == 0) {
        query_string = url;
        while((*query_string != '?') && ((*query_string) != '\0')) query_string++;
        if(*query_string == '?') {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }
    sprintf(path, "htdocs%s", url);
    if(path[strlen(path) - 1] == '/') {
        strcat(path, "index.html");    
    }
    if(stat(path, &st) == -1) {
        while((numchars > 0) && strcmp("\n", buf)) numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else {
        if((st.st_mode & S_IFMT) == S_IFDIR) {
            strcat(path, "/index.html");
        }
        if((st.st_mode & S_IXUSR) || (st.st_mode & S_IXGRP) || (st.st_mode & S_IXOTH)) {
            cgi = 1;
        }
        if(!cgi) {
            serve_file(client, path);
        }
        else {
            execute_cgi(client, path, method, query_string);
        }
    }
    close(client);
}


// **************************** //

int startup(u_short * port) {
    int httpd = 0;
    struct sockaddr_in name;
    httpd = socket(PF_INET, SOCK_STREAM, 0); // socket(地址族, 套接字类型(服务于哪个协议), protocol(通常是 0,让它自己匹配)) PF_INET 和 AF_INET 同义
    if(httpd == -1) {
        error_die("socket");
    }
    // 初始化 name 套接字 
    memset(&name, 0, sizeof name);
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);
    if(bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0) {
        error_die("bind");
    }
    if(*port == 0) {
        int namelen = sizeof(name);
        if(getsockname(httpd, (struct sockaddr *)&name, (socklen_t *)&namelen) == -1) {
            error_die("getsockname");
        } // 给 name 的 sin_port 赋值一个端口号
        *port = ntohs(name.sin_port); // 网路序转字节序
    }
    if(listen(httpd, 5) < 0) { // 因为是服务端, 所以应该是监听模式
        error_die("listen");
    }
    return (httpd);
}
void handler(int sig) {
    pid_t id;
    while((id = waitpid(-1, NULL, WNOHANG)) > 0) { // while循环处理需要回收的子进程
        printf("wait child success : %d\n", id);
    }
}
int main() {
    signal(SIGCHLD, handler);
    int server_sock = -1;
    u_short port = 8080;
    int client_sock = -1;
    struct sockaddr_in client_name;
    // sockaddr_in
    // short            sin_family;   // 地址族 对于 IPv4 地址，通常设置为 AF_INET
    // unsigned short   sin_port;     // 端口号 htons(端口的地址)
    // struct in_addr   sin_addr;     // IP 地址 htonl(INADDR_ANY) 本机的 IP
    // char             sin_zero[8];  // 填充字段 
    int client_name_len = sizeof(client_name);
    server_sock = startup(&port); // 服务端的套接字, 此时已经和底层连接起来了
    printf("httpd running on port %d\n", port);

    while(1) {
        // 阻塞等待客户端的连接
        client_sock = accept(server_sock, (struct sockaddr *)&client_name, (socklen_t *)&client_name_len);
        // 这相当于把 client 的套接字 和底层连起来, 并且和服务端连起来
        if(client_sock == -1) {
            error_die("accept");
        }

        pid_t pid = fork();
        if(pid < 0) {
            error_die("fork");
        }
        else if(pid == 0) {
            close(server_sock);
            accept_request(client_sock);
            close(client_sock);
            exit(0);
        }
        else {
            close(client_sock);
        }
    }
    close(server_sock);
    return 0;
}