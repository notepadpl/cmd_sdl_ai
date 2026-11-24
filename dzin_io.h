#ifndef DZIN_IO_H
#define DZIN_IO_H

#include "dzin_server.h"
#include "dzin_client.h"
#include "dzin_core.h"
#include "serverc.h"
#include "press.h"
#include "sip_mod.h"
#include "dzin_netstat.h"
#include "dzin_term.h"
void push_to_global_output(const char* line);
/*
void push_to_global_output(const char* line) {
    if (!line) return;
    pthread_mutex_lock(&ui_mutex);
    if (global_output_count < MAX_LINES) {
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "%s", line);
    } else {
        for (int i = 0; i < MAX_LINES - 1; i++) {
            snprintf(globalOutputLines[i], LINE_LEN, "%s", globalOutputLines[i + 1]);
        }
        snprintf(globalOutputLines[MAX_LINES - 1], LINE_LEN, "%s", line);
    }
    pthread_mutex_unlock(&ui_mutex);
}
*/
char* run_system_command(const char* cmd) {
    char full_cmd[2048];
    snprintf(full_cmd, sizeof(full_cmd), 
        "export PATH=\"/busybox-virtual:/data/user/0/com.n0n3m4.droidc/files/:/data/user/0/com.n0n3m4.droidc/files/gcc/bin:$PATH\"; %s </dev/null 2>&1", cmd);

    FILE *fp = popen(full_cmd, "r");
    if (fp == NULL) return NULL;

    size_t buffer_size = 4096;
    size_t total_len = 0;
    char *buffer = (char*)malloc(buffer_size);
    if (!buffer) {
        pclose(fp);
        return NULL;
    }

    char chunk[1024];
    size_t bytes_read;

    while ((bytes_read = fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        if (total_len + bytes_read >= buffer_size) {
            buffer_size *= 2;
            char *new_buffer = (char*)realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                pclose(fp);
                return NULL;
            }
            buffer = new_buffer;
        }
        memcpy(buffer + total_len, chunk, bytes_read);
        total_len += bytes_read;
    }

    if (total_len >= buffer_size) {
        char *new_buffer = (char*)realloc(buffer, total_len + 1);
        if (new_buffer) buffer = new_buffer;
    }
    buffer[total_len] = '\0';

    pclose(fp);
    return buffer;
}

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

EM_JS(void, js_on_dzin_reply, (const char* msg), {
    if (!msg) return;
    const text = UTF8ToString(msg);
    if (typeof window.onDzinReply === 'function') {
        window.onDzinReply(text);
    }
});
#endif


void push_to_global_output(const char* line) {
    if (!line) return;
    pthread_mutex_lock(&ui_mutex);
    if (global_output_count < MAX_LINES) {
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "%s", line);
    } else {
        for (int i = 0; i < MAX_LINES - 1; i++) {
            snprintf(globalOutputLines[i], LINE_LEN, "%s", globalOutputLines[i + 1]);
        }
        snprintf(globalOutputLines[MAX_LINES - 1], LINE_LEN, "%s", line);
    }
    pthread_mutex_unlock(&ui_mutex);

#ifdef __EMSCRIPTEN__
    js_on_dzin_reply(line);
