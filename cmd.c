#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_net.h>  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include<dzin_server.h>
#include<dzin_client.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define OKNO_SZEROKOSC 800
#define OKNO_WYSOKOSC  600
#define MAX_FILE_SIZE 32768 // Zwiększony bufor na większe strony HTML
#define MAX_LINES 64
#define LINE_LEN 256

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void zewnetrzny_input_ai(const char* user_text) {
    if (!user_text || strlen(user_text) == 0) return;
    
    // Przekazujesz tekst do swojej funkcji parsującej/wykonawczej
    parse_and_learn_command(user_text, "USER-WEB", 1);
}
// --- ZMIENNE GLOBALNE DLA WĄTKÓW I ENTROPII ---
pthread_mutex_t ocean_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t evo_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_rw_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t stm_mutex = PTHREAD_MUTEX_INITIALIZER; 

unsigned char *onnx_model_buffer = NULL;
long onnx_model_size = 0;
long ocean_index = 0;
typedef struct {
    float x, y, z;
    uint8_t b;
} Węzeł;
// Bufor na linie tekstowe UI
char globalOutputLines[MAX_LINES][LINE_LEN];
int global_output_count = 0;
int global_last_output_len = 0; 

// --- PAMIĘĆ KRÓTKOTRWAŁA (STM) ---
#define STM_CAPACITY 5
char short_term_memory[STM_CAPACITY][LINE_LEN];
int stm_count = 0;
int stm_head = 0;

void push_to_stm(const char* item) {
    pthread_mutex_lock(&stm_mutex);
    strncpy(short_term_memory[stm_head], item, LINE_LEN - 1);
    short_term_memory[stm_head][LINE_LEN - 1] = '\0';
    stm_head = (stm_head + 1) % STM_CAPACITY;
    if (stm_count < STM_CAPACITY) stm_count++;
    pthread_mutex_unlock(&stm_mutex);
}

unsigned char get_next_ocean_byte(); // deklaracja wstępna



// --- STRUKTURY MICRO-NN ---
typedef struct {
    float w_hidden[3][4];
    float b_hidden[4];
    float w_output[4];
    float b_output;
    int generation;
} MicroNeuralNet;

MicroNeuralNet trained_model = {
    .w_hidden = { { 1.2f, -0.5f,  2.1f,  0.1f },
                  {-0.8f,  1.5f, -0.3f,  0.9f },
                  { 0.4f,  0.2f,  1.1f, -1.2f } },
    .b_hidden = { -0.1f, 0.0f, 0.2f, -0.5f },
    .w_output = { 1.5f, 2.0f, -1.1f, 0.8f },
    .b_output = -0.7f,
    .generation = 1
};

unsigned char get_next_ocean_byte() {
    unsigned char b = 0xAF;
    pthread_mutex_lock(&ocean_mutex);
    if (onnx_model_buffer && onnx_model_size > 0) {
        b = onnx_model_buffer[ocean_index % onnx_model_size];
        ocean_index++;
    }
    pthread_mutex_unlock(&ocean_mutex);
    return b;
}
// --- ALFABETY (CYFRY OSOBNO) ---
const char* alphabet_pure = "abcdefghijklmnopqrstuvwxyz";
const char* alphabet_digits = "0123456789";         // Osobna pula cyfr
const char* alphabet_special = "-|><&;/!?=";
const char space_char = ' ';


