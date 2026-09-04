/**
 * @brief config_codec 单元测试 (宿主机 gcc)
 *
 * 覆盖 web_config 1.3.1 的四个缺陷:
 *   1. HTML 转义缺失   -> 密码含引号破坏表单结构
 *   2. URL 解码不全    -> 非 ASCII 字符 (UTF-8 中文) 被 strtol 错误处理
 *   3. 表单体硬截断    -> 512 字节上限, 长 SSID/密码静默丢失
 *   4. 非法 % 序列     -> 原实现读取 val[i+1]/val[i+2] 越界
 *
 * 编译: gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
 *            -I../ main/../main/test_config_codec.c main/config_codec.c -o t.exe
 */

#include "config_codec.h"

#include <stdio.h>
#include <string.h>

static int g_fail = 0;
static int g_pass = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (cond) {                                                        \
            g_pass++;                                                      \
        } else {                                                           \
            g_fail++;                                                      \
            printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                  \
    } while (0)

#define CHECK_STR(actual, expect)                                          \
    do {                                                                   \
        const char* a_ = (actual);                                         \
        const char* e_ = (expect);                                         \
        if (a_ && e_ && strcmp(a_, e_) == 0) {                             \
            g_pass++;                                                      \
        } else {                                                           \
            g_fail++;                                                      \
            printf("  FAIL %s:%d\n    expect: \"%s\"\n    actual: \"%s\"\n", \
                   __FILE__, __LINE__, e_, a_ ? a_ : "(null)");            \
        }                                                                  \
    } while (0)

/* ============ html_escape ============ */

static void test_html_escape(void)
{
    char buf[256];

    printf("html_escape\n");

    /* 普通字符串原样输出 */
    html_escape("hello", buf, sizeof(buf));
    CHECK_STR(buf, "hello");

    /* 缺陷 1: 密码含单引号, 原实现直接嵌入 value='...' 会截断属性 */
    html_escape("pa'ss", buf, sizeof(buf));
    CHECK_STR(buf, "pa&#39;ss");

    html_escape("a\"b", buf, sizeof(buf));
    CHECK_STR(buf, "a&quot;b");

    html_escape("a<b>c&d", buf, sizeof(buf));
    CHECK_STR(buf, "a&lt;b&gt;c&amp;d");

    /* 转义后不会再出现裸的单引号 */
    html_escape("it's a 'test'", buf, sizeof(buf));
    CHECK(strchr(buf, '\'') == NULL);

    /* 往返: 转义 & 号不会再被二次解析成实体 */
    html_escape("&amp;", buf, sizeof(buf));
    CHECK_STR(buf, "&amp;amp;");

    /* 空串 / NULL */
    html_escape("", buf, sizeof(buf));
    CHECK_STR(buf, "");
    CHECK(html_escape(NULL, buf, sizeof(buf)) == 0);

    /* 仅计算长度: out 为 NULL 时应返回所需字节数 */
    CHECK(html_escape("a<b", NULL, 0) == strlen("a&lt;b"));
    CHECK(html_escape("hello", NULL, 0) == 5);

    /* 缓冲区不足: 返回值 >= out_sz 表示截断, 且始终以 \0 结尾 */
    size_t need = html_escape("a<b", buf, 4);
    CHECK(need == strlen("a&lt;b"));
    CHECK(need >= 4);
    CHECK(strlen(buf) == 3); /* 已截断到 3 字符 */
}

/* ============ url_decode ============ */

