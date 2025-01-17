#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "k_printf.h"

/* region [k_printf_buf_impl] */

/* `char []` 缓冲区
 *
 * 缓冲区中的字符串始终保持结尾处为 `\0`。
 * 若指定缓冲区内存段的长度不在 int 所能表达的正数范围内，则 `str_buf` 不会对外输出任何内容。
 */
struct str_buf {
    struct k_printf_buf impl;
    char *buffer;
    int str_len;
    int max_len;
};

/* `FILE *` 缓冲区
 */
struct file_buf {
    struct k_printf_buf impl;
    FILE *file;
};

static void init_str_buf   (struct str_buf *str_buf, char *buf, size_t capacity);
static void str_buf_puts   (struct k_printf_buf *buf, const char *str, size_t len);
static void str_buf_printf (struct k_printf_buf *buf, const char *fmt, ...);
static void str_buf_vprintf(struct k_printf_buf *buf, const char *fmt, va_list args);

static void init_file_buf   (struct file_buf *buf, FILE *file);
static void file_buf_puts   (struct k_printf_buf *buf, const char *str, size_t len);
static void file_buf_printf (struct k_printf_buf *buf, const char *fmt, ...);
static void file_buf_vprintf(struct k_printf_buf *buf, const char *fmt, va_list args);

static void init_str_buf(struct str_buf *str_buf, char *buf, size_t capacity) {

    static char buf_[1] = { '\0' };

    str_buf->impl.fn_puts    = str_buf_puts,
    str_buf->impl.fn_printf  = str_buf_printf,
    str_buf->impl.fn_vprintf = str_buf_vprintf,
    str_buf->impl.n          = 0;

    if (0 < capacity && capacity <= INT_MAX) {
        str_buf->buffer  = buf;
        str_buf->str_len = 0;
        str_buf->max_len = (int)capacity - 1;
    } else {
        str_buf->buffer  = buf_;
        str_buf->str_len = 0;
        str_buf->max_len = 0;
    }

    str_buf->buffer[0] = '\0';
}

static void str_buf_puts(struct k_printf_buf *buf, const char *str, size_t len) {
    if (-1 == buf->n)
        return;

    struct str_buf *str_buf = (struct str_buf *)buf;

    int remain_capacity = str_buf->max_len - str_buf->str_len;

    if (len <= remain_capacity) {
        memcpy(&str_buf->buffer[str_buf->str_len], str, len * sizeof(char));
        str_buf->str_len += (int)len;
        str_buf->buffer[str_buf->str_len] = '\0';

        buf->n += (int)len;
        return;
    }

    if (0 < remain_capacity) {
        memcpy(&str_buf->buffer[str_buf->str_len], str, remain_capacity * sizeof(char));
        str_buf->str_len = str_buf->max_len;
    }

    str_buf->buffer[str_buf->str_len] = '\0';

    if (len <= INT_MAX) {
        buf->n += (int)len;
        if (buf->n < 0)
            buf->n = -1;
    } else {
        buf->n = -1;
    }
}

static void str_buf_printf(struct k_printf_buf *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str_buf_vprintf(buf, fmt, args);
    va_end(args);
}

static void str_buf_vprintf(struct k_printf_buf *buf, const char *fmt, va_list args) {
    if (-1 == buf->n)
        return;

    struct str_buf *str_buf = (struct str_buf *)buf;

    int remain_len = str_buf->max_len - str_buf->str_len;
    int r = vsnprintf(&str_buf->buffer[str_buf->str_len], remain_len + 1, fmt, args);
    if (r < 0) {
        buf->n = -1;
        return;
    }

    if (r <= remain_len) {
        str_buf->str_len += r;
        buf->n += r;
    }
    else {
        str_buf->str_len = str_buf->max_len;
        buf->n += r;
        if (buf->n < 0)
            buf->n = -1;
    }
}

static void init_file_buf(struct file_buf *buf, FILE *file) {

    buf->impl.fn_puts    = file_buf_puts,
    buf->impl.fn_printf  = file_buf_printf,
    buf->impl.fn_vprintf = file_buf_vprintf,
    buf->impl.n          = 0;
    buf->file            = file;
}

static void file_buf_puts(struct k_printf_buf *buf, const char *str, size_t len) {
    if (-1 == buf->n)
        return;

    struct file_buf *file_buf = (struct file_buf *)buf;

    size_t r = fwrite(str, sizeof(char), len, file_buf->file);
    if (INT_MAX < r) {
        buf->n = -1;
        return;
    }

    buf->n += (int)r;
    if (buf->n < 0)
        buf->n = -1;
}

static void file_buf_printf(struct k_printf_buf *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    file_buf_vprintf(buf, fmt, args);
    va_end(args);
}

