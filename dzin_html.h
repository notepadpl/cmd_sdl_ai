#ifndef DZIN_HTML_H
#define DZIN_HTML_H

#include <stdio.h>
#include <string.h>
#include "dzin_io.h"
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

static inline void emit_dzin_reply(const char* prefix, const char* msg) {
    if (!msg || msg[0] == '\0') return;
    char full_msg[LINE_LEN];
    snprintf(full_msg, sizeof(full_msg), "%s%s", prefix ? prefix : "", msg);
    push_to_global_output(full_msg);
#ifdef __EMSCRIPTEN__
    js_on_dzin_reply(full_msg);
#endif
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void zewnetrzny_input_ai(const char* user_text) {
    if (!user_text || user_text[0] == '\0') return;
    parse_and_learn_command(user_text, "USER-WEB", 1);
}

#endif /* DZIN_HTML_H */
/*
```[cite: 1, 2, 3]

### Jak to wpiąć w `main.c` bez chochlików?

* W `main.c` (po `#include "dzin_io.h"`) dodaj `#include "dzin_html.h"`[cite: 2, 3].
* W `main_loop` używaj `dzin_html_emit_reply("[KLIENT] ", cli_in);` (oraz pod `#ifndef __EMSCRIPTEN__` `dzin_html_emit_reply("[SERWER] ", srv_in);`), zamiast ręcznie kleić `append_prefixed_to_user_console`[cite: 2].
static void emit_dzin_reply(const char* prefix, const char* msg) {
    if (!msg || msg[0] == '\0') return;
    append_prefixed_to_user_console(prefix, msg);
#ifdef __EMSCRIPTEN__
    char full_msg[LINE_LEN];
    snprintf(full_msg, sizeof(full_msg), "%s%s", prefix ? prefix : "", msg);
    js_on_dzin_reply(full_msg);
#endif
}
Na tym etapie najlepiej najpierw sprawdzić na żywo w GitHub Actions / przeglądarce, czy build przechodzi czysto i czy nie ma już żadnego `undefined reference` — jak pojawi się jakikolwiek błąd kompilacji, wklej tu dokładną linijkę, a od razu go złapiemy.
*/