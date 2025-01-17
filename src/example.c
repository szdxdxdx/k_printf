#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>

#include "k_printf.h"

/* 本示例教你如何自定义 `k_printf` 的格式说明符 `%arr`，用于打印整型的数组
 *
 * 在 C 中，传递数组往往要伴随传递数组长度，所以 `k_printf` 遇到 `%arr` 时应读入两个实参，
 * 分别整型数组的指针和数组长度。这里假定数组长度不超过 int 上限。
 *
 * `%arr` 支持 `*` 修饰，表示输出数组元素的最小宽度
 * 例如：`%5arr` 表示打印时每个数字的最小宽度为 5。
 *
 * `%arr` 支持 `.*` 修饰，表示将每行打印的元素个数。
 * 例如：`%.3arr` 表示将数组内容换行打印，每行 3 个元素。
 */
static void printf_callback_spec_arr(struct k_printf_buf *buf, const struct k_printf_spec *spec, va_list *args) {

    /* 第一步，按需消耗变长参数列表中的实参 */

    int min_width = 0;
    if (spec->use_min_width) {

        if (spec->min_width == -1) {
            min_width = va_arg(*args, int);
            if (min_width < 0)
                min_width = 0;
        }
        else
            min_width = spec->min_width;
    }

    int line_len = INT_MAX;
    if (spec->use_precision) {

        if (spec->precision == -1) {
            line_len = va_arg(*args, int);
            if (line_len <= 0)
                line_len = INT_MAX;
        }
        else
            line_len = spec->precision;
    }

    int *arr = va_arg(*args, int *);
    int  len = va_arg(*args, int);

    /* 第二步，向缓冲区输出内容 */

    if (len == 0) {
        buf->fn_puts(buf, "[]", 2);
        return;
    }

    if (len == 1) {
        buf->fn_printf(buf, "[ %*d ]", min_width, arr[0]);
        return;
    }

    int i = 0;
    buf->fn_printf(buf, "[ %*d,", min_width, arr[i]);
    for (;;) {
        i++;
        if (i % line_len == 0)
            buf->fn_puts(buf, "\n ", 2);
        if (len - 1 == i)
            break;
        buf->fn_printf(buf, " %*d,", min_width, arr[i]);
    }
    buf->fn_printf(buf, " %*d ]", min_width, arr[i]);
}

/* 本示例教你如何重载 `k_printf` 的格式说明符 `%c`
 *
 * `k_printf` 默认的 `%c` 同 C `printf` 一样，用于打印一个字符。
 *
 * 本示例要重载 `%c`，仍用于打印一个字符。
 *
 * 重载默认格式说明符会失去所有修饰符的默认行为，不再支持左右对齐、零填充、精度等。
 * 若需要，你得自己实现它们。
 *
 * 这里重新定义最小宽度修饰 `*`，改为重复输出的字符个数。
 * 例如：`%5c` 表示重复输出 5 次该字符。
 */
static void printf_callback_spec_c(struct k_printf_buf *buf, const struct k_printf_spec *spec, va_list *args) {

    /* 第一步，按需消耗变长参数列表中的实参 */

    int repeat = 1;
    if (spec->use_min_width) {

        if (spec->min_width == -1) {
            repeat = va_arg(*args, int);
            if (repeat < 0)
                repeat = 1;
        }
        else
            repeat = spec->min_width;
    }

    char ch = (char)va_arg(*args, int);

    /* 第二步，向缓冲区输出内容 */

    if (repeat == 1) {
        buf->fn_puts(buf, &ch, 1);
        return;
    }

    char ch_buf[64];

    if (repeat <= sizeof(ch_buf)) {
        memset(ch_buf, ch, repeat);
        buf->fn_puts(buf, ch_buf, repeat);
        return;
    }

    memset(ch_buf, ch, sizeof(ch_buf));

    int putc_num = 0;
    while (putc_num + sizeof(ch_buf) <= repeat) {
        buf->fn_puts(buf, ch_buf, sizeof(ch_buf));
        putc_num += sizeof(ch_buf);
    }
    if (putc_num < repeat)
        buf->fn_puts(buf, ch_buf, repeat - putc_num);
}