static void file_buf_vprintf(struct k_printf_buf *buf, const char *fmt, va_list args) {
    if (-1 == buf->n)
        return;

    struct file_buf *file_buf = (struct file_buf *)buf;

    int r = vfprintf(file_buf->file, fmt, args);
    if (r < 0) {
        buf->n = -1;
        return;
    }

    buf->n += r;
    if (buf->n < 0)
        buf->n = -1;
}

/* endregion */

/* region [c_std_spec] */

/* 处理 C `printf` 格式说明符的回调
 *
 * 此函数专门处理 `%n` 一族的格式说明符。
 * 函数假定传入的格式说明符类型是正确的。
 */
static void printf_callback_c_std_spec_n(struct k_printf_buf *buf, const struct k_printf_spec *spec, va_list *args) {

    const char c1 = spec->type[0];
    const char c2 = spec->type[1];

    int n = buf->n;

    if (c1=='n') {
        *(va_arg(*args, int *)) = (int)n; /* %n */
    } else if (c1=='j') {
        *(va_arg(*args, intmax_t *)) = (intmax_t)n; /* %jn */
    } else if (c1=='t') {
        *(va_arg(*args, ptrdiff_t *)) = (ptrdiff_t)n; /* %tn */
    } else if (c1=='z') {
        *(va_arg(*args, size_t *)) = (size_t)n; /* %zn */
    } else if (c1=='l') {
        if (c2=='l') {
            *(va_arg(*args, long long *)) = (long long)n; /* %lln */
        } else {
            *(va_arg(*args, long *)) = (long)n; /* %ln */
        }
    } else {
        if (c2 == 'h') {
            *(va_arg(*args, unsigned char *)) = (unsigned char)n; /* %hhn */
        } else {
            *(va_arg(*args, short *)) = (short)n; /* %hn */
        }
    }
}

/* 处理 C `printf` 格式说明符的回调
 *
 * 此函数处理 C `printf` 的格式说明符（除了 `%n` 一族）。
 * 函数假定传入的格式说明符类型是正确的。
 */
static void printf_callback_c_std_spec(struct k_printf_buf *buf, const struct k_printf_spec *spec, va_list *args) {

    /* 将格式说明符交回给 C `printf` 处理，之后按需消耗掉变长参数列表的实参 */

    char fmt_buf[80];
    char *fmt = fmt_buf;

    int len = (int)(spec->end - spec->start);
    if (sizeof(fmt_buf) < len + 1) {
        if (NULL == (fmt = malloc(len + 1))) {
            buf->n = -1;
            return;
        }
    }

    memcpy(fmt, spec->start, len);
    fmt[len] = '\0';

    va_list args_copy;
    va_copy(args_copy, *args);
    buf->fn_vprintf(buf, fmt, args_copy);
    va_end(args_copy);

    if (fmt != fmt_buf)
        free(fmt);

    if (spec->use_min_width && -1 == spec->min_width)
        va_arg(*args, int);
    if (spec->use_precision && -1 == spec->precision)
        va_arg(*args, int);

    switch(spec->type[0]) {
        case 'a': case 'A': case 'e': case 'E':
        case 'f': case 'F': case 'g': case 'G':
            va_arg(*args, double);
            break;
        case 'c': case 'd': case 'i': case 'h':
            va_arg(*args, int);
            break;
        case 'o': case 'u': case 'x': case 'X':
            va_arg(*args, unsigned int);
            break;
        case 'j':
            switch (spec->type[1]) {
                case 'd': case 'i':
                    va_arg(*args, intmax_t);
                    break;
                case 'x': case 'X':
                case 'o': case 'u':
                    va_arg(*args, uintmax_t);
                    break;
            }
            break;
        case 'l':
            switch (spec->type[1]) {
                case 'a': case 'A': case 'e': case 'E':
                case 'f': case 'F': case 'g': case 'G':
                    va_arg(*args, double);
                    break;
                case 'c':
                    va_arg(*args, int);
                    break;
                case 'd': case 'i':
                    va_arg(*args, long);
                    break;
                case 'o': case 'u': case 'x': case 'X':
                    va_arg(*args, unsigned long);
                    break;
                case 's':
                    va_arg(*args, wchar_t *);
                    break;
                case 'l':
                    switch (spec->type[2]) {
                        case 'd': case 'i':
                            va_arg(*args, long long);
                            break;
                        case 'o': case 'u':
                        case 'x': case 'X':
                            va_arg(*args, unsigned long long);
                            break;
                    }
                    break;
            }
            break;
        case 'L': va_arg(*args, long double); break;
        case 'p': va_arg(*args, void *);      break;
        case 's': va_arg(*args, char *);      break;
        case 't': va_arg(*args, ptrdiff_t);   break;
        case 'z': va_arg(*args, size_t);      break;
    }
}

