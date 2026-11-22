#ifndef DZIN_BABBLE_H
#define DZIN_BABBLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "dzin_io.h"
/* Zakłada, że przed dzin_babble.h włączone są:
 * dzin_core.h (ocean_mutex, onnx_model_buffer, onnx_model_size,
 *              ocean_index, trained_model, dzin_atomic_molecular_mutation)
 * dzin_client.h (dzin_client_send)
 * oraz w main.c zdefiniowane są: push_to_global_output, check_and_reward_genie_thought */

static uint32_t dzin_babble_mix32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static void dzin_babble_generate_ex(char *out, size_t out_sz,
                                    int gen,
                                    const uint8_t *buf,
                                    long buf_size,
                                    long idx) {
    static const char *alfabet = "abcdefghijklmnopqrstuvwxyz";
    size_t alpha_len = strlen(alfabet);
    if (!out || out_sz == 0 || alpha_len == 0) return;

    if (gen < 1) gen = 1;

    uint8_t sample = (uint8_t)(gen * 37 + (int)(idx & 0xFF));
    if (buf && buf_size > 0) {
        long pos = idx % buf_size;
        if (pos < 0) pos += buf_size;
        sample = buf[pos];
    }

    uint32_t state = dzin_babble_mix32((uint32_t)gen * 0x9e3779b9U ^
                                      (uint32_t)idx ^
                                      ((uint32_t)sample * 2654435761U));

    size_t target_chars = (size_t)(6 + (gen % 22));
    if (target_chars + 4 >= out_sz) {
        target_chars = (out_sz > 4) ? (out_sz - 4) : 1;
    }

    char stream[128];
    size_t pos_out = 0;

    for (size_t i = 0; i < target_chars && pos_out + 2 < sizeof(stream); i++) {
        uint8_t net_byte = sample;
        if (buf && buf_size > 0) {
            long p = (idx + (long)i * (1 + (gen % 7))) % buf_size;
            if (p < 0) p += buf_size;
            net_byte ^= buf[p];
        }

        state = dzin_babble_mix32(state + (uint32_t)net_byte * 131U + (uint32_t)i * 17U);

        if (gen >= 3 && i > 0 && (state % 5U) == 0U && stream[pos_out - 1] != ' ') {
            stream[pos_out++] = ' ';
        }

        size_t a_idx = (size_t)(state % (uint32_t)alpha_len);
        stream[pos_out++] = alfabet[a_idx];
    }
    stream[pos_out] = '\0';

    snprintf(out, out_sz, "%s", stream);
}

static void dzin_babble_tick(char *last_thought_out, size_t last_thought_sz, int *scroll_offset_ptr) {
    static int s_gen = 1;
    static long s_step = 0;

    s_step++;
    if (s_step % 4 == 0) {
        s_gen++;
    }

    const uint8_t *buf_snap = NULL;
    long size_snap = 0;
    long idx_snap = s_step;

    pthread_mutex_lock(&ocean_mutex);
    buf_snap = onnx_model_buffer;
    size_snap = onnx_model_size;
    if (size_snap > 0) {
        ocean_index = (ocean_index + 1) % size_snap;
        if (ocean_index < 0) ocean_index += size_snap;
    } else {
        ocean_index++;
    }
    idx_snap = ocean_index + s_step;
    pthread_mutex_unlock(&ocean_mutex);

    char babble_buf[LINE_LEN];
    dzin_babble_generate_ex(babble_buf, sizeof(babble_buf), s_gen, buf_snap, size_snap, idx_snap);

    if (last_thought_out && last_thought_sz > 0 && babble_buf[0] != '\0') {
        snprintf(last_thought_out, last_thought_sz, "%s", babble_buf);
    }

    char ui_line[LINE_LEN];
    snprintf(ui_line, sizeof(ui_line), "[DZIN g%d] %s", s_gen, babble_buf);
    push_to_global_output(ui_line);

    (void)dzin_client_send(babble_buf);
    check_and_reward_genie_thought(babble_buf);

    if (scroll_offset_ptr) {
        *scroll_offset_ptr = 0;
    }

    dzin_atomic_molecular_mutation(&trained_model, 0.3f);
}

#endif /* DZIN_BABBLE_H */