void generate_ai_thought(char* out_buf, int max_len) {
    pthread_mutex_lock(&evo_mutex);
    int word_len = 6 + (get_next_ocean_byte() % 12); 
    if (word_len >= max_len) word_len = max_len - 1;

    int alpha_len = strlen(alphabet_pure);
    int digit_len = strlen(alphabet_digits);
    int spec_len = strlen(alphabet_special);

    out_buf[0] = '>';
    out_buf[1] = ' ';
    int idx = 2;

    for (int i = 0; i < word_len && idx < max_len - 1; i++) {
        unsigned char b = get_next_ocean_byte();
        float activation = sinf(trained_model.w_output[i % 4] + (float)b) * 10.0f;
        int choice = abs((int)(activation + b)) % 100;

        if (choice < 60) {
            out_buf[idx++] = alphabet_pure[b % alpha_len];       // 60% szans na literę
        } else if (choice < 75) {
            out_buf[idx++] = alphabet_digits[b % digit_len];     // 15% szans na cyfrę!
        } else if (choice < 90) {
            out_buf[idx++] = alphabet_special[b % spec_len];     // 15% szans na znak specjalny
        } else {
            out_buf[idx++] = space_char;                         // 10% szans na spację
        }
    }
    out_buf[idx] = '\0';
    pthread_mutex_unlock(&evo_mutex);
} 
// --- AUTONOMICZNE DUKANIE PRZEZ AI ---
/*
void generate_ai_thought(char* out_buf, int max_len) {
    pthread_mutex_lock(&evo_mutex);
    int word_len = 6 + (get_next_ocean_byte() % 12); 
    if (word_len >= max_len) word_len = max_len - 1;

    int alpha_len = strlen(alphabet_pure);
    int spec_len = strlen(alphabet_special);

    out_buf[0] = '>';
    out_buf[1] = ' ';
    int idx = 2;

    for (int i = 0; i < word_len && idx < max_len - 1; i++) {
        unsigned char b = get_next_ocean_byte();
        float activation = sinf(trained_model.w_output[i % 4] + (float)b) * 10.0f;
        int choice = abs((int)(activation + b)) % 100;

        if (choice < 75) {
            out_buf[idx++] = alphabet_pure[b % alpha_len];
        } else if (choice < 90) {
            out_buf[idx++] = alphabet_special[b % spec_len];
        } else {
            out_buf[idx++] = space_char;
        }
    }
    out_buf[idx] = '\0';
    pthread_mutex_unlock(&evo_mutex);
}
*/
unsigned char cognitive_read_screen_output() {
    pthread_mutex_lock(&ui_mutex);
    unsigned char screen_entropy = 0x00;
    
    if (global_output_count > 0) {
        char* last_line = globalOutputLines[global_output_count - 1];
        push_to_stm(last_line);

        for (size_t i = 0; i < strlen(last_line); i++) {
            unsigned char c = (unsigned char)last_line[i];
            screen_entropy = (screen_entropy + c) ^ 0x5A;
            screen_entropy = ((screen_entropy & 0x0F) << 4) | ((screen_entropy & 0xF0) >> 4);
        }
    }
    pthread_mutex_unlock(&ui_mutex);
    return screen_entropy;
}

void dzin_atomic_molecular_mutation(MicroNeuralNet *net, float ambient_entropy) {
    pthread_mutex_lock(&evo_mutex);
    net->generation++;

    const float H2O_IDEAL_ANGLE = 104.5f;
    const float PLANCK_APPROX = 6.626f;

    unsigned char entropy_byte_1 = get_next_ocean_byte();
    unsigned char screen_read_val = cognitive_read_screen_output();
    
    unsigned char mixed_entropy = (entropy_byte_1 & 0xCC) | (screen_read_val & 0x33);
    float mapped_entropy_1 = ((float)mixed_entropy / 255.0f) * 10.0f - 5.0f;

    float current_angle = H2O_IDEAL_ANGLE + mapped_entropy_1 * ambient_entropy;
    float molecular_pressure = fabsf(current_angle - H2O_IDEAL_ANGLE) / H2O_IDEAL_ANGLE;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            float atomic_shift = sinf(i + j + (float)net->generation) * molecular_pressure * (PLANCK_APPROX / 10.0f);
            net->w_hidden[i][j] += atomic_shift;
        }
    }
    pthread_mutex_unlock(&evo_mutex);
}

char* run_system_command(const char* cmd) {
    char *echo = (char*)malloc(MAX_FILE_SIZE);
    if (!echo) return NULL;
    
    char full_cmd[MAX_FILE_SIZE];
    snprintf(full_cmd, sizeof(full_cmd), 
        "export PATH=\"/busybox-virtual:/data/user/0/com.n0n3m4.droidc/files/:/data/user/0/com.n0n3m4.droidc/files/gcc/bin:$PATH\"; %s 2>&1", cmd);

    FILE *fp = popen(full_cmd, "r");
    if (fp == NULL) {
        free(echo);
        return NULL;
    }
    size_t len = fread(echo, 1, MAX_FILE_SIZE - 1, fp);
    echo[len] = '\0';
    pclose(fp);
    return echo;
}