#endif
}
void extract_and_save_links(const unsigned char *buffer, long size) {
    pthread_mutex_lock(&file_rw_mutex);
    FILE *sf = fopen("success.html", "a+");
    if (!sf) {
        pthread_mutex_unlock(&file_rw_mutex);
        return;
    }

    fprintf(sf, "<hr><h3>Nowe linki z pobranej strony:</h3><ul>\n");

    long i = 0;
    while (i < size - 8) {
        if ((buffer[i] == 'h' || buffer[i] == 'H') &&
            (buffer[i+1] == 't' || buffer[i+1] == 'T') &&
            (buffer[i+2] == 't' || buffer[i+2] == 'T') &&
            (buffer[i+3] == 'p' || buffer[i+3] == 'P')) {
            
            long start = i;
            long end = start;
            while (end < size) {
                char c = (char)buffer[end];
                if (c == ' ' || c == '"' || c == '\'' || c == '>' || c == '<' || c == '\n' || c == '\r' || c == '\t') {
                    break;
                }
                end++;
            }

            long url_len = end - start;
            if (url_len > 10 && url_len < 512) {
                char url_str[512];
                memcpy(url_str, &buffer[start], url_len);
                url_str[url_len] = '\0';
                fprintf(sf, "<li><a href=\"%s\" target=\"_blank\">%s</a></li>\n", url_str, url_str);
            }
            i = end;
        } else {
            i++;
        }
    }

    fprintf(sf, "</ul>\n");
    fclose(sf);
    pthread_mutex_unlock(&file_rw_mutex);
}

void load_new_ocean_source(const char* filepath) {
    FILE *plik = fopen(filepath, "rb");
    if (!plik) return;

    fseek(plik, 0, SEEK_END);
    long new_size = ftell(plik);
    rewind(plik);

    if (new_size <= 0) {
        fclose(plik);
        return;
    }

    uint8_t *new_buffer = (uint8_t*)malloc(new_size);
    if (!new_buffer) {
        fclose(plik);
        return;
    }

    fread(new_buffer, 1, new_size, plik);
    fclose(plik);

    extract_and_save_links(new_buffer, new_size);

    pthread_mutex_lock(&ocean_mutex);
    if (onnx_model_buffer) free(onnx_model_buffer);
    onnx_model_buffer = new_buffer;
    onnx_model_size = new_size;
    ocean_index = 0; 
    pthread_mutex_unlock(&ocean_mutex);
}
typedef struct {
    char *cmd;
} TermArgs;

static void* dzin_term_thread_func(void* arg) {
    TermArgs *targs = (TermArgs*)arg;
    char *raw = (targs && targs->cmd) ? targs->cmd : NULL;

    /* Pomijamy wiodące "term" */
    const char *args_part = "";
    if (raw) {
        if (strncmp(raw, "term ", 5) == 0) {
            args_part = raw + 5;
            while (*args_part == ' ' || *args_part == '\t') {
                args_part++;
            }
        }
    }

    pthread_mutex_lock(&ui_mutex);
    if (global_output_count < MAX_LINES) {
        if (*args_part != '\0') {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[TERM] Start w tle: %s", args_part);
        } else {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[TERM] Start terminala w tle...");
        }
    }
    pthread_mutex_unlock(&ui_mutex);

    int rc = 0;

    if (*args_part != '\0') {
        char line_copy[DZIN_TERM_LINE_CAP];
        snprintf(line_copy, sizeof(line_copy), "%s", args_part);

        char *argv_term[DZIN_TERM_ARGV_CAP];
        int argc_term = dzin_split_argv(line_copy, argv_term, DZIN_TERM_ARGV_CAP);

        if (argc_term > 0) {
            dzin_ensure_android_path();
            int builtin_rc = dzin_exec_builtin(argc_term, argv_term);
            if (builtin_rc == 0) {
                dzin_exec_external(argv_term);
            }
        }
    } else {
        rc = dzin_term_run();
    }

    pthread_mutex_lock(&ui_mutex);
    if (global_output_count < MAX_LINES) {
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[TERM] Zakonczono (kod: %d).", rc);
    }
    pthread_mutex_unlock(&ui_mutex);

    if (targs) {
        free(targs->cmd);
        free(targs);
    }
    return NULL;
}
void* server_thread_func(void* arg) {
    int port = *(int*)arg;
    free(arg);

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);
    char *argv_server[] = { "serverc", port_str, NULL }; 
    
    serverc(2, argv_server);
    return NULL;
}
static int press_running = 0;

typedef struct {
    char *cmd;
} PressArgs;

static void append_ui_line(const char *msg) {
    pthread_mutex_lock(&ui_mutex);
    if (global_output_count < MAX_LINES) {
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "%s", msg);
    }
    pthread_mutex_unlock(&ui_mutex);
}

