#ifndef SIP_MOD_H
#define SIP_MOD_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <netdb.h>
#include <sys/select.h>
#include <pthread.h>
#include <math.h>
#include <time.h>
#include "dzin_io.h"
#define DO 0xfd
#define WONT 0xfc
#define WILL 0xfb
#define DONT 0xfe
#define CMD 0xff
#define CMD_WINDOW_SIZE 31
#define BUFLEN 20096
#define MAX_REDIRECTS 3

#define RESET   "\033[0m"
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[1;36m"
#define MAGENTA "\033[1;35m"
#define BOLD    "\033[1m"

#define info(...)  printf(GREEN __VA_ARGS__); printf(RESET)
#define warn(...)  printf(YELLOW __VA_ARGS__); printf(RESET)
#define err(...)   fprintf(stderr, RED __VA_ARGS__); fprintf(stderr, RESET)
#define debug(...) printf(CYAN __VA_ARGS__); printf(RESET)
void dzin_ensure_android_path2(void) {
    const char *cur = getenv("PATH");
    const char *base = "/system/bin:/system/xbin:/busybox-virtual:"
                       "/data/user/0/com.n0n3m4.droidc/files:"
                       "/data/user/0/com.n0n3m4.droidc/files/gcc/bin";
    if (!cur || !*cur) {
        setenv("PATH", base, 1);
        return;
    }
    if (!strstr(cur, "/system/bin")) {
        char combined[1024];
        snprintf(combined, sizeof(combined), "%s:%s", base, cur);
        setenv("PATH", combined, 1);
    }
}
#define START_PORT 1
#define END_PORT 1024
#define TIMEOUT_SEC 0
#define TIMEOUT_USEC 200000  /* 200ms */
extern void push_to_global_output(const char* line);
static const char *sip_target_ip = NULL;
extern unsigned char *onnx_model_buffer;
static size_t sip_onnx_model_size = 0;

extern pthread_mutex_t ocean_mutex;
extern pthread_mutex_t evo_mutex;
static unsigned int sip_global_binary_ocean_seed = 133721;
static size_t sip_ocean_index = 0;

/* --- STRUKTURY MICRO-NN (CHEMIA AI) --- */
typedef struct {
    float w_hidden[3][4];
    float b_hidden[4];
    float w_output[4];
    float b_output;
    int generation;
} SipMicroNeuralNet;

static SipMicroNeuralNet sip_trained_model = {
    .w_hidden = { { 1.2f, -0.5f,  2.1f,  0.1f },
                  {-0.8f,  1.5f, -0.3f,  0.9f },
                  { 0.4f,  0.2f,  1.1f, -1.2f } },
    .b_hidden = { -0.1f, 0.0f, 0.2f, -0.5f },
    .w_output = { 1.5f, 2.0f, -1.1f, 0.8f },
    .b_output = -0.7f,
    .generation = 1
};

static SipMicroNeuralNet sip_reverse_model = {
    .w_hidden = { {-0.5f,  1.2f, -1.1f,  0.4f },
                  { 1.1f, -0.8f,  0.3f, -1.5f },
                  {-0.2f,  0.4f, -1.9f,  0.8f } },
    .b_hidden = {  0.2f, -0.1f, -0.4f,  0.3f },
    .w_output = { -1.1f, 1.8f,  0.9f, -0.5f },
    .b_output =  0.4f,
    .generation = 1
};

static const char *sip_user_agents[] = {
    "Mozilla/5.0 () { : ;};%s\n"
};
static const int sip_num_uas = 1;

static inline void sip_mutate_micro_nn(SipMicroNeuralNet *net, float mutation_intensity) {
    if (!net) return;
    pthread_mutex_lock(&evo_mutex);
    net->generation++;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            net->w_hidden[i][j] += (((float)rand() / (float)RAND_MAX) - 0.5f) * mutation_intensity;
        }
    }
    for (int j = 0; j < 4; j++) {
        net->b_hidden[j] += (((float)rand() / (float)RAND_MAX) - 0.5f) * mutation_intensity;
        net->w_output[j] += (((float)rand() / (float)RAND_MAX) - 0.5f) * mutation_intensity;
    }
    net->b_output += (((float)rand() / (float)RAND_MAX) - 0.5f) * mutation_intensity;
    pthread_mutex_unlock(&evo_mutex);
}

