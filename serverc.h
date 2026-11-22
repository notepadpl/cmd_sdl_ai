#ifndef SERVERC_H
#define SERVERC_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <pthread.h>
#include <time.h>

#define BUFSIZE 1024

#define RESET   "\033[0m"
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[1;36m"
#define BOLD    "\033[1m"

#define info(...)  printf(GREEN __VA_ARGS__); printf(RESET)
#define warn(...)  printf(YELLOW __VA_ARGS__); printf(RESET)
#define err(...)   fprintf(stderr, RED __VA_ARGS__); fprintf(stderr, RESET)
#define debug(...) printf(CYAN __VA_ARGS__); printf(RESET)

// Zmienne globalne jako static (działają w pliku nagłówkowym bez konfliktów extern)
static int port = 8080;
static unsigned char *onnx_model_buffer1 = NULL;
static size_t oonnx_model_size = 0;
static pthread_mutex_t oocean_mutex = PTHREAD_MUTEX_INITIALIZER;
static size_t oocean_index = 0;

 void load_onnx_matrix(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        warn("[-] Nie znaleziono pliku %s. Tworzenie matrycy zastępczej w RAM...\n", filename);
        oonnx_model_size = 4304;
        onnx_model_buffer1 = (unsigned char *)malloc(oonnx_model_size);
        if (onnx_model_buffer1) {
            memset(onnx_model_buffer1, 0x55, oonnx_model_size);
        }
        return;
    }

    fseek(f, 0, SEEK_END);
    oonnx_model_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    onnx_model_buffer1 = (unsigned char *)malloc(oonnx_model_size);
    if (onnx_model_buffer1) {
        fread(onnx_model_buffer1, 1, oonnx_model_size, f);
        info("🧞 [Dżin - Serwer]: Pomyślnie wczytano matrycę '%s' do RAM (%zu bajtów)\n", filename, oonnx_model_size);
    }
    fclose(f);
}

 void cerror(FILE *stream, char *cause, char *errno_str, char *shortmsg, char *longmsg) {
    fprintf(stream, "HTTP/1.1 %s %s\nContent-type: text/html\n\n", errno_str, shortmsg);
    fprintf(stream, "<html><body><h1>%s: %s</h1><p>%s: %s</p></body></html>", errno_str, shortmsg, longmsg, cause);
    fflush(stream);
}

 void dzin_log_thought(const char *client_ip, const char *method, const char *uri, unsigned char b1, unsigned char b2, size_t index) {
    FILE *log = fopen("log.txt", "a");
    if (!log) return;

    fprintf(log, "[Zeszyt Dżina] Indeks oceanu: %zu\n", index);
    fprintf(log, "  ├── Ruch: IP: %s | Metoda: %s | URI: %s\n", client_ip, method, uri);
    fprintf(log, "  ├── Genetyka ONNX: Bajt [0x%02X] | Wariacja [0x%02X]\n", b1, b2);
    fprintf(log, "  └── Przemyślenie: ");
    if (b1 > 0xAA) {
        fprintf(log, "Wysoka entropia matrycy. Endpoint %s bada strukturę systemu.\n", uri);
    } else if (b1 == 0x00) {
        fprintf(log, "Martwa strefa oceanu. Cichy ruch tła.\n");
    } else {
        fprintf(log, "Standardowy wzorzec żądania HTTP. Brak anomalii krytycznych.\n");
    }
    fprintf(log, "--------------------------------------------------\n");
    fclose(log);
}

 void serve_directory(FILE *stream, char *dirname, const char *request_uri) {
    DIR *dir = opendir(dirname);
    if (!dir) {
        cerror(stream, dirname, "403", "Forbidden", "Cannot read directory");
        return;
    }

    fprintf(stream, "HTTP/1.1 200 OK\nContent-type: text/html\n\n");
    fprintf(stream, "<html><body><h1>Index of %s</h1><ul>", request_uri);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        
        if (strcmp(request_uri, "/") == 0) {
            fprintf(stream, "<li><a href=\"/%s\">%s</a></li>", entry->d_name, entry->d_name);
        } else {
            size_t len = strlen(request_uri);
            if (request_uri[len - 1] == '/') {
                fprintf(stream, "<li><a href=\"%s%s\">%s</a></li>", request_uri, entry->d_name, entry->d_name);
            } else {
                fprintf(stream, "<li><a href=\"%s/%s\">%s</a></li>", request_uri, entry->d_name, entry->d_name);
            }
        }
    }
    fprintf(stream, "</ul></body></html>");
    fflush(stream);
    closedir(dir);
}

 void handle_post(FILE *stream, const char *uri, int content_length, FILE *client_socket_file) {
    char filepath[BUFSIZE];
    snprintf(filepath, sizeof(filepath), "./uploads");
    mkdir(filepath, 0777);

    time_t now = time(NULL);
    snprintf(filepath, sizeof(filepath), "./uploads/post_%ld.txt", now);

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        cerror(stream, filepath, "500", "Internal Error", "Could not create file for POST");
        return;
    }

    char *body_buf = (char *)malloc(content_length);
    if (body_buf) {
        int total_read = 0;
        while (total_read < content_length) {
            int n = fread(body_buf + total_read, 1, content_length - total_read, client_socket_file);
            if (n <= 0) break;
            total_read += n;
        }
        fwrite(body_buf, 1, total_read, f);
        free(body_buf);
    }
    fclose(f);

    fprintf(stream, "HTTP/1.1 201 Created\nContent-Type: text/html\n\n");
    fprintf(stream, "<html><body><h1>201 Created</h1><p>POST data successfully saved to %s</p></body></html>", filepath);
    fflush(stream);
}

 void handle_put(FILE *stream, const char *uri, int content_length, FILE *client_socket_file) {
    char filename[BUFSIZE];
    snprintf(filename, sizeof(filename), ".%s", uri);

    FILE *f = fopen(filename, "wb");
    if (!f) {
        cerror(stream, filename, "500", "Internal Error", "Could not open file for PUT");
        return;
    }

    char *body_buf = (char *)malloc(content_length);
    if (body_buf) {
        int total_read = 0;
        while (total_read < content_length) {
            int n = fread(body_buf + total_read, 1, content_length - total_read, client_socket_file);
            if (n <= 0) break;
            total_read += n;
        }
        fwrite(body_buf, 1, total_read, f);
        free(body_buf);
    }
    fclose(f);

    fprintf(stream, "HTTP/1.1 200 OK\nContent-Type: text/html\n\n");
    fprintf(stream, "<html><body><h1>200 OK</h1><p>Resource %s successfully updated/created via PUT</p></body></html>", uri);
    fflush(stream);
}

 void handle_delete(FILE *stream, const char *uri) {
    char filename[BUFSIZE];
    snprintf(filename, sizeof(filename), ".%s", uri);

    struct stat sbuf;
    if (stat(filename, &sbuf) < 0) {
        cerror(stream, filename, "404", "Not Found", "Resource not found for deletion");
        return;
    }

    if (S_ISDIR(sbuf.st_mode)) {
        cerror(stream, filename, "403", "Forbidden", "Cannot delete a directory");
        return;
    }

    if (unlink(filename) == 0) {
        fprintf(stream, "HTTP/1.1 200 OK\nContent-Type: text/html\n\n");
        fprintf(stream, "<html><body><h1>200 OK</h1><p>Resource %s deleted successfully</p></body></html>", uri);
    } else {
        cerror(stream, filename, "500", "Internal Error", "Could not delete resource");
    }
    fflush(stream);
}

 int serverc(int argc, char **argv) {
    if (argc == 2) {
        port = atoi(argv[1]);
    }

    load_onnx_matrix("network_anomaly.onnx");

    int parentfd, childfd, optval;
    socklen_t clientlen;
    struct sockaddr_in serveraddr, clientaddr;
    FILE *stream;
    char buf[BUFSIZE], method[BUFSIZE], uri[BUFSIZE], version[BUFSIZE];
    char filename[BUFSIZE], filetype[BUFSIZE];
    struct stat sbuf;

    parentfd = socket(AF_INET, SOCK_STREAM, 0);
    optval = 1;
    setsockopt(parentfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    bzero((char *)&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons((unsigned short)port);
    bind(parentfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
    listen(parentfd, 5);

    info("🧞 [Dżin - Serwer HTTP] nasłuchuje na porcie 127.0.0.1:%d...\n", port);

    while (1) {
        clientlen = sizeof(clientaddr);
        childfd = accept(parentfd, (struct sockaddr *)&clientaddr, &clientlen);
        stream = fdopen(childfd, "r+");

        if (fgets(buf, BUFSIZE, stream) == NULL) { 
            fclose(stream); 
            continue; 
        }
        
        sscanf(buf, "%s %s %s", method, uri, version);
        
        int content_length = 0;
        while (fgets(buf, BUFSIZE, stream) != NULL && strcmp(buf, "\r\n") && strcmp(buf, "\n")) {
            if (strncasecmp(buf, "Content-Length:", 15) == 0) {
                content_length = atoi(buf + 15);
            }
        }

        unsigned char anomaly_byte1 = 0x00;
        unsigned char anomaly_byte2 = 0x00;
        
        pthread_mutex_lock(&oocean_mutex);
        if (onnx_model_buffer1 && oonnx_model_size > 0) {
            anomaly_byte1 = onnx_model_buffer1[oocean_index % oonnx_model_size];
            anomaly_byte2 = onnx_model_buffer1[(oocean_index + 13) % oonnx_model_size];
            oocean_index += 5;
        }
        pthread_mutex_unlock(&oocean_mutex);
        
        char *client_ip = inet_ntoa(clientaddr.sin_addr);
        printf(CYAN "[*] Ruch przychodzący | IP: %s | Metoda: %s | URI: %s\n" RESET, client_ip, method, uri);
        printf(YELLOW "    └── [ONNX Anomaly Core] Bajt decyzyjny: 0x%02X | Współczynnik wariacji: 0x%02X (Indeks: %zu)\n" RESET, 
               anomaly_byte1, anomaly_byte2, oocean_index);

        dzin_log_thought(client_ip, method, uri, anomaly_byte1, anomaly_byte2, oocean_index);

        if (strcasecmp(method, "GET") == 0) {
            strcpy(filename, ".");
            strcat(filename, uri);

            if (stat(filename, &sbuf) < 0) {
                cerror(stream, filename, "404", "Not found", "Couldn't find this file");
            } else if (S_ISDIR(sbuf.st_mode)) {
                serve_directory(stream, filename, uri);
            } else {
                if (strstr(filename, ".html")) strcpy(filetype, "text/html");
                else strcpy(filetype, "text/plain");

                fprintf(stream, "HTTP/1.1 200 OK\nServer: Dzin-Anomaly-Server\nContent-length: %d\nContent-type: %s\n\n", (int)sbuf.st_size, filetype);
                fflush(stream);
                
                int fd = open(filename, O_RDONLY);
                void *p = mmap(0, sbuf.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
                fwrite(p, 1, sbuf.st_size, stream);
                munmap(p, sbuf.st_size);
                close(fd);
            }
        } 
        else if (strcasecmp(method, "POST") == 0) {
            handle_post(stream, uri, content_length, stream);
        } 
        else if (strcasecmp(method, "PUT") == 0) {
            handle_put(stream, uri, content_length, stream);
        } 
        else if (strcasecmp(method, "DELETE") == 0) {
            handle_delete(stream, uri);
        } 
        else {
            cerror(stream, method, "501", "Not Implemented", "Tiny does not implement this method");
        }
        
        fclose(stream);
    }
    return 0;
}

#endif // SERVERC_H