static void test_url_decode(void)
{
    char buf[256];

    printf("url_decode\n");

    url_decode("abc", buf, sizeof(buf));
    CHECK_STR(buf, "abc");

    /* 加号转空格 */
    url_decode("my+wifi", buf, sizeof(buf));
    CHECK_STR(buf, "my wifi");

    /* %XX 十六进制, 大小写不敏感 */
    url_decode("%41%42%43", buf, sizeof(buf));
    CHECK_STR(buf, "ABC");
    url_decode("%4a%4b", buf, sizeof(buf));
    CHECK_STR(buf, "JK");

    /* UTF-8 中文 SSID。注意: 1.3.1 的 strtol 解码在字节层面是正确的
       (0xE5 存入 char 虽为负数, 但内存字节不变), 此用例是回归保护。 */
    url_decode("%E5%AF%9D%E5%AE%A4", buf, sizeof(buf));
    CHECK_STR(buf, "寝室");
    CHECK(strlen(buf) == 6); /* 2 个汉字 = 6 字节 UTF-8 */

    /* 尾部孤立的 % 或半截 %XX 不得越界读取
       (1.3.1 的 %XX 分支有 val[i+2] 检查, 但截断到 512 字节后
        若 % 落在边界上就会越界, 见缺陷 3) */
    url_decode("abc%", buf, sizeof(buf));
    CHECK_STR(buf, "abc%");
    url_decode("abc%4", buf, sizeof(buf));
    CHECK_STR(buf, "abc%4");
    url_decode("abc%ZZ", buf, sizeof(buf));
    CHECK_STR(buf, "abc%ZZ");

    /* 空输入 */
    url_decode("", buf, sizeof(buf));
    CHECK_STR(buf, "");
    CHECK(url_decode(NULL, buf, sizeof(buf)) == 0);

    /* 仅计算长度 */
    CHECK(url_decode("a+b%20c", NULL, 0) == strlen("a b c"));
}

/* ============ form_parse ============ */

static void test_form_parse(void)
{
    app_config_t cfg;
    char body[8192];

    printf("form_parse\n");

    /* 基本解析 */
    strcpy(body, "wifi_ssid=MyWifi&wifi_password=secret123&"
                 "campus_username=2023001&campus_password=pwd&channel=pc");
    CHECK(form_parse(body, &cfg) == true);
    CHECK_STR(cfg.wifi_ssid, "MyWifi");
    CHECK_STR(cfg.wifi_password, "secret123");
    CHECK_STR(cfg.campus_username, "2023001");
    CHECK_STR(cfg.campus_password, "pwd");
    CHECK_STR(cfg.channel, "pc");

    /* 开放网络: 密码留空是合法的 */
    strcpy(body, "wifi_ssid=OpenNet&wifi_password=&"
                 "campus_username=u&campus_password=p&channel=phone");
    CHECK(form_parse(body, &cfg) == true);
    CHECK_STR(cfg.wifi_password, "");

    /* 必填校验: 缺 SSID */
    strcpy(body, "wifi_ssid=&campus_username=u&campus_password=p");
    CHECK(form_parse(body, &cfg) == false);

    /* 必填校验: 缺账号 */
    strcpy(body, "wifi_ssid=S&campus_username=&campus_password=p");
    CHECK(form_parse(body, &cfg) == false);

    /* 未提交 channel 时回落到 phone */
    strcpy(body, "wifi_ssid=S&campus_username=u&campus_password=p");
    CHECK(form_parse(body, &cfg) == true);
    CHECK_STR(cfg.channel, "phone");

    /* 缺陷 1+2 组合: 密码含单引号 + 空格 + 中文, 这是用户真实报错场景 */
    strcpy(body, "wifi_ssid=%E5%AF%9D%E5%AE%A4&wifi_password=my+pa%27ss%22&"
                 "campus_username=2023001%40edu&campus_password=a%26b&channel=phone");
    CHECK(form_parse(body, &cfg) == true);
    CHECK_STR(cfg.wifi_ssid, "寝室");
    CHECK_STR(cfg.wifi_password, "my pa'ss\"");
    CHECK_STR(cfg.campus_username, "2023001@edu");
    CHECK_STR(cfg.campus_password, "a&b");

    /* 字段顺序无关 */
    strcpy(body, "channel=pc&campus_password=p&campus_username=u&"
                 "wifi_password=w&wifi_ssid=S");
    CHECK(form_parse(body, &cfg) == true);
    CHECK_STR(cfg.wifi_ssid, "S");
    CHECK_STR(cfg.channel, "pc");

    /* 未知字段忽略, 不影响其余解析 */
    strcpy(body, "unknown=zzz&wifi_ssid=S&campus_username=u&another=x");
    CHECK(form_parse(body, &cfg) == true);
    CHECK_STR(cfg.wifi_ssid, "S");

    /* 无值字段 (无 =) 不得崩溃 */
    strcpy(body, "justkey&wifi_ssid=S&campus_username=u");
    CHECK(form_parse(body, &cfg) == true);
    CHECK_STR(cfg.wifi_ssid, "S");

    /* 缺陷 3: 超长表单体。原实现硬截断到 512 字节导致字段静默丢失 */
    {
        char long_pw[600];
        memset(long_pw, 'x', sizeof(long_pw) - 1);
        long_pw[sizeof(long_pw) - 1] = '\0';
        snprintf(body, sizeof(body),
                 "wifi_ssid=LongSSID&wifi_password=%s&"
                 "campus_username=u&campus_password=p&channel=pc", long_pw);
        CHECK(form_parse(body, &cfg) == true);
        CHECK_STR(cfg.wifi_ssid, "LongSSID");
        /* 密码被截断到 63 字节 (缓冲区 64 含 \0) */
        CHECK(strlen(cfg.wifi_password) == 63);
        CHECK(strlen(cfg.wifi_password) < sizeof(cfg.wifi_password));
        CHECK(cfg.wifi_password[sizeof(cfg.wifi_password) - 1] == '\0');
        /* 关键: 截断不得影响后续字段 */
        CHECK_STR(cfg.campus_username, "u");
        CHECK_STR(cfg.channel, "pc");
    }

    /* 超长值不得溢出相邻字段 (ASan 会捕获越界写) */
    {
        char huge[4096];
        memset(huge, 'A', sizeof(huge) - 1);
        huge[sizeof(huge) - 1] = '\0';
        int n = snprintf(body, sizeof(body),
                         "wifi_ssid=%s&campus_username=u&campus_password=p",
                         huge);
        CHECK(n > 0 && (size_t)n < sizeof(body));
        CHECK(form_parse(body, &cfg) == true);
        CHECK(strlen(cfg.wifi_ssid) == 31);
        CHECK_STR(cfg.campus_username, "u");
        CHECK_STR(cfg.campus_password, "p");
    }

    /* 空表单体 */
    body[0] = '\0';
    CHECK(form_parse(body, &cfg) == false);
    CHECK(form_parse(NULL, &cfg) == false);

    /* 残留状态: 上一次的解析结果不得泄漏到下一次 */
    strcpy(body, "wifi_ssid=S&campus_username=u&campus_password=p&channel=pc");
    CHECK(form_parse(body, &cfg) == true);
    CHECK_STR(cfg.channel, "pc");
    strcpy(body, "wifi_ssid=S2&campus_username=u2&campus_password=p2");
    CHECK(form_parse(body, &cfg) == true);
    CHECK_STR(cfg.wifi_ssid, "S2");
    CHECK_STR(cfg.channel, "phone"); /* 未被上次的 pc 污染 */
    CHECK_STR(cfg.wifi_password, "");
}