// --- FUNKCJA WYCIĄGANIA LINKÓW HTTPS I ZAPISU DO success.html ---
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
        // Szukamy prefiksu "http://" lub "https://" (wielkość liter ignorowana lub sprawdzana)
        if ((buffer[i] == 'h' || buffer[i] == 'H') &&
            (buffer[i+1] == 't' || buffer[i+1] == 'T') &&
            (buffer[i+2] == 't' || buffer[i+2] == 'T') &&
            (buffer[i+3] == 'p' || buffer[i+3] == 'P')) {
            
            long start = i;
            long end = start;
            // Szukamy końca linku (spacja, cudzysłów, nawiasy, znaki końca linii)
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

                // Zapisujemy w formacie pełnego tagu a href do success.html
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

// --- PODMIANA BUFORA ORAZ URUCHOMIENIE EKSTRAKCJI ---
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

    // Automatycznie wyciągnij linki HTTPS/HTTP i zapisz do success.html
    extract_and_save_links(new_buffer, new_size);

    // Bezpieczna podmiana wskaźnika pod mutexem dla AI (dukania)
    pthread_mutex_lock(&ocean_mutex);
    if (onnx_model_buffer) free(onnx_model_buffer);
    onnx_model_buffer = new_buffer;
    onnx_model_size = new_size;
    ocean_index = 0; 
    pthread_mutex_unlock(&ocean_mutex);
}

// --- POPRAWIONY PARSER KOMEND Z FORCE_DISPLAY ORAZ FETCH ---
void parse_and_learn_command(const char* raw_input, const char* source_name, int force_display) {
    if (!raw_input || strlen(raw_input) == 0) return;

    // Obsługa komendy fetch z internetu
    if (strncmp(raw_input, "fetch ", 6) == 0) {
        const char* url = raw_input + 6;
        char curl_cmd[512];
        snprintf(curl_cmd, sizeof(curl_cmd), "curl -s -L \"%s\" -o fetched_web.dat", url);

        pthread_mutex_lock(&ui_mutex);
        global_output_count = 0; // Czyścimy ekran, żeby pokazać status sieci
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[NET] Pobieranie: %s", url);
        pthread_mutex_unlock(&ui_mutex);

        int ret = system(curl_cmd);
        if (ret == 0) {
            load_new_ocean_source("fetched_web.dat"); // Wczytuje i wyciąga linki do success.html

            pthread_mutex_lock(&ui_mutex);
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[NET] Sukces! Zapisano linki do success.html");
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[NET] AI przełaczyło ocean na kod strony.");
            pthread_mutex_unlock(&ui_mutex);
        } else {
            pthread_mutex_lock(&ui_mutex);
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[NET] Blad pobierania strony.");
            pthread_mutex_unlock(&ui_mutex);
        }
        return;
    }

    if (strcmp(raw_input, "-h") == 0 || strcmp(raw_input, "ia") == 0) {
        pthread_mutex_lock(&ui_mutex);
        global_output_count = 0;
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "Wpisz 'fetch <URL>', aby wyciagnac linki i dukać stronę.");
        pthread_mutex_unlock(&ui_mutex);
        return;
    }

    char *res = run_system_command(raw_input);
    int command_failed = 0;
    int total_len = 0;

    if (force_display) {
        pthread_mutex_lock(&ui_mutex);
        global_output_count = 0; // Czyszczenie ekranu przy komendzie z klawiatury
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

struct AppState {
    SDL_Renderer* renderer;
    TTF_Font* font;
    Węzeł* siec;
    int liczba_wezlow;
    char userInput[256];
    float rotacja;
    int frame_counter;
    int scroll_offset;
    int quit;
};

void* background_terminal_thread(void* arg) {
    char input_buf[256];
    while (fgets(input_buf, sizeof(input_buf), stdin) != NULL) {
        input_buf[strcspn(input_buf, "\r\n")] = 0;
        if (strlen(input_buf) > 0) {
            parse_and_learn_command(input_buf, "TERM-2", 0);
        }
    }
    return NULL;
}
// --- BRAKUJĄCA FUNKCJA ZAPISU SUKCESU ---
void write_success_state(int gen, const char* source, const char* successful_cmd) {
    pthread_mutex_lock(&file_rw_mutex);
    FILE *sf = fopen("success.txt", "a+");
    if (sf) {
        fprintf(sf, "[SUCCESS] GEN: %d | SOURCE: %s | CMD: %s\n", gen, source, successful_cmd);
        fclose(sf);
    }
    pthread_mutex_unlock(&file_rw_mutex);
}
// --- FUNKCJA NAGRODY I SPRAWDZANIA CZY DŽIN WYDUKAŁ KOMENDĘ ---
void check_and_reward_genie_thought(const char* thought) {
    char target_cmd[32] = "";

    // Dżin próbuje "wyczarować" komendy systemowe z własnego dukania
    if (strstr(thought, "ls") != NULL) {
        strcpy(target_cmd, "ls -la");
    } else if (strstr(thought, "pwd") != NULL) {
        strcpy(target_cmd, "pwd");
    } else if (strstr(thought, "date") != NULL) {
        strcpy(target_cmd, "date");
    }

    if (strlen(target_cmd) > 0) {
        // UDAŁO SIĘ! Dżin wydukał poprawną komendę!
        pthread_mutex_lock(&ui_mutex);
        snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[MAGIA] Dzin wydukal komende: %s", target_cmd);
        pthread_mutex_unlock(&ui_mutex);

        // Wykonaj komendę i wrzuć wyniki na ekran
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

        // NAGRODA DLA SIECI: Pozytywne wzmocnienie (stabilizacja wag, mniejsza mutacja)
        // Zamiast kary, nagradzamy strukturę, żeby częściej generowała logiczne wzorce
        dzin_atomic_molecular_mutation(&trained_model, 0.1f); // Bardzo niska entropia = nagroda/stabilizacja
        
        // Zapisz sukces do pliku
        write_success_state(trained_model.generation, "GENIE-MAGIC", target_cmd);
    }
}
// ... (main_loop i main pozostają bez zmian)

void main_loop(void* arg) {
    struct AppState* state = (struct AppState*)arg;
    SDL_Event e;
  
    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            state->quit = 1;
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
        } else if (e.type == SDL_TEXTINPUT) {
        	   SDL_StartTextInput();
            size_t len = strlen(state->userInput);
            if (len < 80) {
                strcat(state->userInput, e.text.text);
            }
        } else if (e.type == SDL_KEYDOWN) {
            size_t len = strlen(state->userInput);
            if (e.key.keysym.sym == SDLK_BACKSPACE && len > 0) {
                state->userInput[len - 1] = '\0';
            } else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                if (len > 0) {
                    parse_and_learn_command(state->userInput, "KBD-SDL", 1);
                    state->userInput[0] = '\0';
                    state->scroll_offset = 0;
                }
            } else if (e.key.keysym.sym == SDLK_DOWN) {
                state->scroll_offset += 2;
            } else if (e.key.keysym.sym == SDLK_UP) {
                if (state->scroll_offset >= 2) state->scroll_offset -= 2;
            }
        }
    }
