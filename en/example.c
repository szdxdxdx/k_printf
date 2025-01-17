#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <limits.h>
#include <string.h>

#include "k_printf.h"

/* Example demonstrating how to customize the `k_printf` format specifier `%arr` to print an array of integers.
 *
 * In C, arrays are often passed with their length, so when `k_printf` encounters `%arr`,
 * it should read two arguments: the pointer to the array and the length of the array.
 * It is assumed that the array length does not exceed the maximum value for an `int`.
 *
 * The `%arr` specifier supports the `*` modifier, which specifies the minimum width for each array element when printed.
 * For example: `%5arr` means that each number will be printed with a minimum width of 5 characters.
 *
 * The `%arr` specifier also supports the `.*` modifier, which specifies the number of elements per line when printing the array.
 * For example: `%.3arr` means that the array content will be printed with 3 elements per line.
 */
static void printf_callback_spec_arr(struct k_printf_buf *buf, const struct k_printf_spec *spec, va_list *args) {

    /* Step 1: Consume the arguments from the variable argument list as needed. */

    int min_width = 0;
    if (spec->use_min_width) {

        if (spec->min_width < 0) {
            min_width = va_arg(*args, int);
            if (min_width < 0)
                min_width = 0;
        }
        else
            min_width = spec->min_width;
    }

    int line_len = INT_MAX;
    if (spec->use_precision) {

        if (spec->precision < 0) {
            line_len = va_arg(*args, int);
            if (line_len <= 0)
                line_len = INT_MAX;
        }
        else
            line_len = spec->precision;
    }

    int *arr = va_arg(*args, int *);
    int  len = va_arg(*args, int);

    /* Step 2: Output content to the buffer. */

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

/* This example demonstrates how to overload the `k_printf` format specifier `%c`.
 *
 * By default, `k_printf`'s `%c` works like C `printf`, used to print a single character.
 *
 * In this example, we will overload `%c` to still print a single character.
 *
 * Overloading the default format specifier will lose all the default behaviors of the modifiers,
 * such as left/right alignment, zero padding, precision, etc.
 * If needed, you must implement these behaviors yourself.
 *
 * Here, we redefine the minimum width modifier `*`, changing it to repeat the output character multiple times.
 * For example: `%5c` means repeating the character 5 times.
 */
static void printf_callback_spec_c(struct k_printf_buf *buf, const struct k_printf_spec *spec, va_list *args) {

    /* Step 1: Consume the arguments from the variable argument list as needed. */

    int repeat = 1;
    if (spec->use_min_width) {

        if (spec->min_width < 1) {
            repeat = va_arg(*args, int);
            if (repeat < 0)
                repeat = 1;
        }
        else
            repeat = spec->min_width;
    }

    char ch = (char)va_arg(*args, int);

    /* Step 2: Output content to the buffer. */

    if (repeat == 1) {
        buf->fn_puts(buf, &ch, 1);
        return;
    }

    char tmp_buf[96];

    if (repeat <= sizeof(tmp_buf)) {
        memset(tmp_buf, ch, repeat);
        buf->fn_puts(buf, tmp_buf, repeat);
        return;
    }

    memset(tmp_buf, ch, sizeof(tmp_buf));

    int putc_num = 0;
    while (putc_num + sizeof(tmp_buf) <= repeat) {
        buf->fn_puts(buf, tmp_buf, sizeof(tmp_buf));
        putc_num += sizeof(tmp_buf);
    }
    if (putc_num < repeat)
        buf->fn_puts(buf, tmp_buf, repeat - putc_num);
}

/* This example demonstrates how to match custom format specifiers:
 * If you have many format specifiers, you can manually hash
 * using if-else or switch-case to improve matching efficiency.
 */
static k_printf_callback_fn match_my_spec_1(const char **str) {

    const char *s = *str;

    if (s[0] == 'c') {
         /* If matched, move the string pointer and return the corresponding callback */
        *str += 1;
        return printf_callback_spec_c;
    } else if (s[0] == 'a' && s[1] == 'r' && s[2] == 'r') {
        *str += 3;
        return printf_callback_spec_arr;
    } else {
        /* If matching fails, return NULL and do not move the string pointer */
        return NULL;
    }
}

/* This example demonstrates how to match custom format specifiers:
 * If you have fewer format specifiers, or you want a simpler solution,
 * you can use `k_printf_match_spec_helper`.
 */
static k_printf_callback_fn match_my_spec_2(const char **str) {

    static struct k_printf_spec_callback_tuple tuples[] = {
        { "arr", printf_callback_spec_arr },
        { "c"  , printf_callback_spec_c   },
        { NULL , NULL }
    };

    return k_printf_match_spec_helper(tuples, str);
}

static struct k_printf_config config = {
    .fn_match_spec = match_my_spec_1, /* <- Or you can use `match_my_spec_2` */
};

/* ------------------------------------------------------------------------ */

static void example_1(void) {

    int arr[] = {  1,  2,  3,  4,  5,
                   6,  7,  8,  9, 10,
                  11, 12, 13, 14, 15,
                  16, 17, 18, 19, 20 };

    /* Using `k_printf` */
    {
        /* This output uses the default configuration,
         * which only supports C `printf` format specifiers.
         * `%a` prints a floating-point number in hexadecimal scientific notation.
         * `%4c` prints a character but occupies 4 character widths.
         */
        k_printf(NULL, "%arr, %d, %4c\n\n", 0xcp-1076, 5, 'b');

        int n;

        /* This output uses the specified configuration,
         * with the `%c` specifier overloaded to repeat the character.
         * It still supports C `printf` format specifiers.
         */
        k_printf(&config, "%s, %c,%n %4c, %*c\n\n", "hello", 'a', &n, 'b', 3, 'c');
        k_printf(&config, "%s, %d, %5.2f, %5lld\n\n", "hello", n, 3.14, (long long)123);
    }

    /* Using `k_fprintf` */
    {
        /* Using the custom format specifier `%arr`, print the first 8 elements of the array. */
        k_fprintf(&config, stdout, "%arr\n\n", arr, 8);
    }

    /* Using `k_asprintf` */
    {
        char *get_s;

        /* Print the first 20 elements of the array, with 7 elements printed per line. */
        k_asprintf(&config, &get_s, "%.7arr\n", arr, 20);
        if (NULL != get_s) {
            puts(get_s);
            free(get_s);
        }
    }

    /* Using `k_sprintf` */
    {
        char buf[96];

        /* Print the first 13 elements of the array, with 5 elements printed per line.
         * Each element should occupy at least 2 character widths.
         */
        k_sprintf(&config, buf, "%2.5arr\n", arr, 13);
        puts(buf);
    }

    /* Using `k_snprintf` */
    {
        char buf[96];

        /* Set the buffer size to 96; any content exceeding this will be truncated.
         * Print the first 20 elements of the array, with 5 elements printed per line.
         * Each element should occupy at least 3 character widths.
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