static inline float sip_relu(float x) {
    return x > 0.0f ? x : 0.0f;
}

static inline float sip_sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

static inline float sip_ai_evaluate_response(int status_code, size_t response_len, int is_reflected, SipMicroNeuralNet *model) {
    if (!model) return 0.0f;
    float inputs[3];
    inputs[0] = (float)status_code / 500.0f;
    inputs[1] = (float)response_len / 10000.0f;
    inputs[2] = is_reflected ? 1.0f : 0.0f;

    float hidden[4];
    for (int j = 0; j < 4; j++) {
        float sum = model->b_hidden[j];
        for (int i = 0; i < 3; i++) sum += inputs[i] * model->w_hidden[i][j];
        hidden[j] = sip_relu(sum);
    }

    float output_sum = model->b_output;
    for (int j = 0; j < 4; j++) output_sum += hidden[j] * model->w_output[j];

    return sip_sigmoid(output_sum);
}

static inline int sip_check_water_molecule_resonance(float score_main, float score_reverse, float *out_angle) {
    float val = fminf(fmaxf(score_main * score_reverse, -1.0f), 1.0f);
    float angle = acosf(val) * (180.0f / 3.14159f);
    if (out_angle) *out_angle = angle;
    return (angle >= 98.0f && angle <= 111.0f) ? 1 : 0;
}

/* Decyzja Dżina oparta na oceanie ONNX zamiast sztywnego warunku if */
static inline int sip_dzin_decide_action(unsigned char ocean_byte, int threshold) {
    return ((int)(ocean_byte % 100) < threshold) ? 1 : 0;
}

/* Tryb surowego Telnetu (interaktywny strumień socketu) */
static inline void sip_handle_telnet_session(int sock, FILE *fp) {
    info("🔌 [Dżin Telnet Mode]: Połączono z surowym gniazdem. Nasłuchiwanie strumienia...\n");
    char buffer[4096];
    fd_set readfds;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        int max_fd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;
        struct timeval tv = { .tv_sec = 5, .tv_usec = 0 };

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0) break;

        if (FD_ISSET(sock, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes <= 0) {
            	push_to_global_output("\n[-] Połączenie zamknięte przez serwer.\n");
                warn("\n[-] Połączenie zamknięte przez serwer.\n");
                break;
            }
            printf("%s", buffer);
            push_to_global_output(buffer);
            if (fp) {
            	
                fprintf(fp, "%s", buffer);
                fflush(fp);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            if (!fgets(buffer, sizeof(buffer), stdin)) break;
            if (send(sock, buffer, strlen(buffer), 0) < 0) break;
        }
    }
}

static inline void sip_load_onnx_matrix(const char *filename) {
    pthread_mutex_lock(&ocean_mutex);
    if (onnx_model_buffer) {
        free(onnx_model_buffer);
        onnx_model_buffer = NULL;
        sip_onnx_model_size = 0;
    }
    sip_ocean_index = 0;

    FILE *f = fopen(filename, "rb");
    if (!f) {
        warn("[-] Brak pliku %s. Tworzenie matrycy zastępczej w RAM...\n", filename);
        sip_onnx_model_size = 4304;
        onnx_model_buffer = (unsigned char *)malloc(sip_onnx_model_size);
        if (onnx_model_buffer) memset(onnx_model_buffer, 0xAA, sip_onnx_model_size);
        pthread_mutex_unlock(&ocean_mutex);
        return;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz > 0) {
        sip_onnx_model_size = (size_t)sz;
        onnx_model_buffer = (unsigned char *)malloc(sip_onnx_model_size);
        if (onnx_model_buffer) {
            size_t got = fread(onnx_model_buffer, 1, sip_onnx_model_size, f);
            sip_onnx_model_size = got;
        }
    }
    fclose(f);
    pthread_mutex_unlock(&ocean_mutex);
}

