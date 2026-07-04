/*
 * WaTTrack desktop launcher
 * ---------------------------
 * A tiny, dependency-free static file server for Windows.
 *
 * Why this exists: WaTTrack is a 100% client-side Vite/React app (home
 * energy audit: device inventory, consumption charts, upgrade simulator,
 * PDF export). It ships an unused express/dotenv/@google-genai dependency
 * left over from its scaffolding template, but nothing in src/ actually
 * calls a backend or an AI API - every calculation, chart, and PDF
 * export runs in the browser, and audit data is saved locally via
 * localStorage. So a native build just needs to serve the built dist/
 * folder over localhost; no proxy or API logic to reimplement at all.
 *
 * Note on localStorage: because it's scoped per-origin (host+port),
 * this program always tries port 3000 first so the app's saved audits
 * stay reachable across runs. It only moves to another port if 3000 is
 * genuinely unavailable.
 *
 * Behavior:
 *   - Serves ./dist next to the executable on http://127.0.0.1:<port>
 *   - Loopback-only (127.0.0.1), never 0.0.0.0, so Windows Firewall
 *     shouldn't prompt on first run.
 *   - Tries ports 3000..3019 until one is free.
 *   - Auto-opens the default browser once the server is ready.
 *   - Unmatched routes fall back to index.html (standard SPA fallback).
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "shell32.lib")

#define PORT_START 3000
#define PORT_TRIES 20
#define REQUEST_BUF_SIZE 16384

static char g_dist_dir[MAX_PATH];

/* ---------------------------------------------------------------------- */
/* Utility: MIME type lookup                                              */
/* ---------------------------------------------------------------------- */
static const char *get_mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";

    if (_stricmp(dot, ".html") == 0 || _stricmp(dot, ".htm") == 0) return "text/html; charset=utf-8";
    if (_stricmp(dot, ".js") == 0 || _stricmp(dot, ".mjs") == 0)   return "text/javascript; charset=utf-8";
    if (_stricmp(dot, ".css") == 0)   return "text/css; charset=utf-8";
    if (_stricmp(dot, ".json") == 0)  return "application/json; charset=utf-8";
    if (_stricmp(dot, ".map") == 0)   return "application/json; charset=utf-8";
    if (_stricmp(dot, ".svg") == 0)   return "image/svg+xml";
    if (_stricmp(dot, ".png") == 0)   return "image/png";
    if (_stricmp(dot, ".jpg") == 0 || _stricmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (_stricmp(dot, ".gif") == 0)   return "image/gif";
    if (_stricmp(dot, ".webp") == 0)  return "image/webp";
    if (_stricmp(dot, ".ico") == 0)   return "image/x-icon";
    if (_stricmp(dot, ".woff") == 0)  return "font/woff";
    if (_stricmp(dot, ".woff2") == 0) return "font/woff2";
    if (_stricmp(dot, ".ttf") == 0)   return "font/ttf";
    if (_stricmp(dot, ".eot") == 0)   return "application/vnd.ms-fontobject";
    if (_stricmp(dot, ".txt") == 0)   return "text/plain; charset=utf-8";
    if (_stricmp(dot, ".wasm") == 0)  return "application/wasm";
    return "application/octet-stream";
}

/* ---------------------------------------------------------------------- */
/* Utility: percent-decode a URL path in place-ish (dst must be >= src)    */
/* Returns 0 on success, -1 on malformed/unsafe input.                    */
/* ---------------------------------------------------------------------- */
static int url_decode(const char *src, char *dst, size_t dst_size) {
    size_t si = 0, di = 0;
    size_t slen = strlen(src);

    while (si < slen) {
        char c = src[si];
        if (c == '?') break; /* stop at query string */

        if (c == '%' && si + 2 < slen &&
            isxdigit((unsigned char)src[si + 1]) &&
            isxdigit((unsigned char)src[si + 2])) {
            char hex[3] = { src[si + 1], src[si + 2], 0 };
            int val = (int)strtol(hex, NULL, 16);
            if (val == 0) return -1; /* reject embedded NUL */
            if (di + 1 >= dst_size) return -1;
            dst[di++] = (char)val;
            si += 3;
        } else if (c == '+') {
            if (di + 1 >= dst_size) return -1;
            dst[di++] = ' ';
            si++;
        } else {
            if (di + 1 >= dst_size) return -1;
            dst[di++] = c;
            si++;
        }
    }
    dst[di] = '\0';
    return 0;
}