// --- GIMBERLINK: DIALOG SERWER <-> KLIENT (LOCALHOST) ---
    char srv_in[LINE_LEN];
    char cli_in[LINE_LEN];
    char last_thought[LINE_LEN] = "a4a4";

    // Pobieramy ostatnią myśl Dżina, żeby Serwer miał co odesłać Klientowi
    pthread_mutex_lock(&ui_mutex);
    if (global_output_count > 0) {
        strncpy(last_thought, globalOutputLines[global_output_count - 1], LINE_LEN - 1);
        last_thought[LINE_LEN - 1] = '\0';
    }
    pthread_mutex_unlock(&ui_mutex);

    // 1. Serwer nasłuchuje: jeśli Klient coś wysłał, wrzucamy w pamięć STM i odsyłamy last_thought
    if (dzin_server_update(srv_in, LINE_LEN, last_thought)) {
        push_to_stm(srv_in);
        dzin_atomic_molecular_mutation(&trained_model, 0.25f);
    }

    // 2. Klient nasłuchuje: jeśli Serwer odesłał odpowiedź, wyświetlamy ją na ekranie jako [GIMBERLINK]
    if (dzin_client_update(cli_in, LINE_LEN)) {
        push_to_stm(cli_in);

        pthread_mutex_lock(&ui_mutex);
        if (global_output_count < MAX_LINES) {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "[GIMBERLINK] %s", cli_in);
        } else {
            for (int i = 0; i < MAX_LINES - 1; i++) {
                strcpy(globalOutputLines[i], globalOutputLines[i + 1]);
            }
            snprintf(globalOutputLines[MAX_LINES - 1], LINE_LEN, "[GIMBERLINK] %s", cli_in);
        }
        pthread_mutex_unlock(&ui_mutex);
    }
    // --------------------------------------------------------

    state->frame_counter++;
    // ----------------------------------------------

    state->frame_counter++;

    // Automatyczna myśli / pisanie AI co określony czas