static inline void *sip_scan_port(void *arg) {
    int port = *(int *)arg;
    free(arg);

    if (!sip_target_ip) pthread_exit(NULL);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) pthread_exit(NULL);

    struct timeval timeout = { .tv_sec = TIMEOUT_SEC, .tv_usec = TIMEOUT_USEC };
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons((uint16_t)port);
    target.sin_addr.s_addr = inet_addr(sip_target_ip);

    if (connect(sockfd, (struct sockaddr *)&target, sizeof(target)) == 0) {
    	
        printf("Port %d jest OTWARTY\n", port);
    }
    close(sockfd);
    pthread_exit(NULL);
}

static inline int sip_scan(const char *ip) {
    sip_target_ip = ip;
    info("Skanowanie hosta %s (1-1024)...\n", sip_target_ip);
    pthread_t threads[1024];
    int count = 0;
    for (int p = START_PORT; p <= END_PORT; p++) {
        int *arg = (int *)malloc(sizeof(int));
        if (arg) {
            *arg = p;
            if (pthread_create(&threads[count], NULL, sip_scan_port, arg) == 0) count++;
            else free(arg);
        }
    }
    for (int i = 0; i < count; i++) pthread_join(threads[i], NULL);
    return 0;
}

static inline int sip_hostname_to_ip(const char *hostname, char *ip, size_t ip_sz) {
    if (!hostname || !ip || ip_sz == 0) return 1;
    struct hostent *he = gethostbyname(hostname);
    if (!he) return 1;
    struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
    if (addr_list && addr_list[0]) {
        snprintf(ip, ip_sz, "%s", inet_ntoa(*addr_list[0]));
        return 0;
    }
    return 1;
}

static inline void sip_parse_url(const char *url, char *hostname, size_t host_sz,
                                 char *path, size_t path_sz, int *port) {
    if (!url || !hostname || !path || !port || host_sz == 0 || path_sz == 0) return;
    const char *p = url;
    *port = 80;
    if (strncmp(p, "https://", 8) == 0) {
        *port = 443;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        *port = 80;
        p += 7;
    }

    const char *host_end = strchr(p, '/');
    const char *port_ptr = strchr(p, ':');

    if (port_ptr && (!host_end || port_ptr < host_end)) {
        *port = atoi(port_ptr + 1);
        size_t len = (size_t)(port_ptr - p);
        if (len >= host_sz) len = host_sz - 1;
        memcpy(hostname, p, len);
        hostname[len] = '\0';
    } else if (host_end) {
        size_t len = (size_t)(host_end - p);
        if (len >= host_sz) len = host_sz - 1;
        memcpy(hostname, p, len);
        hostname[len] = '\0';
    } else {
        snprintf(hostname, host_sz, "%s", p);
    }

    if (host_end && *host_end != '\0') {
        snprintf(path, path_sz, "%s", host_end);
    } else {
        snprintf(path, path_sz, "/");
    }
}

static inline const char *sip_usage_str(const char *prog_name, char *buf, size_t buf_sz) {
    if (!buf || buf_sz == 0) return "";
    const char *name = (prog_name && prog_name[0] != '\0') ? prog_name : "sip";

    snprintf(
        buf,
        buf_sz,
        GREEN "Użycie: %s <URL lub IP> [opcje]\n\n" RESET
        BOLD "Metody i Tryby:\n" RESET
        "  -p          Metoda POST\n"
        "  -P          Metoda PUT\n"
        "  -d          Metoda DELETE\n"
        "  -o          Metoda OPTIONS\n"
        "  -t [port]   Surowy tryb Telnet / Socket (domyślnie port 80)\n"
        "  -s          Skanowanie portów TCP 1-1024\n"
        "  -h          Wyświetl pomoc\n",
        name
    );

    return buf;
}

static inline void sip_cleanup_buffer(void) {
    pthread_mutex_lock(&ocean_mutex);
    if (onnx_model_buffer) {
        free(onnx_model_buffer);
        onnx_model_buffer = NULL;
        sip_onnx_model_size = 0;
    }
    pthread_mutex_unlock(&ocean_mutex);
}