/* 匹配 C `printf` 格式说明符，若匹配成功则移动字符串指针，并返回对应的回调
 *
 * C `printf` 支持的格式说明符详见：https://zh.cppreference.com/w/c/io/fprintf
 */
static k_printf_callback_fn match_c_std_spec(const char **str) {

    /* 通过打表的方式，给每个 C `printf` 格式说明符分配回调 */

    switch ((*str)[0]) {
        case 'a': case 'A': case 'c': case 'd':
        case 'e': case 'E': case 'f': case 'F':
        case 'g': case 'G': case 'i': case 'o':
        case 'p': case 's': case 'u': case 'x':
        case 'X':
            *str += 1;
            return printf_callback_c_std_spec;

        case 'n':
            *str += 1;
            return printf_callback_c_std_spec_n;

        case 'h': {
            switch ((*str)[1]) {
                case 'd': case 'i':
                case 'o': case 'u':
                case 'x': case 'X':
                    *str += 2;
                    return printf_callback_c_std_spec;
                case 'n':
                    *str += 2;
                    return printf_callback_c_std_spec_n;
                case 'h':
                    switch ((*str)[2]) {
                        case 'd': case 'i':
                        case 'o': case 'u':
                        case 'x': case 'X':
                            *str += 3;
                            return printf_callback_c_std_spec;
                        case 'n':
                            *str += 3;
                            return printf_callback_c_std_spec_n;
                    }
                    break;
            }
            break;
        }

        case 'l': {
            switch ((*str)[1]) {
                case 'a': case 'A': case 'c': case 'd':
                case 'e': case 'E': case 'f': case 'F':
                case 'g': case 'G': case 'i': case 'o':
                case 's': case 'u': case 'x': case 'X':
                    *str += 2;
                    return printf_callback_c_std_spec;
                case 'n':
                    *str += 2;
                    return printf_callback_c_std_spec_n;
                case 'l':
                    switch ((*str)[2]) {
                        case 'd': case 'i':
                        case 'o': case 'u':
                        case 'x': case 'X':
                            *str += 3;
                            return printf_callback_c_std_spec;
                        case 'n':
                            *str += 3;
                            return printf_callback_c_std_spec_n;
                    }
                    break;
            }
            break;
        }
        case 'L':
            switch ((*str)[1]) {
                case 'a': case 'A': case 'e': case 'E':
                case 'f': case 'F': case 'g': case 'G':
                    *str += 2;
                    return printf_callback_c_std_spec;
            }
            break;

        case 'j': case 't': case 'z':
            switch ((*str)[1]) {
                case 'd': case 'i':
                case 'o': case 'u':
                case 'x': case 'X':
                    *str += 2;
                    return printf_callback_c_std_spec;
                case 'n':
                    *str += 2;
                    return printf_callback_c_std_spec_n;
            }
            break;
    }

    return NULL;
}

/* endregion */

/* region [user_spec] */

k_printf_callback_fn k_printf_match_spec_helper(const struct k_printf_spec_callback_tuple *tuples, const char **str) {

    const struct k_printf_spec_callback_tuple *spec = tuples;
    for (; NULL != spec->spec_type; ++spec) {

        const char *p_str  = *str;
        const char *p_spec = spec->spec_type;
        for (; '\0' != *p_spec; ++p_str, ++p_spec) {
            if (*p_str != *p_spec)
                goto next_spec;
        }

        *str += p_spec - spec->spec_type;
        return spec->fn_callback;

    next_spec:;
    }

    return NULL;
}

/* endregion */

/* region [x_printf] */

/* 提取字符串开头的 int 值
 *
 * 函数假定字符串开头存在非负的数值。
 * 函数提取该数值成 int 并返回，并移动字符串指针跳过数字（若超过上限则返回 INT_MAX）。
 */
static int extract_non_negative_int(const char **str) {

    unsigned long long num = 0;

    const char *ch = *str;
    for (; '0' <= *ch && *ch <= '9'; ch++) {
        num = num * 10 + (*ch - '0');

        if (INT_MAX <= num) {
            while ('0' <= *ch && *ch <= '9')
                ch++;

            num = INT_MAX;
            break;
        }
    }

    *str = ch;
    return (int)num;
}

/* 提取格式说明符，若提取成功则移动字符串指针，并返回对应的回调
 *
 * 函数假定字符串的起始为 `%` 符号。
 */