static void *press_thread_func(void *arg) {
    PressArgs *pargs = (PressArgs *)arg;
    char *cmd = pargs ? pargs->cmd : NULL;
    free(pargs);

    append_ui_line("[PRESS] Inicjalizacja skanera...");

    if (cmd) {
        char *argv_press[4] = { NULL, NULL, NULL, NULL };
        int argc_press = 0;
        char *saveptr = NULL;
        char *token = strtok_r(cmd, " ", &saveptr);

        while (token != NULL && argc_press < 3) {
            argv_press[argc_press++] = token;
            token = strtok_r(NULL, " ", &saveptr);
        }

        press(argc_press, argv_press);
        free(cmd);
    }

    pthread_mutex_lock(&ui_mutex);
    press_running = 0;
    if (global_output_count < MAX_LINES) {
        snprintf(globalOutputLines[global_output_count++], LINE_LEN,
                 "[PRESS] Zakonczono! Sprawdz report.html");
    }
    pthread_mutex_unlock(&ui_mutex);

    return NULL;
}
void parse_and_learn_command(const char* raw_input, const char* source_name, int force_display) {
    if (!raw_input || strlen(raw_input) == 0) return;
// Komenda TERM (z opcjonalnymi argumentami, w tle bez blokowania głównego wątku)
    if (strcmp(raw_input, "term") == 0 || strncmp(raw_input, "term ", 5) == 0) {
        TermArgs *targs = (TermArgs*)malloc(sizeof(TermArgs));
        if (!targs) return;

        targs->cmd = strdup(raw_input);
        if (!targs->cmd) {
            free(targs);
            return;
        }

        pthread_t tth;
        if (pthread_create(&tth, NULL, dzin_term_thread_func, targs) == 0) {
            pthread_detach(tth);
        } else {
            free(targs->cmd);
            free(targs);
            pthread_mutex_lock(&ui_mutex);
            if (global_output_count < MAX_LINES) {
                snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[TERM] Blad tworzenia watku.");
            }
            pthread_mutex_unlock(&ui_mutex);
        }
        return;
    }
    // 1. Komenda SERVER (w tle)
    if (strcmp(raw_input, "server") == 0 || strncmp(raw_input, "server ", 7) == 0) {
        int custom_port = 8080;
        if (strncmp(raw_input, "server ", 7) == 0) {
            custom_port = atoi(raw_input + 7);
            if (custom_port <= 0 || custom_port > 65535) custom_port = 8080;
        }

        int *port_ptr = (int*)malloc(sizeof(int));
        if (!port_ptr) return;
        *port_ptr = custom_port;

        pthread_t sth;
        if (pthread_create(&sth, NULL, server_thread_func, port_ptr) == 0) {
            pthread_detach(sth);
            pthread_mutex_lock(&ui_mutex);
            if (global_output_count < MAX_LINES) {
                snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[SERVER] Odpalono w tle na porcie %d.", custom_port);
            }
            pthread_mutex_unlock(&ui_mutex);
        } else {
            free(port_ptr);
        }
        return;
    }

if (strncmp(raw_input, "press ", 6) == 0 || strcmp(raw_input, "press") == 0) {
	/*c++ PressArgs *pargs = (PressArgs *)malloc(sizeof(PressArgs));*/
    PressArgs *pargs = malloc(sizeof(PressArgs));
    if (!pargs) return;

    pargs->cmd = strdup(raw_input);
    if (!pargs->cmd) {
        free(pargs);
        return;
    }

    pthread_t pth;
    if (pthread_create(&pth, NULL, press_thread_func, pargs) == 0) {
        pthread_detach(pth);
    } else {
        free(pargs->cmd);
        free(pargs);
    }
    return;
}

    // 3. Komenda SIP
// 3. Komenda SIP
    if (strncmp(raw_input, "sip ", 4) == 0 || strcmp(raw_input, "sip") == 0) {
        pthread_mutex_lock(&ui_mutex);
        if (global_output_count < MAX_LINES) {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[SIP] Inicjalizacja modulu SIP...");
        } else if (MAX_LINES > 0) {
            for (int i = 0; i < MAX_LINES - 1; i++) {
                snprintf(globalOutputLines[i], LINE_LEN, "%s", globalOutputLines[i + 1]);
            }
            snprintf(globalOutputLines[MAX_LINES - 1], LINE_LEN, "[SIP] Inicjalizacja modulu SIP...");
        }
        pthread_mutex_unlock(&ui_mutex);

        char input_copy[512];
        strncpy(input_copy, raw_input, sizeof(input_copy));
        input_copy[sizeof(input_copy) - 1] = '\0';

        // Maksymalnie 8 argumentów + końcowy NULL dla zgodności ze standardowym argv
        char *argv_sip[9] = { NULL };
        int argc_sip = 0;
        char *token = strtok(input_copy, " ");
        while (token != NULL && argc_sip < 8) {
            argv_sip[argc_sip++] = token;
            token = strtok(NULL, " ");
        }
        argv_sip[argc_sip] = NULL;

        pthread_mutex_lock(&ui_mutex);
        if (global_output_count < MAX_LINES) {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[SIP] Przekazano argumentow (argc): %d", argc_sip);
        } else if (MAX_LINES > 0) {
            for (int i = 0; i < MAX_LINES - 1; i++) {
                snprintf(globalOutputLines[i], LINE_LEN, "%s", globalOutputLines[i + 1]);
            }
            snprintf(globalOutputLines[MAX_LINES - 1], LINE_LEN, "[SIP] Przekazano argumentow (argc): %d", argc_sip);
        }
        pthread_mutex_unlock(&ui_mutex);

        sip_main(argc_sip, argv_sip);

        pthread_mutex_lock(&ui_mutex);
        if (global_output_count < MAX_LINES) {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[SIP] Zakonczono! Sprawdz index.html");
        } else if (MAX_LINES > 0) {
            for (int i = 0; i < MAX_LINES - 1; i++) {
                snprintf(globalOutputLines[i], LINE_LEN, "%s", globalOutputLines[i + 1]);
            }
            snprintf(globalOutputLines[MAX_LINES - 1], LINE_LEN, "[SIP] Zakonczono! Sprawdz index.html");
        }
        pthread_mutex_unlock(&ui_mutex);
        return;
    }
    // 4. Komenda NETSTAT (linki IP/porty do netstat.html)
    if (strcmp(raw_input, "netstat") == 0) {
        char info[LINE_LEN];
        dzin_netstat_generate_html("netstat.html", info, sizeof(info));
        pthread_mutex_lock(&ui_mutex);
        if (global_output_count < MAX_LINES) {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "%s", info);
        }
        pthread_mutex_unlock(&ui_mutex);
        return;
    }

    // 5. Komenda FETCH
    if (strncmp(raw_input, "fetch ", 6) == 0) {
        const char* url = raw_input + 6;
        char curl_cmd[512];
        snprintf(curl_cmd, sizeof(curl_cmd), "curl -s -L \"%s\" -o fetched_web.dat", url);

        pthread_mutex_lock(&ui_mutex);
        global_output_count = 0;
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[NET] Pobieranie: %s", url);
        pthread_mutex_unlock(&ui_mutex);

        int ret = system(curl_cmd);
        if (ret == 0) {
            load_new_ocean_source("fetched_web.dat");
            pthread_mutex_lock(&ui_mutex);
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[NET] Sukces! Zapisano linki do success.html");
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[NET] AI przelaczylo ocean na kod strony.");
            pthread_mutex_unlock(&ui_mutex);
        } else {
            pthread_mutex_lock(&ui_mutex);
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[NET] Blad pobierania strony.");
            pthread_mutex_unlock(&ui_mutex);
        }
        return;
    }

    // 6. Pomoc (-h)
    if (strcmp(raw_input, "-h") == 0) {
        pthread_mutex_lock(&ui_mutex);
        global_output_count = 0;
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "Komendy: fetch <URL>, server [port], press <url>, sip, netstat");
        pthread_mutex_unlock(&ui_mutex);
        return;
    }

    // Domyślna obsługa systemowa
    char *res = run_system_command(raw_input);
    int command_failed = 0;
    int total_len = 0;

    if (force_display) {
        pthread_mutex_lock(&ui_mutex);
        global_output_count = 0;
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[%s] $ %s", source_name, raw_input);

        if (res) {
            char *line = strtok(res, "\n");
            while (line != NULL && global_output_count < MAX_LINES) {
                snprintf(globalOutputLines[global_output_count++], LINE_LEN, "%s", line);
                total_len += (int)strlen(line);
                line = strtok(NULL, "\n");
            }
        } else {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "Blad wykonania / Brak wyniku.");
            command_failed = 1;
        }
        global_last_output_len = total_len > 0 ? total_len : 1;
        pthread_mutex_unlock(&ui_mutex);
    } else {
        if (res) {
            char *line = strtok(res, "\n");
            while (line != NULL) {
                total_len += (int)strlen(line);
                line = strtok(NULL, "\n");
            }
            global_last_output_len = total_len > 0 ? total_len : 1;
        } else {
            command_failed = 1;
        }
    }

    if (res) free(res);
    dzin_atomic_molecular_mutation(&trained_model, command_failed ? 0.9f : 0.4f);
}

