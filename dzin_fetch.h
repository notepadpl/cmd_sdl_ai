#ifndef DZIN_FETCH_H
#define DZIN_FETCH_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
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


#endif