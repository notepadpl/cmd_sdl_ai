#ifndef NETSTAT_H
#define NETSTAT_H

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

static void dzin_ns_strip_zone(char *ip6) {
    if (!ip6) return;
    char *pct = strchr(ip6, '%');
    if (pct) *pct = '\0';
}

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

    if (strcmp(raw_port, "https") == 0)    { snprintf(out, out_sz, "443");  return; }
    if (strcmp(raw_port, "http") == 0)     { snprintf(out, out_sz, "80");   return; }
    if (strcmp(raw_port, "http-alt") == 0) { snprintf(out, out_sz, "8080"); return; }
    if (strcmp(raw_port, "domain") == 0)   { snprintf(out, out_sz, "53");   return; }
    if (strcmp(raw_port, "ssh") == 0)      { snprintf(out, out_sz, "22");   return; }
    if (strcmp(raw_port, "ftp") == 0)      { snprintf(out, out_sz, "21");   return; }
    if (strcmp(raw_port, "bootps") == 0)   { snprintf(out, out_sz, "67");   return; }
    if (strcmp(raw_port, "bootpc") == 0)   { snprintf(out, out_sz, "68");   return; }

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

static void dzin_ns_push_entry(DzinNetLink *arr, int *count, int max_count,
                               const char *ip, const char *port_num, int is_v6) {
    if (*count >= max_count) return;
    if (dzin_ns_is_unspecified(ip)) return;
    if (dzin_ns_seen(arr, *count, ip, port_num)) return;

    snprintf(arr[*count].ip, sizeof(arr[*count].ip), "%s", ip);
    snprintf(arr[*count].port, sizeof(arr[*count].port), "%s", port_num);
    arr[*count].is_v6 = is_v6;
    (*count)++;
}

static void dzin_ns_add_ip(DzinNetLink *arr, int *count, int max_count,
                           const char *host, const char *port_raw) {
    if (!host || !*host || *count >= max_count) return;

    char pnum[32];
    dzin_ns_port_to_num(port_raw, pnum, sizeof(pnum));

    char host_clean[INET6_ADDRSTRLEN];
    snprintf(host_clean, sizeof(host_clean), "%s", host);
    dzin_ns_strip_zone(host_clean);

    char clean_ip[INET6_ADDRSTRLEN];

    /* 1. Adresy zmapowane IPv4-w-IPv6: ::ffff:1.2.3.4 */
    if (strncmp(host_clean, "::ffff:", 7) == 0) {
        struct in_addr a4_mapped;
        if (inet_pton(AF_INET, host_clean + 7, &a4_mapped) == 1 &&
            inet_ntop(AF_INET, &a4_mapped, clean_ip, sizeof(clean_ip))) {
            dzin_ns_push_entry(arr, count, max_count, clean_ip, pnum, 0);
            return;
        }
    }

    /* 2. Czysty IPv4 */
    struct in_addr a4;
    if (inet_pton(AF_INET, host_clean, &a4) == 1 &&
        inet_ntop(AF_INET, &a4, clean_ip, sizeof(clean_ip))) {
        dzin_ns_push_entry(arr, count, max_count, clean_ip, pnum, 0);
        return;
    }

    /* 3. Czysty IPv6 */
    struct in6_addr a6;
    if (inet_pton(AF_INET6, host_clean, &a6) == 1 &&
        inet_ntop(AF_INET6, &a6, clean_ip, sizeof(clean_ip))) {
        dzin_ns_push_entry(arr, count, max_count, clean_ip, pnum, 1);
        return;
    }
}

static void dzin_ns_parse_token(const char *tok, DzinNetLink *arr, int *count, int max_count) {
    if (!tok || !*tok) return;

    char buf[256];
    snprintf(buf, sizeof(buf), "%s", tok);

    /* Przypadek [2001:db8::1]:443 lub [fe80::1%eth0]:80 */
    if (buf[0] == '[') {
        char *rb = strchr(buf + 1, ']');
        if (!rb) return;
        *rb = '\0';
        const char *port_raw = (rb[1] == ':' && rb[2] != '\0') ? (rb + 2) : "80";
        dzin_ns_add_ip(arr, count, max_count, buf + 1, port_raw);
        return;
    }

    /* Przypadek samego adresu IPv6 bez portu (np. 2001:db8::1 lub fe80::1%eth0) */
    char v6_probe[INET6_ADDRSTRLEN];
    snprintf(v6_probe, sizeof(v6_probe), "%s", buf);
    dzin_ns_strip_zone(v6_probe);
    struct in6_addr a6_only;
    if (inet_pton(AF_INET6, v6_probe, &a6_only) == 1) {
        dzin_ns_add_ip(arr, count, max_count, v6_probe, "80");
        return;
    }

    /* Przypadek host:port (w tym 1.2.3.4:8080 oraz ::ffff:1.2.3.4:443) */
    char *colon = strrchr(buf, ':');
    if (!colon || colon == buf || colon[1] == '\0') return;
    *colon = '\0';
    dzin_ns_add_ip(arr, count, max_count, buf, colon + 1);
}

static int dzin_ns_collect_from_cmd(const char *cmd, DzinNetLink *arr, int *count, int max_count) {
    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    char line[1024];
    memset(line, 0, sizeof(line));

    while (fgets(line, sizeof(line), fp)) {
        char *saveptr = NULL;
        char *tok = strtok_r(line, " \t\r\n", &saveptr);
        while (tok) {
            dzin_ns_parse_token(tok, arr, count, max_count);
            tok = strtok_r(NULL, " \t\r\n", &saveptr);
        }
        memset(line, 0, sizeof(line));
    }

    memset(line, 0, sizeof(line));
    pclose(fp);
    return 0;
}

static int dzin_netstat_generate_html(const char *out_html_path, char *summary_out, size_t summary_sz) {
    DzinNetLink items[512];
    int count = 0;
    memset(items, 0, sizeof(items));

    if (summary_out && summary_sz > 0) {
        memset(summary_out, 0, summary_sz);
    }

    int rc1 = dzin_ns_collect_from_cmd("netstat -tun 2>/dev/null || netstat -an 2>/dev/null", items, &count, 512);
    if (count == 0) {
        int rc2 = dzin_ns_collect_from_cmd("ss -tun 2>/dev/null", items, &count, 512);
        if (rc1 != 0 && rc2 != 0) {
            if (summary_out && summary_sz > 0) {
                snprintf(summary_out, summary_sz, "[NETSTAT] Blad uruchomienia netstat / ss.");
            }
            memset(items, 0, sizeof(items));
            return -1;
        }
    }

    const char *path = (out_html_path && *out_html_path) ? out_html_path : "netstat.html";
    FILE *html = fopen(path, "w");
    if (!html) {
        if (summary_out && summary_sz > 0) {
            snprintf(summary_out, summary_sz, "[NETSTAT] Nie mozna zapisac %s", path);
        }
        memset(items, 0, sizeof(items));
        return -1;
    }

    fprintf(html, "<!DOCTYPE html>\n<html>\n<head><meta charset=\"utf-8\"><title>Netstat</title></head>\n<body>\n<ul>\n");
    for (int i = 0; i < count; i++) {
        const char *scheme = (strcmp(items[i].port, "443") == 0) ? "https" : "http";
        if (items[i].is_v6) {
            fprintf(html, "  <li><a href=\"%s://[%s]:%s\">[%s]:%s</a></li>\n",
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

    memset(items, 0, sizeof(items));
    count = 0;
    return 0;
}


#endif /* DZIN_NETSTAT_H */