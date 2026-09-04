/**
 * @brief 配置编解码 - HTML 转义 / URL 解码 / 表单解析
 *
 * 该模块不依赖 ESP-IDF，可在宿主机上编译单元测试。
 */

#include "config_codec.h"

#include <string.h>

size_t html_escape(const char* in, char* out, size_t out_sz)
{
    size_t n = 0;

    if (!in) {
        if (out && out_sz > 0) out[0] = '\0';
        return 0;
    }

    for (size_t i = 0; in[i]; i++) {
        const char* rep;
        switch (in[i]) {
        case '&':  rep = "&amp;";  break;
        case '<':  rep = "&lt;";   break;
        case '>':  rep = "&gt;";   break;
        case '"':  rep = "&quot;"; break;
        case '\'': rep = "&#39;";  break;
        default:
            rep = NULL;
            break;
        }

        if (rep) {
            size_t rl = strlen(rep);
            if (out) {
                for (size_t j = 0; j < rl; j++) {
                    if (n + 1 < out_sz) out[n] = rep[j];
                    n++;
                }
            } else {
                n += rl;
            }
        } else {
            if (out && n + 1 < out_sz) out[n] = in[i];
            n++;
        }
    }

    if (out && out_sz > 0) out[n < out_sz ? n : out_sz - 1] = '\0';
    return n;
}

static int hex_val(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

size_t url_decode(const char* in, char* out, size_t out_sz)
{
    size_t n = 0;

    if (!in) {
        if (out && out_sz > 0) out[0] = '\0';
        return 0;
    }

    for (size_t i = 0; in[i]; i++) {
        int hi, lo;
        if (in[i] == '+') {
            if (out && n + 1 < out_sz) out[n] = ' ';
            n++;
        } else if (in[i] == '%' && (hi = hex_val(in[i + 1])) >= 0 &&
                   (lo = hex_val(in[i + 2])) >= 0) {
            if (out && n + 1 < out_sz) out[n] = (char)((hi << 4) | lo);
            n++;
            i += 2;
        } else {
            if (out && n + 1 < out_sz) out[n] = in[i];
            n++;
        }
    }

    if (out && out_sz > 0) out[n < out_sz ? n : out_sz - 1] = '\0';
    return n;
}

/* 目标字段及其大小，避免重复的 if/else 链 */
#define FIELD(member) \
    { #member, sizeof(#member) - 1, offsetof(app_config_t, member), \
      sizeof(((app_config_t*)0)->member) }

static const struct {
    const char* key;
    size_t      key_len;
    size_t      offset;
    size_t      size;
} s_fields[] = {
    FIELD(wifi_ssid),
    FIELD(wifi_password),
    FIELD(campus_username),
    FIELD(campus_password),
    FIELD(channel),
};

#undef FIELD

static const size_t s_field_count = sizeof(s_fields) / sizeof(s_fields[0]);

bool form_parse(char* body, app_config_t* out)
{
    if (!out) return false;

    memset(out, 0, sizeof(*out));
    /* 未勾选/未提供时保持 CVersion 默认通道 */
    strncpy(out->channel, "phone", sizeof(out->channel) - 1);

    if (!body) return false;

    char* kv = body;
    while (kv && *kv) {
        char* next = strchr(kv, '&');
        if (next) { *next = '\0'; next++; }

        char* eq = strchr(kv, '=');
        if (eq) {
            *eq = '\0';
            char* key = kv;
            char* val = eq + 1;
            size_t key_len = strlen(key);

            for (size_t i = 0; i < s_field_count; i++) {
                if (s_fields[i].key_len == key_len &&
                    memcmp(s_fields[i].key, key, key_len) == 0) {
                    char* dst = (char*)out + s_fields[i].offset;
                    url_decode(val, dst, s_fields[i].size);
                    break;
                }
            }
        }
        kv = next;
    }

    return out->wifi_ssid[0] != '\0' && out->campus_username[0] != '\0';
}