/* Reject paths that could escape the dist directory. */
static int is_safe_path(const char *decoded_path) {
    if (decoded_path[0] != '/') return 0;
    if (strstr(decoded_path, "..") != NULL) return 0;
    if (strstr(decoded_path, "\\") != NULL) return 0; /* no backslashes from a URL path */
    return 1;
}

/* ---------------------------------------------------------------------- */
/* Utility: read an entire file into a malloc'd buffer                    */
/* ---------------------------------------------------------------------- */
static int read_entire_file(const char *path, char **out_buf, long *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }

    char *buf = (char *)malloc((size_t)size > 0 ? (size_t)size : 1);
    if (!buf) { fclose(f); return -1; }

    size_t read_bytes = size > 0 ? fread(buf, 1, (size_t)size, f) : 0;
    fclose(f);

    if ((long)read_bytes != size) { free(buf); return -1; }

    *out_buf = buf;
    *out_size = size;
    return 0;
}

/* ---------------------------------------------------------------------- */
/* Utility: send() until every byte is written                            */
/* ---------------------------------------------------------------------- */
static int send_all(SOCKET s, const char *buf, int len) {
    int sent_total = 0;
    while (sent_total < len) {
        int sent = send(s, buf + sent_total, len - sent_total, 0);
        if (sent == SOCKET_ERROR) return -1;
        sent_total += sent;
    }
    return 0;
}

static void send_status_only(SOCKET s, const char *status_line) {
    char header[256];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\nConnection: close\r\nContent-Length: 0\r\n\r\n", status_line);
    send_all(s, header, n);
}

/* ---------------------------------------------------------------------- */
/* Connection handler                                                     */
/* ---------------------------------------------------------------------- */
static unsigned __stdcall handle_client(void *arg) {
    SOCKET client = (SOCKET)(uintptr_t)arg;
    char req[REQUEST_BUF_SIZE];
    int total = 0;

    /* Read until we have a full request line + headers terminator, or fill the buffer. */
    while (total < (int)sizeof(req) - 1) {
        int n = recv(client, req + total, (int)sizeof(req) - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        req[total] = '\0';
        if (strstr(req, "\r\n\r\n") != NULL) break;
        if (strstr(req, "\r\n") != NULL && total < 4) break;
    }

    if (total <= 0) {
        closesocket(client);
        return 0;
    }
    req[total > 0 ? total : 0] = '\0';

    char method[8] = {0};
    char raw_path[1024] = {0};
    if (sscanf(req, "%7s %1023s", method, raw_path) != 2) {
        send_status_only(client, "400 Bad Request");
        closesocket(client);
        return 0;
    }

    int is_head = (_stricmp(method, "HEAD") == 0);
    int is_get = (_stricmp(method, "GET") == 0);

    if (!is_get && !is_head) {
        send_status_only(client, "405 Method Not Allowed");
        closesocket(client);
        return 0;
    }

    char decoded[1024];
    if (url_decode(raw_path, decoded, sizeof(decoded)) != 0 || !is_safe_path(decoded)) {
        send_status_only(client, "400 Bad Request");
        closesocket(client);
        return 0;
    }

    /* Map "/" to the SPA entry point. */
    const char *rel = decoded;
    if (strcmp(rel, "/") == 0) rel = "/index.html";

    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", g_dist_dir, rel);
    /* Convert forward slashes to backslashes for Windows file APIs. */
    for (char *p = full_path; *p; p++) if (*p == '/') *p = '\\';

    DWORD attrs = GetFileAttributesA(full_path);
    int found = (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);

    char index_path[MAX_PATH];
    snprintf(index_path, sizeof(index_path), "%s\\index.html", g_dist_dir);

    const char *serve_path = found ? full_path : index_path;
    const char *mime = get_mime_type(found ? full_path : "index.html");

    char *file_buf = NULL;
    long file_size = 0;
    if (read_entire_file(serve_path, &file_buf, &file_size) != 0) {
        send_status_only(client, "404 Not Found");
        closesocket(client);
        return 0;
    }

    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n"
        "\r\n",
        mime, file_size);

    if (send_all(client, header, header_len) == 0 && !is_head) {
        send_all(client, file_buf, (int)file_size);
    }

    free(file_buf);
    shutdown(client, SD_SEND);
    closesocket(client);
    return 0;
}

