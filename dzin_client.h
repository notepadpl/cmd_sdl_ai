#ifndef DZIN_CLIENT_H
#define DZIN_CLIENT_H

#include <SDL.h>
#include <SDL_net.h>
#include <stdio.h>
#include <string.h>

static TCPsocket dzin_cli_conn = NULL;
static SDLNet_SocketSet dzin_cli_set = NULL;

// 1. Wywołujesz RAZ przy starcie programu (po SDL_Init)
// host: adres IP (np. "192.168.1.2" lub "127.0.0.1") albo domena tunelu (np. "moj-dzin.trycloudflare.com")
// port: np. 4444 (natywnie) lub 80 / 443 (przez tunel WebSocket w przeglądarce)
static inline int dzin_client_init(const char *host, Uint16 port) {
    IPaddress addr;
    if (SDLNet_Init() < 0) {
        printf("[NET-CLI ERR] SDLNet_Init: %s\n", SDLNet_GetError());
        return 0;
    }

    dzin_cli_set = SDLNet_AllocSocketSet(1);
    if (!dzin_cli_set) {
        printf("[NET-CLI ERR] AllocSocketSet: %s\n", SDLNet_GetError());
        return 0;
    }

    if (SDLNet_ResolveHost(&addr, host, port) < 0) {
        printf("[NET-CLI ERR] ResolveHost (%s:%d): %s\n", host, port, SDLNet_GetError());
        return 0;
    }

    // W Emscriptenie (-s USE_SDL_NET=2) to wywołanie otwiera w tle WebSocket (ws:// lub wss://)
    dzin_cli_conn = SDLNet_TCP_Open(&addr);
    if (!dzin_cli_conn) {
        printf("[NET-CLI ERR] TCP_Open (%s:%d): %s\n", host, port, SDLNet_GetError());
        return 0;
    }

    SDLNet_TCP_AddSocket(dzin_cli_set, dzin_cli_conn);
    printf("[NET-CLI] Polaczono z serwerem %s:%d\n", host, port);
    return 1;
}

// 2. Wysyłanie tekstu / myśli Dżina do serwera (nieblokujące dla małych pakietów)
static inline int dzin_client_send(const char *msg) {
    if (!dzin_cli_conn || !msg || strlen(msg) == 0) return 0;

    int len = (int)strlen(msg) + 1;
    int sent = SDLNet_TCP_Send(dzin_cli_conn, msg, len);
    if (sent < len) {
        printf("[NET-CLI] Blad wysylania lub rozlaczenie z serwerem.\n");
        SDLNet_TCP_DelSocket(dzin_cli_set, dzin_cli_conn);
        SDLNet_TCP_Close(dzin_cli_conn);
        dzin_cli_conn = NULL;
        return 0;
    }
    return 1;
}

// 3. Wywołujesz W PĘTLI GŁÓWNEJ (0 ms czekania!).
// Zwraca 1, gdy przyszła odpowiedź od Dżina-Serwera (wpisuje ją do out_msg), w przeciwnym razie 0.
static inline int dzin_client_update(char *out_msg, int max_len) {
    if (!dzin_cli_conn || !dzin_cli_set) return 0;

    if (SDLNet_CheckSockets(dzin_cli_set, 0) > 0) {
        if (SDLNet_SocketReady(dzin_cli_conn)) {
            int bytes = SDLNet_TCP_Recv(dzin_cli_conn, out_msg, max_len - 1);
            if (bytes <= 0) {
                // Serwer zamknął połączenie
                SDLNet_TCP_DelSocket(dzin_cli_set, dzin_cli_conn);
                SDLNet_TCP_Close(dzin_cli_conn);
                dzin_cli_conn = NULL;
                printf("[NET-CLI] Rozlaczono z serwerem.\n");
                return 0;
            }
            out_msg[bytes] = '\0';
            return 1; // Mamy odpowiedź z serwera!
        }
    }
    return 0;
}

// 4. Wywołujesz RAZ przy zamykaniu programu (przed SDL_Quit)
static inline void dzin_client_close(void) {
    if (dzin_cli_conn) {
        if (dzin_cli_set) SDLNet_TCP_DelSocket(dzin_cli_set, dzin_cli_conn);
        SDLNet_TCP_Close(dzin_cli_conn);
        dzin_cli_conn = NULL;
    }
    if (dzin_cli_set) {
        SDLNet_FreeSocketSet(dzin_cli_set);
        dzin_cli_set = NULL;
    }
    SDLNet_Quit();
}

#endif