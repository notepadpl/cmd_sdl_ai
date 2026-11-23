#ifndef CMD_GEN_H
#define CMD_GEN_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>

// Filtr pułapek: odrzuca komendy lustrzane, wrappery procesów i powłoki interaktywne
static int is_trap_command(const char *cmd)
{
    static const char *blacklist[] = {
        // Lustra (pętla samoreferencji)
        "echo", "printf",
        // Wrappery uruchamiające inne procesy lub pliki (np. test.txt)
        "env", "xargs", "nohup", "sudo", "su", "chroot", "time", "timeout", "exec","expand",
        // Narzędzia monitorujące / śledzące / pagery (ryzyko blokady)
        "watch", "strace", "ltrace", "gdb", "top", "htop", "less", "more", "vi", "vim", "nano",
        // Powłoki interaktywne
        "sh", "bash", "ash", "dash", "zsh"
    };

    int count = sizeof(blacklist) / sizeof(blacklist[0]);
    for (int i = 0; i < count; i++)
    {
        if (strcmp(cmd, blacklist[i]) == 0)
            return 1;
    }
    return 0;
}

// Krok 1: Skanuje katalog systemowy i zapisuje surowe komendy do pliku
static int dzin_scan_commands(const char *bin_dir, const char *out_filename)
{
    DIR *d = opendir(bin_dir);
    if (!d)
    {
        // Fallback dla zwykłego PC/Linuksa
        d = opendir("/bin");
        if (!d)
            return 0;
    }

    FILE *out = fopen(out_filename, "w");
    if (!out)
    {
        closedir(d);
        return 0;
    }

    int count = 0;
    struct dirent *dir;
    while ((dir = readdir(d)) != NULL)
    {
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
            continue;
        fprintf(out, "%s\n", dir->d_name);
        count++;
    }

    fclose(out);
    closedir(d);
    printf("[KROK 1] Zeskanowano %d komend do %s\n", count, out_filename);
    return count;
}