static inline int sip_main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));
    dzin_ensure_android_path2(); sip_load_onnx_matrix("network_anomaly.onnx");

    if (argc < 2) {
//        sip_usage_str(argv && argv[0] ? argv[0] : "sip");
        sip_cleanup_buffer();
        return 1;
    }

    const char *target_arg = NULL;
    int custom_port = -1;
    int telnet_mode = 0;
    char method[16] = "GET";
    char ua_param[256] = "id";
    int scan_mode = 0;
  int curl_mode = 0;
     char curl_domain[256] = "";
    for (int i = 1; i < argc; i++) {
        if (!argv[i]) continue;
        if (strcmp(argv[i], "-p") == 0) snprintf(method, sizeof(method), "POST");
        else if (strcmp(argv[i], "-d") == 0) snprintf(method, sizeof(method), "DELETE");
        else if (strcmp(argv[i], "-P") == 0) snprintf(method, sizeof(method), "PUT");
        else if (strcmp(argv[i], "-o") == 0) snprintf(method, sizeof(method), "OPTIONS");
        else if (strcmp(argv[i], "-t") == 0) {
            telnet_mode = 1;
            custom_port = 80;
            if (i + 1 < argc && argv[i + 1] && argv[i + 1][0] != '-') {
                custom_port = atoi(argv[i + 1]);
                i++;
            }
        } else if (strcmp(argv[i], "-s") == 0) {
            scan_mode = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            curl_mode = 1;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                strncpy(curl_domain, argv[i + 1], sizeof(curl_domain) - 1);
                i++;
            }
        }
        else if (strcmp(argv[i], "-u") == 0) {
            if (i + 1 < argc && argv[i + 1]) {
                snprintf(ua_param, sizeof(ua_param), "%s", argv[i + 1]);
                i++;
            }
        } else if (strcmp(argv[i], "-h") == 0) {
char help_buf[1024];
            const char *help_text = sip_usage_str(argv[0], help_buf, sizeof(help_buf));
            push_to_global_output(help_text);

            sip_cleanup_buffer();
    return 0;
            sip_cleanup_buffer();
            return 0;
        } else if (!target_arg) {
            target_arg = argv[i];
        }
    }

    FILE *fp = fopen("sip.html", "w");
    if (fp) {
        fprintf(fp, "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Dzin H2O Log</title></head><body style='background:#111;color:#0f0;font-family:monospace;'>\n");
        fprintf(fp, "<h2>--- [DZIN H2O SESSION LOG] ---</h2>\n");
        fflush(fp);
    }

    if (!target_arg) {
        err("Brak celu!\n");
        //sip_usage(argv[0]);
        if (fp) fclose(fp);
        sip_cleanup_buffer();
        return 1;
    }

    char hostname[256], path[BUFLEN];
    int port = 80;
    sip_parse_url(target_arg, hostname, sizeof(hostname), path, sizeof(path), &port);
    if (custom_port != -1) port = custom_port;

    char ip_str[INET_ADDRSTRLEN];
    if (sip_hostname_to_ip(hostname, ip_str, sizeof(ip_str)) != 0) {
        err("Nie można rozwiązać hosta.\n");
        if (fp) fclose(fp);
        sip_cleanup_buffer();
        return 1;
    }if (curl_mode) {
        char command[1024];
        char buffer[1024];

        const char *target_url = NULL;
        if (curl_domain[0] != '\0') {
            target_url = curl_domain;
        } else if (target_arg != NULL) {
            target_url = target_arg;
        }

        if (target_url == NULL || target_url[0] == '\0') {
            err("Błąd: Tryb cURL (-c) wymaga podania adresu URL lub domeny!\n");
            if (fp) fclose(fp);
            sip_cleanup_buffer();
            return 1;
        }

        /* Mutacja seedu z oceanu ONNX (bezpieczna wątkowo) */
        unsigned int ai_seed = 0;
        unsigned char decision_byte = 0x00;

        pthread_mutex_lock(&ocean_mutex);
        ai_seed = global_binary_ocean_seed;
        global_binary_ocean_seed = (global_binary_ocean_seed >> 3) ^ (global_binary_ocean_seed << 5);

        if (onnx_model_buffer && onnx_model_size > 0) {
            decision_byte = onnx_model_buffer[ocean_index % onnx_model_size];
            global_binary_ocean_seed ^= decision_byte;
            ocean_index += 7;
        }
        pthread_mutex_unlock(&ocean_mutex);

        /* Bezpieczne budowanie flag curl (bez -A i -b bez argumentu) */
        char adaptive_flags[128] = "-s";
        if (decision_byte % 2 == 0) {
            strncat(adaptive_flags, " -L", sizeof(adaptive_flags) - strlen(adaptive_flags) - 1);
        }
        if (decision_byte % 3 == 0) {
            strncat(adaptive_flags, " -k", sizeof(adaptive_flags) - strlen(adaptive_flags) - 1);
        }
        if (decision_byte % 5 == 0) {
            strncat(adaptive_flags, " -G", sizeof(adaptive_flags) - strlen(adaptive_flags) - 1);
        }
        if (decision_byte % 6 == 0) {
            strncat(adaptive_flags, " --keepalive-time 5", sizeof(adaptive_flags) - strlen(adaptive_flags) - 1);
        }

        info("🧞 [Dżin - AI Core]: Analizuję stan oceanu ONNX...\n");
        debug("   ├── Seed decyzyjny: 0x%X (Wartość wariacji: %u)\n", decision_byte, ai_seed);
        debug("   ├── Wybrane przez sieć flagi cURL: [%s]\n", adaptive_flags);
        info("   └── Cel operacji: %s\n\n", target_url);

        /* Proste zabezpieczenie przed cudzysłowem w URL */
        char safe_url[512];
        size_t u_len = 0;
        for (size_t k = 0; target_url[k] != '\0' && u_len + 2 < sizeof(safe_url); k++) {
            if (target_url[k] == '"' || target_url[k] == '\\' || target_url[k] == '$') {
                continue;
            }
            safe_url[u_len++] = target_url[k];
        }
        safe_url[u_len] = '\0';

        int written = 0;
        if (strncmp(safe_url, "http://", 7) == 0 || strncmp(safe_url, "https://", 8) == 0) {
            written = snprintf(command, sizeof(command), "curl %s \"%s\" 2>&1", adaptive_flags, safe_url);
        } else {
            written = snprintf(command, sizeof(command), "curl %s \"https://%s\" 2>&1", adaptive_flags, safe_url);
        }

        if (written < 0 || (size_t)written >= sizeof(command)) {
            err("Błąd: Polecenie cURL jest zbyt długie.\n");
            if (fp) fclose(fp);
            sip_cleanup_buffer();
            return 1;
        }

        FILE *fp_curl = popen(command, "r");
        if (fp_curl == NULL) {
            push_to_global_output("Błąd: Nie udało się uruchomić popen dla cURL\n");
            err("Błąd: Nie udało się uruchomić popen dla cURL\n");
            if (fp) fclose(fp);
            sip_cleanup_buffer();
            return 1;
        }

        size_t total_out = 0;
        const size_t MAX_CURL_OUTPUT = 262144; /* 256 KB limit na jedną odpowiedź w pamięci */

        while (fgets(buffer, sizeof(buffer), fp_curl) != NULL) {
            size_t chunk_len = strlen(buffer);
            if (total_out + chunk_len > MAX_CURL_OUTPUT) {
                const char *trunc_msg = "\n[!] Odpowiedź przekroczyła 256 KB — obcięto resztę dla bezpieczeństwa.\n";
                push_to_global_output(trunc_msg);
                printf("%s", trunc_msg);
                if (fp) {
                    fprintf(fp, "%s", trunc_msg);
                    fflush(fp);
                }
                break;
            }
            total_out += chunk_len;

            push_to_global_output(buffer);
            printf("%s", buffer);
            if (fp) {
                fprintf(fp, "%s", buffer);
                fflush(fp);
            }
        }

        int status = pclose(fp_curl);
        debug("\n   └── Kod zakończenia powłoki/cURL: %d\n", status);

        if (fp) fclose(fp);
        sip_cleanup_buffer();
        info("\n🧞 [Dżin]: Zapytanie cURL zakończone. Wracam do lampy.\n");
        return 0;
    }
    if (scan_mode) {
        sip_scan(ip_str);
        
push_to_global_output(ip_str);
        if (fp) fclose(fp);
        sip_cleanup_buffer();
        return 0;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Błąd socket");
        if (fp) fclose(fp);
        sip_cleanup_buffer();
        return 1;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons((uint16_t)port);
    server.sin_addr.s_addr = inet_addr(ip_str);

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("Błąd connect");
        close(sock);
        if (fp) fclose(fp);
        sip_cleanup_buffer();
        return 1;
    }

    unsigned char djin_decision_byte = 0xAF;
    pthread_mutex_lock(&ocean_mutex);
    if (onnx_model_buffer && sip_onnx_model_size > 0) {
        djin_decision_byte = onnx_model_buffer[sip_ocean_index % sip_onnx_model_size];
        sip_ocean_index++;
    }
    pthread_mutex_unlock(&ocean_mutex);

    if (telnet_mode) {
        sip_handle_telnet_session(sock, fp);
    } else {
        struct timeval rcv_timeout = { .tv_sec = 5, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &rcv_timeout, sizeof(rcv_timeout));

        char final_ua[512];
        snprintf(final_ua, sizeof(final_ua), sip_user_agents[sip_num_uas - 1], ua_param);
        char http_request[4096];
        const char *body = "probe=active_anomaly";

        if (sip_dzin_decide_action(djin_decision_byte, 50)) {
            debug("🧞 [Decyzja Dżina]: Wstrzykiwanie morficznej zmiennej rezonansowej do nagłówka.\n");
        }

        snprintf(http_request, sizeof(http_request),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: %s\r\n"
            "Content-Length: %ld\r\n"
            "Connection: close\r\n\r\n"
            "%s",
            method, path, hostname, final_ua, (long)strlen(body), body);

        if (fp) {
            fprintf(fp, "<h3>[ŻĄDANIE DO: %s:%d%s]</h3><pre>", hostname, port, path);
            for (size_t k = 0; http_request[k] != '\0'; k++) {
                if (http_request[k] == '<') fputs("&lt;", fp);
                else if (http_request[k] == '>') fputs("&gt;", fp);
                else if (http_request[k] == '&') fputs("&amp;", fp);
                else fputc(http_request[k], fp);
            }
            fprintf(fp, "</pre><hr>");
            fflush(fp);
        }

        size_t req_len = strlen(http_request);
        size_t sent_total = 0;
        while (sent_total < req_len) {
            ssize_t sent = send(sock, http_request + sent_total, req_len - sent_total, 0);
            if (sent <= 0) break;
            sent_total += (size_t)sent;
        }

        char recv_buf[4096];
        ssize_t bytes;
        size_t total_recv_bytes = 0;

        if (fp) {
            fprintf(fp, "<h3>[ODPOWIEDŹ]</h3><pre>");
        }

        while ((bytes = recv(sock, recv_buf, sizeof(recv_buf) - 1, 0)) > 0) {
            recv_buf[bytes] = '\0';
            total_recv_bytes += (size_t)bytes;
            fwrite(recv_buf, 1, (size_t)bytes, stdout);
            
push_to_global_output(recv_buf);
            if (fp) {
                for (ssize_t idx = 0; idx < bytes; idx++) {
                    unsigned char ch = (unsigned char)recv_buf[idx];
                    if (ch == '<') fputs("&lt;", fp);
                    else if (ch == '>') fputs("&gt;", fp);
                    else if (ch == '&') fputs("&amp;", fp);
                    else fputc((int)ch, fp);
                }
                fflush(fp);
            }
        }

        if (fp) {
        	info("zapiaono do pliku sip.html\n");
            fprintf(fp, "</pre><p>Odebrano bajtów: %zu</p><hr>", total_recv_bytes);
            fflush(fp);
        }
    }
    close(sock);
        if (fp) fclose(fp);
    sip_cleanup_buffer();
    info("Zakończono pomyślnie.\n");
    return 0;
}

#endif /* SIP_MOD_H */