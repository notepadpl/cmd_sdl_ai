#ifndef DZIN_CORE_H
#define DZIN_CORE_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include "dzin_server.h"
#include "dzin_client.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define OKNO_SZEROKOSC 800
#define OKNO_WYSOKOSC  600
#define MAX_FILE_SIZE 32768
#define MAX_LINES 64
#define LINE_LEN 256
#define STM_CAPACITY 5

typedef struct {
    float x, y, z;
    uint8_t b;
} Wezel;

typedef struct {
    float w_hidden[3][4];
    float b_hidden[4];
    float w_output[4];
    float b_output;
    int generation;
} MicroNeuralNet;

struct AppState {
    SDL_Renderer* renderer;
    TTF_Font* font;
    Wezel* siec;
    int liczba_wezlow;
    char userInput[256];
    float rotacja;
    int frame_counter;
    int scroll_offset;
    int quit;
};

/* Zmienne współdzielone */
static pthread_mutex_t ocean_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t evo_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t file_rw_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ui_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t stm_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t genie_console_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline void genie_console_log(const char* tag, const char* msg) {
    if (!msg) return;
    pthread_mutex_lock(&genie_console_mutex);
    printf("[%s] %s\n", tag ? tag : "DZIN", msg);
    fflush(stdout);
    pthread_mutex_unlock(&genie_console_mutex);
}

static inline void dzin_write_alphabet_console(int length) {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
    const size_t alpha_len = sizeof(alphabet) - 1;

    if (length <= 0) length = 16;
    if (length > 256) length = 256;

    char buf[257];
    for (int i = 0; i < length; i++) {
        size_t idx = (size_t)rand() % alpha_len;
        buf[i] = alphabet[idx];
    }
    buf[length] = '\0';

    genie_console_log("DZIN-ALFABET", buf);
}
static unsigned char *onnx_model_buffer = NULL;
static long onnx_model_size = 0;
static long ocean_index = 0;

static char globalOutputLines[MAX_LINES][LINE_LEN];
static int global_output_count = 0;
static int global_last_output_len = 0;

static char short_term_memory[STM_CAPACITY][LINE_LEN];
static int stm_count = 0;
static int stm_head = 0;

static MicroNeuralNet trained_model = {
    .w_hidden = { { 1.2f, -0.5f,  2.1f,  0.1f },
                  {-0.8f,  1.5f, -0.3f,  0.9f },
                  { 0.4f,  0.2f,  1.1f, -1.2f } },
    .b_hidden = { -0.1f, 0.0f, 0.2f, -0.5f },
    .w_output = { 1.5f, 2.0f, -1.1f, 0.8f },
    .b_output = -0.7f,
    .generation = 1
};

static inline void push_to_stm(const char* item) {
    if (!item) return;
    pthread_mutex_lock(&stm_mutex);
    strncpy(short_term_memory[stm_head], item, LINE_LEN - 1);
    short_term_memory[stm_head][LINE_LEN - 1] = '\0';
    stm_head = (stm_head + 1) % STM_CAPACITY;
    if (stm_count < STM_CAPACITY) stm_count++;
    pthread_mutex_unlock(&stm_mutex);
}

static inline unsigned char get_next_ocean_byte(void) {
    unsigned char b = 0xAF;
    pthread_mutex_lock(&ocean_mutex);
    if (onnx_model_buffer && onnx_model_size > 0) {
        b = onnx_model_buffer[ocean_index % onnx_model_size];
        ocean_index++;
    }
    pthread_mutex_unlock(&ocean_mutex);
    return b;
}

static inline unsigned char cognitive_read_screen_output(void) {
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

static inline void dzin_atomic_molecular_mutation(MicroNeuralNet *net, float ambient_entropy) {
    if (!net) return;

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
            float atomic_shift = sinf((float)(i + j + net->generation)) *
                                 molecular_pressure * (PLANCK_APPROX / 10.0f);
            net->w_hidden[i][j] += atomic_shift;
        }
    }

    for (int j = 0; j < 4; j++) {
        float out_shift = cosf((float)(j + net->generation)) * molecular_pressure * 0.25f;
        net->w_output[j] += out_shift;
    }
    net->b_output += sinf((float)net->generation) * molecular_pressure * 0.1f;

    pthread_mutex_unlock(&evo_mutex);
}

/* --- ALFABET I DECYZJE PRZEZ SIEĆ NEURONOWĄ NA KOŃCU PLIKU --- */

static inline const char* dzin_full_alphabet(size_t *out_len) {
    /* Jedna spójna pula alfabetu bez sztywnych procentów: litery, cyfry, znaki specjalne, spacja */
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789-|><&;/!?= ";
    if (out_len) {
        *out_len = sizeof(alphabet) - 1;
    }
    return alphabet;
}

/* Przepuszcza wejścia przez MicroNeuralNet i jej wagi, zwracając indeks w pełnym alfabecie */
static inline size_t dzin_nn_pick_alphabet_index(const MicroNeuralNet *net,
                                                 unsigned char ocean_byte,
                                                 int step_idx) {
    size_t alpha_len = 0;
    const char *alpha = dzin_full_alphabet(&alpha_len);
    (void)alpha;
    if (alpha_len == 0) return 0;

    float x[3] = {
        ((float)ocean_byte / 255.0f) * 2.0f - 1.0f,
        sinf((float)step_idx * 0.65f),
        tanhf((float)(net ? net->generation : 1) * 0.05f)
    };

    float hidden[4] = {0.0f};
    for (int j = 0; j < 4; j++) {
        float sum = net ? net->b_hidden[j] : 0.0f;
        for (int i = 0; i < 3; i++) {
            float w = net ? net->w_hidden[i][j] : 0.5f;
            sum += x[i] * w;
        }
        hidden[j] = tanhf(sum);
    }

    float out_val = net ? net->b_output : 0.0f;
    for (int j = 0; j < 4; j++) {
        float w_out = net ? net->w_output[j] : 1.0f;
        out_val += hidden[j] * w_out;
    }

    /* Mapowanie wyjścia sieci (z użyciem aktywacji i wag) na pełny zakres alfabetu */
    float norm = 0.5f * (tanhf(out_val) + 1.0f);
    float raw_pos = norm * (float)(alpha_len - 1) + (float)ocean_byte * 0.25f;
    long idx = (long)llroundf(fabsf(raw_pos)) % (long)alpha_len;
    if (idx < 0) idx = 0;
    return (size_t)idx;
}

static inline void generate_ai_thought(char* out_buf, int max_len) {
    if (!out_buf || max_len <= 2) return;

    size_t alpha_len = 0;
    const char *alpha = dzin_full_alphabet(&alpha_len);

    pthread_mutex_lock(&evo_mutex);
    unsigned char len_byte = get_next_ocean_byte();
    int word_len = 6 + (int)(len_byte % 18);
    if (word_len >= max_len - 2) word_len = max_len - 3;
    if (word_len < 1) word_len = 1;

    out_buf[0] = '>';
    out_buf[1] = ' ';
    int pos = 2;

    for (int i = 0; i < word_len && pos < max_len - 1; i++) {
        unsigned char b = get_next_ocean_byte();
        size_t idx = dzin_nn_pick_alphabet_index(&trained_model, b, i);
        out_buf[pos++] = alpha[idx];
    }
    out_buf[pos] = '\0';
    pthread_mutex_unlock(&evo_mutex);
}

#endif // DZIN_CORE_H