void write_success_state(int gen, const char* source, const char* successful_cmd) {
    pthread_mutex_lock(&file_rw_mutex);
    FILE *sf = fopen("success.txt", "a+");
    if (sf) {
        fprintf(sf, "[SUCCESS] GEN: %d | SOURCE: %s | CMD: %s\n", gen, source, successful_cmd);
        fclose(sf);
    }
    pthread_mutex_unlock(&file_rw_mutex);
}

void check_and_reward_genie_thought(const char* thought) {
    char target_cmd[32] = "";

    if (strstr(thought, "ls") != NULL) {
        strcpy(target_cmd, "ls -la");
    } else if (strstr(thought, "pwd") != NULL) {
        strcpy(target_cmd, "pwd");
    } else if (strstr(thought, "date") != NULL) {
        strcpy(target_cmd, "date");
    }

    if (strlen(target_cmd) > 0) {
        pthread_mutex_lock(&ui_mutex);
        if (global_output_count < MAX_LINES) {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[MAGIA] Dzin wydukal komende: %s", target_cmd);
        }
        pthread_mutex_unlock(&ui_mutex);

        char *res = run_system_command(target_cmd);
        if (res) {
            pthread_mutex_lock(&ui_mutex);
            char *line = strtok(res, "\n");
            while (line != NULL && global_output_count < MAX_LINES) {
                snprintf(globalOutputLines[global_output_count++], LINE_LEN, " > %s", line);
                line = strtok(NULL, "\n");
            }
            pthread_mutex_unlock(&ui_mutex);
            free(res);
        }

        dzin_atomic_molecular_mutation(&trained_model, 0.1f);
        write_success_state(trained_model.generation, "GENIE-MAGIC", target_cmd);
    }
}

void* background_terminal_thread(void* arg) {
    (void)arg;
    char input_buf[256];
    while (fgets(input_buf, sizeof(input_buf), stdin) != NULL) {
        input_buf[strcspn(input_buf, "\r\n")] = 0;
        if (strlen(input_buf) > 0) {
            parse_and_learn_command(input_buf, "TERM-2", 0);
        }
    }
    return NULL;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void zewnetrzny_input_ai(const char* user_text) {
    if (!user_text || strlen(user_text) == 0) return;
    parse_and_learn_command(user_text, "USER-WEB", 1);
}

#endif // DZIN_IO_H