/* ---------------------------------------------------------------------- */
/* Startup helpers                                                        */
/* ---------------------------------------------------------------------- */
static int get_exe_dir(char *buf, size_t buf_size) {
    DWORD len = GetModuleFileNameA(NULL, buf, (DWORD)buf_size);
    if (len == 0 || len == buf_size) return -1;
    char *last_slash = strrchr(buf, '\\');
    if (!last_slash) return -1;
    *last_slash = '\0';
    return 0;
}

static SOCKET create_and_bind(int *out_port) {
    for (int i = 0; i < PORT_TRIES; i++) {
        int port = PORT_START + i;
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) continue;

        BOOL reuse = TRUE;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));

        struct sockaddr_in addr;
        ZeroMemory(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons((u_short)port);

        if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(s);
            continue;
        }
        if (listen(s, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(s);
            continue;
        }
        *out_port = port;
        return s;
    }
    return INVALID_SOCKET;
}

int main(void) {
    printf("========================================\n");
    printf("  WaTTrack - Home Energy Auditor\n");
    printf("========================================\n\n");

    char exe_dir[MAX_PATH];
    if (get_exe_dir(exe_dir, sizeof(exe_dir)) != 0) {
        printf("Could not determine the application folder.\n");
        printf("Press Enter to exit...\n");
        getchar();
        return 1;
    }
    snprintf(g_dist_dir, sizeof(g_dist_dir), "%s\\dist", exe_dir);

    char check_index[MAX_PATH];
    snprintf(check_index, sizeof(check_index), "%s\\index.html", g_dist_dir);
    DWORD attrs = GetFileAttributesA(check_index);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        printf("Could not find:\n  %s\n\n", check_index);
        printf("Make sure the 'dist' folder is next to WaTTrack.exe.\n");
        printf("Press Enter to exit...\n");
        getchar();
        return 1;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Failed to initialize networking (WSAStartup).\n");
        printf("Press Enter to exit...\n");
        getchar();
        return 1;
    }

    int port = 0;
    SOCKET listen_sock = create_and_bind(&port);
    if (listen_sock == INVALID_SOCKET) {
        printf("Could not find a free port between %d and %d.\n", PORT_START, PORT_START + PORT_TRIES - 1);
        printf("Close other applications that might be using these ports and try again.\n");
        printf("Press Enter to exit...\n");
        getchar();
        WSACleanup();
        return 1;
    }

    char url[64];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d/", port);

    printf("Server ready at: %s\n\n", url);
    printf("Opening your default browser...\n");
    printf("Keep this window open while using WaTTrack.\n");
    printf("Close this window to stop the app.\n\n");
    fflush(stdout);

    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);

    for (;;) {
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client = accept(listen_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client == INVALID_SOCKET) continue;

        uintptr_t th = _beginthreadex(NULL, 0, handle_client, (void *)(uintptr_t)client, 0, NULL);
        if (th == 0) {
            closesocket(client);
        } else {
            CloseHandle((HANDLE)th);
        }
    }

    closesocket(listen_sock);
    WSACleanup();
    return 0;
}