// Krok 2: Czyta surowe komendy, filtruje pułapki, odpala -h / --help z </dev/null i tworzy kombinacje
// Krok 2: Czyta surowe komendy, całkowicie omija pułapki i generuje czyste kombinacje
// Krok 2: Czyta surowe komendy, filtruje pułapki, dynamicznie wczytuje całą pomoc i tworzy kombinacje
static int dzin_generate_flags(const char *in_filename, const char *out_filename)
{
    FILE *in = fopen(in_filename, "r");
    if (!in)
        return 0;

    FILE *out = fopen(out_filename, "w");
    if (!out)
    {
        fclose(in);
        return 0;
    }

    char cmd[128];
    int total_combos = 0;

    while (fgets(cmd, sizeof(cmd), in))
    {
        cmd[strcspn(cmd, "\r\n")] = 0;
        if (strlen(cmd) == 0)
            continue;

        if (is_trap_command(cmd))
            continue;

        // Zapis podstawowej linii + natychmiastowy zrzut bufora na dysk
        fprintf(out, "%s -h\n", cmd);
        fflush(out);
        total_combos++;

        // Diagnostyka w czasie rzeczywistym (widzisz dokładnie, co teraz mieli)
        printf("[KROK 2] (%d) %s\n", total_combos, cmd);
        fflush(stdout);

        // Kwerenda z limitem czasu (1s) oraz odcięciem stdin (</dev/null)
        char query[512];
        snprintf(query, sizeof(query), 
                 "timeout 1 %s --help </dev/null 2>&1 || timeout 1 %s -h </dev/null 2>&1", 
                 cmd, cmd);

        FILE *fp = popen(query, "r");
        if (fp)
        {
            size_t buf_size = 4096;
            size_t total_len = 0;
            size_t max_limit = 65536; // Bezpiecznik 64 KB na jedną komendę
            char *buffer = (char*)malloc(buf_size);

            if (buffer)
            {
                char chunk[1024];
                size_t bytes_read;
                while (total_len < max_limit && (bytes_read = fread(chunk, 1, sizeof(chunk), fp)) > 0)
                {
                    if (total_len + bytes_read >= buf_size)
                    {
                        buf_size *= 2;
                        char *new_buf = (char*)realloc(buffer, buf_size);
                        if (!new_buf) break;
                        buffer = new_buf;
                    }
                    memcpy(buffer + total_len, chunk, bytes_read);
                    total_len += bytes_read;
                }
                buffer[total_len] = '\0';
            }
            pclose(fp);

            if (buffer)
            {
                char *ptr = buffer;
                char flags[10][16];
                int flag_count = 0;

                while ((ptr = strstr(ptr, "-")) != NULL && flag_count < 10)
                {
                    if (ptr == buffer || isspace((unsigned char)*(ptr - 1)))
                    {
                        char f[16] = { 0 };
                        int f_idx = 0;
                        f[f_idx++] = *ptr++;

                        while (*ptr && (isalnum((unsigned char)*ptr) || *ptr == '-') && f_idx < 14)
                        {
                            f[f_idx++] = *ptr++;
                        }
                        f[f_idx] = '\0';

                        if (strlen(f) > 1)
                        {
                            int dup = 0;
                            for (int k = 0; k < flag_count; k++)
                            {
                                if (strcmp(flags[k], f) == 0)
                                {
                                    dup = 1;
                                    break;
                                }
                            }
                            if (!dup)
                            {
                                strcpy(flags[flag_count++], f);
                            }
                        }
                    }
                    else
                    {
                        ptr++;
                    }
                }

                for (int i = 0; i < flag_count; i++)
                {
                    fprintf(out, "%s %s\n", cmd, flags[i]);
                    total_combos++;

                    if (i + 1 < flag_count)
                    {
                        fprintf(out, "%s %s %s\n", cmd, flags[i], flags[i + 1]);
                        total_combos++;
                    }
                }
                fflush(out); // Zrzut nowych flag na dysk od razu po każdej komendzie
                free(buffer);
            }
        }
    }

    fclose(in);
    fclose(out);
    printf("[KROK 2] Wygenerowano %d kombinacji flag w %s\n", total_combos, out_filename);
    return total_combos;
}
// Krok 3: Bierze kombinacje z kroku 2 i dopisuje do każdej z nich zawartość test.txt
static int dzin_append_testfile(const char *in_filename, const char *test_filename,
                                const char *final_dict_filename)
{
    char test_arg[128] = "test.txt";
    FILE *tf = fopen(test_filename, "r");
    if (tf)
    {
        if (fgets(test_arg, sizeof(test_arg), tf))
        {
            test_arg[strcspn(test_arg, "\r\n")] = 0;
        }
        fclose(tf);
    }

    FILE *in = fopen(in_filename, "r");
    if (!in)
        return 0;

    FILE *out = fopen(final_dict_filename, "w");
    if (!out)
    {
        fclose(in);
        return 0;
    }

    char line[256];
    int final_count = 0;

    while (fgets(line, sizeof(line), in))
    {
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0)
            continue;

        // Doklejamy argument z test.txt na samym końcu każdej linii
        fprintf(out, "%s %s\n", line, test_arg);
        final_count++;
    }

    fclose(in);
    fclose(out);
    printf("[KROK 3] Stworzono ostateczny słownik Dżina: %s (%d linii)\n", final_dict_filename,
           final_count);
    return final_count;
}

// Główna funkcja wywołująca cały generator (zamiast main)
static int cmd_g(void)
{
    printf("=== GENERATOR SLOWNIKA KOMEND DZINA ===\n\n");

    // Zabezpieczenie: jeśli brakuje pliku test.txt, tworzymy domyślny
    FILE *check = fopen("test.txt", "r");
    if (!check)
    {
        FILE *create_test = fopen("test.txt", "w");
        if (create_test)
        {
            fprintf(create_test, "test.txt\n");
            fclose(create_test);
            printf("[INFO] Utworzono domyslny plik test.txt\n");
        }
    }
    else
    {
        fclose(check);
    }

    // Krok 1: Skanowanie katalogu systemowego i zapis surowych nazw do pliku
    int scanned = dzin_scan_commands("/system/bin", "1_surowe.txt");
    if (scanned == 0)
    {
        printf("[BLAD] Nie znaleziono komend w /system/bin ani /bin.\n");
        return 1;
    }

    // Krok 2: Odpalenie -h / --help, wyłapanie flag i generowanie kombinacji
    int combos = dzin_generate_flags("1_surowe.txt", "2_kombinacje.txt");
    if (combos == 0)
    {
        printf("[BLAD] Nie udalo sie wygenerowac kombinacji flag.\n");
        return 1;
    }

    // Krok 3: Doklejenie test.txt na końcu każdej kombinacji
    int final_lines =
        dzin_append_testfile("2_kombinacje.txt", "test.txt", "dzin_slownik_koncowy.txt");

    printf("\n=== ZAKONCZONO ===\n");
    printf("Gotowy plik dla Dzin-Serwera i Dzin-Klienta: dzin_slownik_koncowy.txt (%d linii)\n",
           final_lines);

    return 0;
}

#endif // CMD_GEN_H