if (state->frame_counter % 120 == 0) {
        char thought_buf[LINE_LEN];
        generate_ai_thought(thought_buf, LINE_LEN);
        dzin_client_send(thought_buf);
        // Sprawdzamy, czy Dżin nie wydukał czegoś pożytecznego!
        check_and_reward_genie_thought(thought_buf);

        pthread_mutex_lock(&ui_mutex);
        if (global_output_count < MAX_LINES) {
            snprintf(globalOutputLines[global_output_count++], LINE_LEN, "%s", thought_buf);
        } else {
            for (int i = 0; i < MAX_LINES - 1; i++) {
                strcpy(globalOutputLines[i], globalOutputLines[i + 1]);
            }
            snprintf(globalOutputLines[MAX_LINES - 1], LINE_LEN, "%s", thought_buf);
        }
        pthread_mutex_unlock(&ui_mutex);

        dzin_atomic_molecular_mutation(&trained_model, 0.3f);
    }

    int current_len = 0;
    pthread_mutex_lock(&ui_mutex);
    current_len = global_last_output_len;
    pthread_mutex_unlock(&ui_mutex);

    if (current_len > 0) {
        for (int i = 0; i < state->liczba_wezlow; i++) {
            float mod = (float)(current_len % 7) * 0.1f;
            float kat = (float)i * 0.2f + (float)state->frame_counter * 0.01f;
            state->siec[i].x = cosf(kat) * (110.0f + mod * 15.0f) + ((i % 5) - 2) * 18.0f;
            state->siec[i].z = sinf(kat) * (110.0f + mod * 15.0f) + ((i / 5) % 5) * 18.0f;
        }
    }

    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->renderer);

    state->rotacja += 0.01f;
    float c = cosf(state->rotacja);
    float s = sinf(state->rotacja);

    int sx[300], sy[300];
    for (int i = 0; i < state->liczba_wezlow; i++) {
        float x1 = state->siec[i].x * c - state->siec[i].z * s;
        float z1 = state->siec[i].x * s + state->siec[i].z * c;
        float y1 = state->siec[i].y;

        float z_kamery = z1 + 400.0f;
        float skala = 300.0f / z_kamery;

        sx[i] = (int)(OKNO_SZEROKOSC / 2 + x1 * skala);
        sy[i] = (int)((OKNO_WYSOKOSC - 130) + y1 * skala);
    }

    SDL_SetRenderDrawColor(state->renderer, 0, 100, 0, 160);
    for (int i = 0; i < state->liczba_wezlow; i++) {
        int n1 = (i + 1) % state->liczba_wezlow;
        int n2 = (i + 15) % state->liczba_wezlow;
        SDL_RenderDrawLine(state->renderer, sx[i], sy[i], sx[n1], sy[n1]);
        if (i % 2 == 0) {
            SDL_RenderDrawLine(state->renderer, sx[i], sy[i], sx[n2], sy[n2]);
        }
    }

    for (int i = 0; i < state->liczba_wezlow; i++) {
        uint8_t b = state->siec[i].b;
        if (b >= 32 && b <= 126) {
            SDL_SetRenderDrawColor(state->renderer, 0, 255, 0, 255);
        } else {
            SDL_SetRenderDrawColor(state->renderer, 50, 205, 50, 255);
        }
        SDL_Rect kropka = {sx[i] - 2, sy[i] - 2, 5, 5};
        SDL_RenderFillRect(state->renderer, &kropka);
    }

    if (state->font) {
        SDL_Color aiTextColor = { 0, 255, 0, 255 };
        SDL_Color inputTextColor = { 150, 255, 150, 255 };
        int y = 15;

        pthread_mutex_lock(&ui_mutex);
        int start_idx = state->scroll_offset;
        int visible_lines = 22; // ile linii mieści się na ekranie 600px
        if (start_idx >= global_output_count) start_idx = global_output_count > 0 ? global_output_count - 1 : 0;

start_idx = (global_output_count > visible_lines) ? (global_output_count - visible_lines) : 0;

        for (int i = start_idx; i < global_output_count; i++) {
            if (strlen(globalOutputLines[i]) == 0) continue;
            SDL_Surface* textSurface = TTF_RenderText_Solid(state->font, globalOutputLines[i], aiTextColor);
            if (textSurface) {
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(state->renderer, textSurface);
                SDL_Rect renderQuad = { 15, y, textSurface->w, textSurface->h };
                
                if (y < OKNO_WYSOKOSC - 100) {
                    SDL_RenderCopy(state->renderer, textTexture, NULL, &renderQuad);
                }
                y += textSurface->h + 4;
                SDL_DestroyTexture(textTexture);
                SDL_FreeSurface(textSurface);
            }
        }
        pthread_mutex_unlock(&ui_mutex);

        y = OKNO_WYSOKOSC - 70;
        SDL_SetRenderDrawColor(state->renderer, 0, 140, 0, 255);
        SDL_RenderDrawLine(state->renderer, 15, y, OKNO_SZEROKOSC - 15, y);
        y += 10;

        int cursorVisible = ((state->frame_counter / 30) % 2) == 0;
        char promptLine[300];
        snprintf(promptLine, sizeof(promptLine), "> %s%s", state->userInput, cursorVisible ? "_" : " ");
        SDL_Surface* inputSurface = TTF_RenderText_Solid(state->font, promptLine, inputTextColor);
        if (inputSurface) {
            SDL_Texture* inputTexture = SDL_CreateTextureFromSurface(state->renderer, inputSurface);
            SDL_Rect inputQuad = { 15, y, inputSurface->w, inputSurface->h };
            SDL_RenderCopy(state->renderer, inputTexture, NULL, &inputQuad);
            SDL_DestroyTexture(inputTexture);
            SDL_FreeSurface(inputSurface);
        }
    }

    SDL_RenderPresent(state->renderer);
}


