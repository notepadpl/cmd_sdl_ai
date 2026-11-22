#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <pthread.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_net.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "dzin_core.h"
#include "dzin_io.h"
#include "dzin_server.h"
#include "dzin_client.h"
#include "dzin_babble.h"

/* Ostatnia myśl Dżina */
static char g_last_genie_thought[LINE_LEN] = "a4a4";

static void set_genie_thought(const char* thought) {
    if (!thought || thought[0] == '\0') return;
    snprintf(g_last_genie_thought, sizeof(g_last_genie_thought), "%s", thought);
}

static void append_prefixed_to_user_console(const char* prefix, const char* msg) {
    if (!msg || msg[0] == '\0') return;
    char buf[LINE_LEN];
    snprintf(buf, sizeof(buf), "%s%s", prefix ? prefix : "", msg);
    push_to_global_output(buf);
}

void main_loop(void* arg) {
    struct AppState* state = (struct AppState*)arg;
    SDL_Event e;

    while (SDL_PollEvent(&e) != 0) {
        if (e.type == SDL_QUIT) {
            state->quit = 1;
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            SDL_StartTextInput();
        } else if (e.type == SDL_TEXTINPUT) {
            size_t len = strlen(state->userInput);
            if (len + strlen(e.text.text) < 80) {
                strcat(state->userInput, e.text.text);
            }
        } else if (e.type == SDL_KEYDOWN) {
            size_t len = strlen(state->userInput);
            if (e.key.keysym.sym == SDLK_BACKSPACE && len > 0) {
                state->userInput[len - 1] = '\0';
            } else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
                if (len > 0) {
                    /* Przekazujemy wejście użytkownika i odpowiadamy także przez kanał serwera/klienta */
                    dzin_client_send(state->userInput);
                    parse_and_learn_command(state->userInput, "USER", 1);
                    state->userInput[0] = '\0';
                    state->scroll_offset = 0;
                }
            } else if (e.key.keysym.sym == SDLK_DOWN) {
                if (state->scroll_offset >= 2) state->scroll_offset -= 2;
                else state->scroll_offset = 0;
            } else if (e.key.keysym.sym == SDLK_UP) {
                state->scroll_offset += 2;
            }
        }
    }

    /* --- SERWER <-> KLIENT: odpowiedzi serwera trafiają na konsolę użytkownika --- */
    char srv_in[LINE_LEN];
    char cli_in[LINE_LEN];

    if (dzin_server_update(srv_in, LINE_LEN, g_last_genie_thought)) {
        push_to_stm(srv_in);
        set_genie_thought(srv_in);
        append_prefixed_to_user_console("[SERWER] ", srv_in);
        state->scroll_offset = 0;
        dzin_atomic_molecular_mutation(&trained_model, 0.25f);
    }

    if (dzin_client_update(cli_in, LINE_LEN)) {
        push_to_stm(cli_in);
        set_genie_thought(cli_in);
        append_prefixed_to_user_console("[KLIENT] ", cli_in);
        state->scroll_offset = 0;
    }

    state->frame_counter++;

    /* Co 120 klatek Dżin wysyła myśl do serwera i pisze na konsolę użytkownika */
/* Co 120 klatek Dżin gaworzy z alfabetu, pisze na konsolę i wysyła do serwera SDL_net */
/* Co 120 klatek cała logika gaworzenia wykonuje się w dzin_babble.h */
    if (state->frame_counter % 120 == 0) {
        dzin_babble_tick(g_last_genie_thought, sizeof(g_last_genie_thought), &state->scroll_offset);
    }
    /* Wizualizacja 3D reaguje na długość ostatniego odczytu */
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

    /* --- RYSOWANIE globalOutputLines --- */
    if (state->font) {
        SDL_Color userTextColor = { 0, 255, 0, 255 };
        SDL_Color inputTextColor = { 150, 255, 150, 255 };
        int y = 15;

        pthread_mutex_lock(&ui_mutex);
        int visible_lines = 22;
        int end_idx = global_output_count - state->scroll_offset;
        if (end_idx > global_output_count) end_idx = global_output_count;
        if (end_idx < 0) end_idx = 0;

        int start_idx = end_idx - visible_lines;
        if (start_idx < 0) start_idx = 0;

        for (int i = start_idx; i < end_idx; i++) {
            if (strlen(globalOutputLines[i]) == 0) continue;
            SDL_Surface* textSurface = TTF_RenderText_Solid(state->font, globalOutputLines[i], userTextColor);
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
        onnx_model_buffer = (uint8_t*)malloc((size_t)onnx_model_size);
        if (onnx_model_buffer) {
            for (int i = 0; i < onnx_model_size; i++) {
                onnx_model_buffer[i] = (uint8_t)i;
            }
        }
    } else {
        fseek(plik, 0, SEEK_END);
        onnx_model_size = ftell(plik);
        rewind(plik);

        onnx_model_buffer = (uint8_t*)malloc((size_t)onnx_model_size);
        if (onnx_model_buffer) {
            size_t n = fread(onnx_model_buffer, 1, (size_t)onnx_model_size, plik);
            onnx_model_size = (long)n;
        }
        fclose(plik);
    }

    pthread_t term_thread;
    if (pthread_create(&term_thread, NULL, background_terminal_thread, NULL) == 0) {
        pthread_detach(term_thread);
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    if (TTF_Init() == -1) return -1;
    if (SDLNet_Init() < 0) return -1;

    SDL_Window* window = SDL_CreateWindow(
        "Matrix AI Pure C Observer",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        OKNO_SZEROKOSC,
        OKNO_WYSOKOSC,
        SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("rubik-black.font", 18);
    if (!font) font = TTF_OpenFont("font.ttf", 18);
    if (!font) font = TTF_OpenFont("assets/lazy.ttf", 18);

    dzin_server_init(4444);
    dzin_client_init("127.0.0.1", 4444);

    pthread_mutex_lock(&ui_mutex);
    global_output_count = 0;
    snprintf(globalOutputLines[global_output_count++], LINE_LEN, "System uruchomiony. Serwer odpowiada na konsoli uzytkownika.");
    pthread_mutex_unlock(&ui_mutex);

    int liczba_wezlow = 300;
    Wezel *siec = (Wezel*)malloc((size_t)liczba_wezlow * sizeof(Wezel));

    if (siec && onnx_model_buffer && onnx_model_size > 0) {
        for (int i = 0; i < liczba_wezlow; i++) {
            long idx = (i * onnx_model_size) / liczba_wezlow;
            float kat_siatki = (float)i * 0.2f;
            siec[i].x = cosf(kat_siatki) * 120.0f + ((i % 5) - 2) * 20.0f;
            siec[i].y = (float)((i / 15) - 5) * 20.0f;
            siec[i].z = sinf(kat_siatki) * 120.0f + ((i / 5) % 5) * 20.0f;
            siec[i].b = onnx_model_buffer[idx % onnx_model_size];
        }
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

    if (onnx_model_buffer) free(onnx_model_buffer);
    if (siec) free(siec);
    if (font) TTF_CloseFont(font);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    dzin_client_close();
    dzin_server_close();
    SDLNet_Quit();
    TTF_Quit();
    SDL_Quit();
#endif

    return 0;
}