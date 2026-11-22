#ifndef DZIN_NETSTAT_H
#define DZIN_NETSTAT_H

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char ip[INET6_ADDRSTRLEN];
    char port[32];
    int is_v6;
} DzinNetLink;

static void dzin_ns_port_to_num(const char *raw_port, char *out, size_t out_sz) {
    if (!raw_port || !*raw_port) {
        snprintf(out, out_sz, "80");
        return;
    }

    int all_digits = 1;
    for (const char *p = raw_port; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            all_digits = 0;
            break;
        }
    }
    if (all_digits) {
        snprintf(out, out_sz, "%s", raw_port);
        return;
    }

    if (strcmp(raw_port, "https") == 0)  { snprintf(out, out_sz, "443"); return; }
    if (strcmp(raw_port, "http") == 0)   { snprintf(out, out_sz, "80");  return; }
    if (strcmp(raw_port, "domain") == 0) { snprintf(out, out_sz, "53");  return; }
    if (strcmp(raw_port, "bootps") == 0) { snprintf(out, out_sz, "67");  return; }
    if (strcmp(raw_port, "bootpc") == 0) { snprintf(out, out_sz, "68");  return; }

    snprintf(out, out_sz, "%s", raw_port);
}

static int dzin_ns_seen(const DzinNetLink *arr, int count, const char *ip, const char *port) {
    for (int i = 0; i < count; i++) {
        if (strcmp(arr[i].ip, ip) == 0 && strcmp(arr[i].port, port) == 0) {
            return 1;
        }
    }
    return 0;
}

static int dzin_ns_is_unspecified(const char *ip) {
    return (strcmp(ip, "0.0.0.0") == 0 || strcmp(ip, "::") == 0);
}

static void dzin_ns_add_ip(DzinNetLink *arr, int *count, int max_count,
                           const char *host, const char *port_raw) {
    if (!host || !*host || *count >= max_count) return;

    char pnum[32];
    dzin_ns_port_to_num(port_raw, pnum, sizeof(pnum));

    char clean_ip[INET6_ADDRSTRLEN];

    /* Obsługa ::ffff:1.2.3.4 */
    if (strncmp(host, "::ffff:", 7) == 0) {
        struct in_addr a4;
        if (inet_pton(AF_INET, host + 7, &a4) == 1 &&
            inet_ntop(AF_INET, &a4, clean_ip, sizeof(clean_ip))) {
            if (!dzin_ns_is_unspecified(clean_ip) && !dzin_ns_seen(arr, *count, clean_ip, pnum)) {
                snprintf(arr[*count].ip, sizeof(arr[*count].ip), "%s", clean_ip);
                snprintf(arr[*count].port, sizeof(arr[*count].port), "%s", pnum);
                arr[*count].is_v6 = 0;
                (*count)++;
            }
            return;
        }
    }

    struct in_addr a4;
    if (inet_pton(AF_INET, host, &a4) == 1 &&
        inet_ntop(AF_INET, &a4, clean_ip, sizeof(clean_ip))) {
        if (!dzin_ns_is_unspecified(clean_ip) && !dzin_ns_seen(arr, *count, clean_ip, pnum)) {
            snprintf(arr[*count].ip, sizeof(arr[*count].ip), "%s", clean_ip);
            snprintf(arr[*count].port, sizeof(arr[*count].port), "%s", pnum);
            arr[*count].is_v6 = 0;
            (*count)++;
        }
        return;
    }

    struct in6_addr a6;
    if (inet_pton(AF_INET6, host, &a6) == 1 &&
        inet_ntop(AF_INET6, &a6, clean_ip, sizeof(clean_ip))) {
        if (!dzin_ns_is_unspecified(clean_ip) && !dzin_ns_seen(arr, *count, clean_ip, pnum)) {
            snprintf(arr[*count].ip, sizeof(arr[*count].ip), "%s", clean_ip);
            snprintf(arr[*count].port, sizeof(arr[*count].port), "%s", pnum);
            arr[*count].is_v6 = 1;
            (*count)++;
        }
        return;
    }
}

static void dzin_ns_parse_token(const char *tok, DzinNetLink *arr, int *count, int max_count) {
    if (!tok || !*tok) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s", tok);

    if (buf[0] == '[') {
        char *rb = strchr(buf + 1, ']');
        if (!rb) return;
        *rb = '\0';
        const char *port_raw = (rb[1] == ':' && rb[2] != '\0') ? (rb + 2) : "80";
        dzin_ns_add_ip(arr, count, max_count, buf + 1, port_raw);
        return;
    }

    char *colon = strrchr(buf, ':');
    if (!colon || colon == buf || colon[1] == '\0') return;
    *colon = '\0';
    dzin_ns_add_ip(arr, count, max_count, buf, colon + 1);
}

static int dzin_netstat_generate_html(const char *out_html_path, char *summary_out, size_t summary_sz) {
    FILE *fp = popen("netstat 2>/dev/null", "r");
    if (!fp) {
        if (summary_out && summary_sz > 0) {
            snprintf(summary_out, summary_sz, "[NETSTAT] Blad uruchomienia netstat.");
        }
        return -1;
    }

    DzinNetLink items[512];
    int count = 0;
    char line[1024];

    while (fgets(line, sizeof(line), fp)) {
        char *saveptr = NULL;
        char *tok = strtok_r(line, " \t\r\n", &saveptr);
        while (tok) {
            dzin_ns_parse_token(tok, items, &count, 512);
            tok = strtok_r(NULL, " \t\r\n", &saveptr);
        }
    }
    pclose(fp);

    const char *path = (out_html_path && *out_html_path) ? out_html_path : "netstat.html";
    FILE *html = fopen(path, "w");
    if (!html) {
        if (summary_out && summary_sz > 0) {
            snprintf(summary_out, summary_sz, "[NETSTAT] Nie mozna zapisac %s", path);
        }
        return -1;
    }

    fprintf(html, "<!DOCTYPE html>\n<html>\n<head><meta charset=\"utf-8\"><title>Netstat</title></head>\n<body>\n<ul>\n");
    for (int i = 0; i < count; i++) {
        const char *scheme = (strcmp(items[i].port, "443") == 0) ? "https" : "http";
        if (items[i].is_v6) {
            fprintf(html, "  <li><a href=\"%s://[%s]:%s\">%s:%s</a></li>\n",
                    scheme, items[i].ip, items[i].port, items[i].ip, items[i].port);
        } else {
            fprintf(html, "  <li><a href=\"%s://%s:%s\">%s:%s</a></li>\n",
                    scheme, items[i].ip, items[i].port, items[i].ip, items[i].port);
        }
    }
    fprintf(html, "</ul>\n</body>\n</html>\n");
    fclose(html);

    if (summary_out && summary_sz > 0) {
        snprintf(summary_out, summary_sz, "[NETSTAT] Zapisano %d linkow do %s", count, path);
    }
    return count;
}

#endif /* DZIN_NETSTAT_H */