int main(int argc, char* args[]) {
    const char* filename = (argc < 2) ? "assets/network_anomaly.onnx" : args[1];

    FILE *plik = fopen(filename, "rb+");
    if (!plik) {
        onnx_model_size = 256;
        onnx_model_buffer = (uint8_t*)malloc(onnx_model_size);
        for(int i = 0; i < onnx_model_size; i++) onnx_model_buffer[i] = (uint8_t)i;
    } else {
        fseek(plik, 0, SEEK_END);
        onnx_model_size = ftell(plik);
        rewind(plik);

        onnx_model_buffer = (uint8_t*)malloc(onnx_model_size);
        fread(onnx_model_buffer, 1, onnx_model_size, plik);
        fclose(plik);
    }
     
    pthread_t term_thread;
    if (pthread_create(&term_thread, NULL, background_terminal_thread, NULL) != 0) {
        // Cicha obsługa błędu
    } else {
        pthread_detach(term_thread);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    if (TTF_Init() == -1) return -1;

    SDL_Window* window = SDL_CreateWindow("Matrix AI Pure C Observer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, OKNO_SZEROKOSC, OKNO_WYSOKOSC, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    TTF_Font* font = TTF_OpenFont("rubik-black.font", 18);
    if (!font) font = TTF_OpenFont("font.ttf", 18);
    if (!font) font = TTF_OpenFont("assets/lazy.ttf", 18);
//83.29.170.125
 //   SDL_StartTextInput();
 dzin_server_init(4444);
dzin_client_init("127.0.0.2",4444);
    pthread_mutex_lock(&ui_mutex);
    snprintf(globalOutputLines[global_output_count++], LINE_LEN, "System C uruchomiony. AI pisze autonomicznie z alfabetu:");
    pthread_mutex_unlock(&ui_mutex);

    int liczba_wezlow = 300; 
    Węzeł *siec = (Węzeł*)malloc(liczba_wezlow * sizeof(Węzeł));

    for (int i = 0; i < liczba_wezlow; i++) {
        long idx = i * (onnx_model_size / liczba_wezlow); 
        float kat_siatki = (float)i * 0.2f;
        siec[i].x = cosf(kat_siatki) * 120.0f + ((i % 5) - 2) * 20.0f;
        siec[i].y = (float)((i / 15) - 5) * 20.0f;
        siec[i].z = sinf(kat_siatki) * 120.0f + ((i / 5) % 5) * 20.0f;
        siec[i].b = onnx_model_buffer[idx % onnx_model_size];
    }
SDL_StartTextInput();
    struct AppState state = {
        .renderer = renderer,
        .font = font,
        .siec = siec,
        .liczba_wezlow = liczba_wezlow,
        .userInput = {0},
        .rotacja = 0.0f,
        .frame_counter = 0,
        .scroll_offset = 0,
        .quit = 0
    };

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(main_loop, &state, 0, 1);
#else
    while (!state.quit) {
        main_loop(&state);
        SDL_Delay(16);
    }

    SDL_StopTextInput();
    free(onnx_model_buffer);
    free(siec);
    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    dzin_client_close();
    dzin_server_close();
//    SDLNet_SocketSet dzin_sock_set = NULL;
    TTF_Quit();
    SDL_Quit();
#endif

    return 0;
}