/* ============ 往返一致性 ============ */

static void test_roundtrip(void)
{
    printf("roundtrip (保存 -> 重新渲染 -> 再次提交 应保持不变)\n");

    /* 一组"难搞"的真实输入法字符 */
    const char* tricky[] = {
        "abc",
        "my pa'ss",
        "p\"a<s>&s",
        "寝室WiFi",
        "2023001@edu.cn",
        "a+b%20c",
        "密码'\"&<>",
    };

    char html[512];
    char body[2048];

    for (size_t i = 0; i < sizeof(tricky) / sizeof(tricky[0]); i++) {
        const char* original = tricky[i];

        /* 服务端渲染 value='<escaped>' */
        char escaped[256];
        html_escape(original, escaped, sizeof(escaped));
        snprintf(html, sizeof(html), "value='%s'", escaped);

        /* 浏览器提交时把整串做 urlencode */
        char encoded[1024];
        size_t e = 0;
        for (size_t j = 0; original[j] && e < sizeof(encoded) - 4; j++) {
            unsigned char c = (unsigned char)original[j];
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                c == '.' || c == '~') {
                encoded[e++] = (char)c;
            } else {
                e += (size_t)snprintf(encoded + e, sizeof(encoded) - e,
                                      "%%%02X", c);
            }
        }
        encoded[e] = '\0';

        snprintf(body, sizeof(body),
                 "wifi_ssid=%s&wifi_password=&campus_username=u&"
                 "campus_password=p&channel=phone", encoded);

        app_config_t cfg;
        CHECK(form_parse(body, &cfg) == true);
        CHECK_STR(cfg.wifi_ssid, original);
    }
}

int main(void)
{
    printf("=== config_codec 单元测试 ===\n\n");

    test_html_escape();
    printf("\n");
    test_url_decode();
    printf("\n");
    test_form_parse();
    printf("\n");
    test_roundtrip();

    printf("\n%s  通过 %d, 失败 %d\n",
           g_fail == 0 ? "[PASS]" : "[FAIL]", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