/* 本示例教你如何匹配自定义格式说明符：
 * 如果你的格式说明符非常多，你可以通过 if-else 与 switch-case 手动哈希，提高识别效率。
 */
static k_printf_callback_fn match_my_spec_1(const char **str) {

    const char *s = *str;

    if (s[0] == 'c') {
        /* 若匹配成功，则移动字符串指针，并返回对应的回调 */
        *str += 1;
        return printf_callback_spec_c;
    } else if (s[0] == 'a' && s[1] == 'r' && s[2] == 'r') {
        *str += 3;
        return printf_callback_spec_arr;
    } else {
        /* 若匹配失败则返回 NULL，不要移动字符串指针 */
        return NULL;
    }
}

/* 本示例教你如何匹配自定义格式说明符：
 * 若你的格式说明符比较少，或你想图方便，你可以使用 `k_printf_match_spec_helper`。
 */
static k_printf_callback_fn match_my_spec_2(const char **str) {

    static struct k_printf_spec_callback_tuple tuples[] = {
        { "arr", printf_callback_spec_arr },
        { "c"  , printf_callback_spec_c   },
        { NULL , NULL }
    };

    return k_printf_match_spec_helper(tuples, str);
}

static void example_1(void) {

    static struct k_printf_config config = {
        .fn_match_spec = match_my_spec_1, /* 或者 match_my_spec_2 */
    };

    int arr[] = {  1,  2,  3,  4,  5,
                   6,  7,  8,  9, 10,
                  11, 12, 13, 14, 15,
                  16, 17, 18, 19, 20 };

    /* 使用 `k_printf` */
    {
        /* 本次输出使用默认配置，只支持 C `printf` 格式说明符
         *  %a` 以十六进制指数记法打印浮点数
         * `%4c` 打印一个字符，但是占 4 个字符宽度
         */
        k_printf(NULL, "%arr, %d, %4c\n\n", 0xcp-1076, 5, 'b');

        int n;

        /* 本次输出使用指定配置，重载 `%c` 用于重复打印字符
         * 依然支持 C `printf` 的格式说明符
         */
        k_printf(&config, "%s, %c,%n %4c, %*c\n\n", "hello", 'a', &n, 'b', 3, 'c');
        k_printf(&config, "%s, %d, %5.2f, %5lld\n\n", "hello", n, 3.14, (long long)123);
    }

    /* 使用 `k_fprintf` */
    {
        /* 使用自定义格式说明符 `%arr`，打印数组的前 8 个元素 */
        k_fprintf(&config, stdout, "%arr\n\n", arr, 8);
    }

    /* 使用 `k_asprintf` */
    {
        char *get_s;

        /* 打印数组的前 20 个元素，每行打印 7 个元素 */
        k_asprintf(&config, &get_s, "%.7arr\n", arr, 20);
        if (NULL != get_s) {
            puts(get_s);
            free(get_s);
        }
    }

    /* 使用 `k_sprintf` */
    {
        char buf[96];

        /* 打印数组的前 13 个元素，每行打印 5 个元素，每个元素最少占 2 个字符宽度 */
        k_sprintf(&config, buf, "%2.5arr\n", arr, 13);
        puts(buf);
    }

    /* 使用 `k_snprintf` */
    {
        char buf[96];

        /* 指定缓冲区大小为 96，超出部分会被截断。
         * 打印数组的前 20 个元素，每行打印 5 个元素，每个元素最少占 3 个字符宽度
         */
        k_snprintf(&config, buf, 96, "%*.*arr\n", 3, 5, arr, 20);
        puts(buf);
        puts("");
    }
}

#if 1

int main(int argc, char **argv) {

    example_1();
}

#endif