static k_printf_callback_fn extract_spec(const struct k_printf_config *config, const char **str, struct k_printf_spec *get_spec) {

    const char *ch = *str + 1;

    struct k_printf_spec spec;
    spec.start = *str;

    spec.left_justified   = 0;
    spec.sign_prepended   = 0;
    spec.space_padded     = 0;
    spec.zero_padding     = 0;
    spec.alternative_form = 0;
    for (;;) {
        switch (*ch) {
            case '-': ch++; spec.left_justified   = ~0; continue;
            case '+': ch++; spec.sign_prepended   = ~0; continue;
            case ' ': ch++; spec.space_padded     = ~0; continue;
            case '0': ch++; spec.zero_padding     = ~0; continue;
            case '#': ch++; spec.alternative_form = ~0; continue;
        }
        break;
    }

    if ('1' <= *ch && *ch <= '9') {
        spec.use_min_width = ~0;
        spec.min_width     = extract_non_negative_int(&ch);
    } else if ('*' == *ch) {
        ch++;
        spec.use_min_width = ~0;
        spec.min_width     = -1;
    } else {
        spec.use_min_width = 0;
        spec.min_width     = -1;
    }

    if ('.' == *ch) {
        ch++;
        if ('0' <= *ch && *ch <= '9') {
            spec.use_precision = ~0;
            spec.precision     = extract_non_negative_int(&ch);
        } else if ('*' == *ch) {
            ch++;
            spec.use_precision = ~0;
            spec.precision     = -1;
        } else {
            return NULL;
        }
    } else {
        spec.use_precision = 0;
        spec.precision     = -1;
    }

    spec.type = ch;

    k_printf_callback_fn fn_callback;
    if (NULL == (fn_callback = config->fn_match_spec(&ch)) &&
        NULL == (fn_callback = match_c_std_spec(&ch)))
        return NULL;

    spec.end = ch;

    *str = ch;
    *get_spec = spec;
    return fn_callback;
}

/* 将格式化字符串写入到缓冲区，并返回格式化后的字符串长度
 *
 * 本函数为 `k_printf` 模块所有函数的核心实现。
 */
static int x_printf(const struct k_printf_config *config, struct k_printf_buf *buf, const char *fmt, va_list args) {

    const char *s = fmt;
    const char *p = s;
    for (;;) {
        while ('\0' != *p && '%' != *p)
            ++p;

        if (s < p)
            buf->fn_puts(buf, s, p - s);

        if ('\0' == *p)
            break;

        if ('%' == *(p + 1)) {
            s = p + 1;
            p = p + 2;
            continue;
        }

        s = p;

        struct k_printf_spec spec;
        k_printf_callback_fn fn_callback = extract_spec(config, &s, &spec);
        if (NULL != fn_callback) {
            fn_callback(buf, &spec, &args);
            p = s;
        } else {
            p = s + 1;
        }
    }

    return buf->n;
}

/* endregion */

/* region [k_printf] */

int k_printf(const struct k_printf_config *config, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = k_vfprintf(config, stdout, fmt, args);
    va_end(args);

    return r;
}

int k_fprintf(const struct k_printf_config *config, FILE *file, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = k_vfprintf(config, file, fmt, args);
    va_end(args);

    return r;
}

int k_vfprintf(const struct k_printf_config *config, FILE *file, const char *fmt, va_list args) {
    assert(NULL != file);
    assert(NULL != fmt);

    if (NULL == config)
        return vfprintf(file, fmt, args);

    struct file_buf file_buf;
    init_file_buf(&file_buf, file);

    return x_printf(config, (struct k_printf_buf *)&file_buf, fmt, args);
}

int k_sprintf(const struct k_printf_config *config, char *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = k_vsprintf(config, buf, fmt, args);
    va_end(args);

    return r;
}

int k_vsprintf(const struct k_printf_config *config, char *buf, const char *fmt, va_list args) {
    return k_vsnprintf(config, buf, INT_MAX, fmt, args);
}

int k_snprintf(const struct k_printf_config *config, char *buf, size_t n, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = k_vsnprintf(config, buf, n, fmt, args);
    va_end(args);

    return r;
}

int k_vsnprintf(const struct k_printf_config *config, char *buf, size_t n, const char *fmt, va_list args) {
    assert(NULL != fmt);

    if (NULL == config)
        return vsnprintf(buf, n, fmt, args);

    struct str_buf str_buf;
    init_str_buf(&str_buf, buf, n);

    return x_printf(config, (struct k_printf_buf *)&str_buf, fmt, args);
}

int k_asprintf(const struct k_printf_config *config, char **get_s, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int r = k_vasprintf(config, get_s, fmt, args);
    va_end(args);

    return r;
}

int k_vasprintf(const struct k_printf_config *config, char **get_s, const char *fmt, va_list args) {
    assert(NULL != get_s);
    assert(NULL != fmt);

    va_list args_copy;
    va_copy(args_copy, args);
    int str_len = k_vsnprintf(config, NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (str_len <= 0 || str_len == INT_MAX)
        return -1;

    char *buf = malloc(str_len + 1);
    if (NULL == buf)
        return -1;

    if (str_len != k_vsnprintf(config, buf, str_len + 1, fmt, args)) {
        free(buf);
        return -1;
    }

    *get_s = buf;
    return str_len;
}

/* endregion */
