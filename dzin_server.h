#ifndef DZIN_SERVER_H
#define DZIN_SERVER_H

#include <SDL.h>
#include <SDL_net.h>
#include <stdio.h>
#include <string.h>

static TCPsocket dzin_srv_sock = NULL;
static TCPsocket dzin_cli_sock = NULL;
static SDLNet_SocketSet dzin_sock_set = NULL;

// 1. Wywolujesz RAZ przy starcie programu (po SDL_Init)
static inline int dzin_server_init(Uint16 port) {
    IPaddress addr;
    if (SDLNet_Init() < 0) {
        printf("[NET ERR] SDLNet_Init: %s\n", SDLNet_GetError());
        return 0;
    }
    dzin_sock_set = SDLNet_AllocSocketSet(2);
    if (SDLNet_ResolveHost(&addr, NULL, port) < 0) {
        printf("[NET ERR] ResolveHost: %s\n", SDLNet_GetError());
        return 0;
    }
    dzin_srv_sock = SDLNet_TCP_Open(&addr);
    if (!dzin_srv_sock) {
        printf("[NET ERR] TCP_Open na porcie %d: %s\n", port, SDLNet_GetError());
        return 0;
    }
    printf("[NET] Serwer Dzina nasluchuje na porcie %d...\n", port);
    return 1;
}

// 2. Wywolujesz W PETLI GLOWNEJ (w kazdej klatce).
// Zwraca 1, gdy przyszla nowa wiadomosc (wpisuje ja do out_msg), w przeciwnym razie 0.
static inline int dzin_server_update(char *out_msg, int max_len, const char *reply_to_send) {
    if (!dzin_srv_sock) return 0;

    // Sprawdzamy, czy polaczyl sie nowy klient (nieblokujaco)
    if (!dzin_cli_sock) {
        dzin_cli_sock = SDLNet_TCP_Accept(dzin_srv_sock);
        if (dzin_cli_sock) {
            SDLNet_TCP_AddSocket(dzin_sock_set, dzin_cli_sock);
            printf("[NET] Polaczono klienta!\n");
        }
    }

    // Jesli klient jest podpiety, sprawdzamy czy wyslal dane (0 ms czekania!)
    if (dzin_cli_sock && SDLNet_CheckSockets(dzin_sock_set, 0) > 0) {
        if (SDLNet_SocketReady(dzin_cli_sock)) {
            int bytes = SDLNet_TCP_Recv(dzin_cli_sock, out_msg, max_len - 1);
            if (bytes <= 0) {
                // Klient zamknal polaczenie
                SDLNet_TCP_DelSocket(dzin_sock_set, dzin_cli_sock);
                SDLNet_TCP_Close(dzin_cli_sock);
                dzin_cli_sock = NULL;
                printf("[NET] Klient rozlaczony.\n");
            } else {
                out_msg[bytes] = '\0';
                // Odsylamy odpowiedz od Dzina (np. jego aktualne gaworzenie!)
                if (reply_to_send && strlen(reply_to_send) > 0) {
                    SDLNet_TCP_Send(dzin_cli_sock, reply_to_send, (int)strlen(reply_to_send) + 1);
                }
                return 1; // Sygnalizujemy petli glownej: "Mamy nowa wiadomosc!"
            }
        }
    }
    return 0;
}

// 3. Wywolujesz RAZ przy zamykaniu programu (przed SDL_Quit)
static inline void dzin_server_close(void) {
    if (dzin_cli_sock) SDLNet_TCP_Close(dzin_cli_sock);
    if (dzin_srv_sock) SDLNet_TCP_Close(dzin_srv_sock);
    if (dzin_sock_set) SDLNet_FreeSocketSet(dzin_sock_set);
    SDLNet_Quit();
}

#endif