#ifndef K_PRINTF_H
#define K_PRINTF_H

#include <stdio.h>
#include <stddef.h>

struct k_printf_config;

/**
 * \brief Outputs a formatted string to `stdout` and returns its length.
 *
 * \ingroup k_printf
 *
 * \param config Output configuration (NULL for default).
 * \param fmt    Format string (C `printf` compatible).
 * \param ...    Arguments matching the format string.
 * \return Length of the formatted string on success, negative value on failure.
 */
int k_printf(const struct k_printf_config *config, const char *fmt, ...);

struct k_printf_buf;
struct k_printf_spec;

/**
 * \brief Callback function for custom format specifiers.
 *
 * In the callback, use `k_printf_buf->fn_tbl` functions to write content to the buffer.
 * The buffer type (either `char []` or `FILE *`) is abstracted.
 *
 * `spec` provides details on the current format specifier.
 * It includes information on modifications like alignment, padding, and precision.
 * Implement or override the behavior for these modifiers as needed.
 * If not required, you can ignore the `spec` parameter.
 *
 * Carefully consume the variable arguments to avoid errors in subsequent format specifiers.
 * Use `va_arg(*args, type)` to retrieve arguments,
 * keeping in mind type promotions (e.g., `char` to `int`).
 *
 * \param buf  Buffer to write content, use functions from `k_printf_buf->fn_tbl`.
 * \param spec Details of the current format specifier.
 * \param args Pointer to the variable argument list, consume arguments as needed.
 */
typedef void (*k_printf_callback_fn)(struct k_printf_buf *buf, const struct k_printf_spec *spec, va_list *args);

/** \brief Functions for manipulating the buffer */
struct k_printf_buf_fn_tbl {

    /** \brief Writes a string of specified length to the buffer */
    void (* const fn_puts)(struct k_printf_buf *buf, const char *str, size_t len);

    /** \brief Writes a formatted string to the buffer (C `printf` format specifiers). */
    void (* const fn_printf)(struct k_printf_buf *buf, const char *fmt, ...);

    /** \brief Writes a formatted string to the buffer (C `printf` format specifiers). */
    void (* const fn_vprintf)(struct k_printf_buf *buf, const char *fmt, va_list args);
};

/** \brief Unified interface for buffer operations, supporting both `char []` and `FILE *` types */
struct k_printf_buf {

    /** \brief Functions for manipulating the buffer */
    struct k_printf_buf_fn_tbl *fn_tbl;

    /**
     * \brief Number of characters printed so far (ignores the actual buffer size).
     *
     * Similar to C `printf` functions, which return the formatted string length,
     * regardless of whether the buffer can fully accommodate the string.
     * For example, `snprintf` can be used with a 0-sized buffer to return
     * the length of the formatted string without actually writing anything.
     *
     * `k_printf` functions behave similarly.
     *
     * `n` records the number of characters printed so far (ignoring the actual buffer size).
     * This value is automatically updated as content is written to the buffer.
     * The return value of `k_printf` functions is the final value of `n`.
     *
     * To implement functionality similar to `printf`'s `%n`, read the value of `n` in your callback.
     * A negative `n` indicates an error during output.
     */
    int n;
};

/** \brief Format specifier */
struct k_printf_spec {

    /** \brief `-` Left-justified */
    int left_justified : 1;

    /** \brief `+` Always show sign */
    int sign_prepended : 1;

    /** \brief ` ` Space-padded */
    int space_padded : 1;

    /** \brief `0` Zero-padded */
    int zero_padding : 1;

    /** \brief `#` Alternative format */
    int alternative_form : 1;

    /** \brief `*` Use minimum width */
    int use_min_width : 1;

    /** \brief `.*` Use precision */
    int use_precision : 1;

    /**
     * \brief Minimum width
     *
     * `min_width` is meaningful only when `use_min_width` is non-zero.
     *
     * If `%k` is a custom format specifier:
     *
     * For a static minimum width, `min_width` holds the width value,
     * which is always positive and no larger than INT_MAX.
     * For example:
     *  - `%2k` sets `min_width` to 2.
     *  - `%02k` sets `min_width` to 2 (with zero padding).
     *  - `%-2k` sets `min_width` to 2 (with left alignment).
     *  - `%999999999999k` sets `min_width` to INT_MAX.
     *
     * For a dynamic minimum width, `min_width` is set to -1.
     * For example, `%*k` has `use_min_width` non-zero and `min_width` is -1.
     * In this case, use `va_arg(*args, int)` to retrieve the dynamic width.
     */
    int min_width;

    /**
     * \brief Precision
     *
     * `precision` is meaningful only when `use_precision` is non-zero.
     *
     * If `%k` is a custom format specifier:
     *
     * For a static precision, `precision` holds the value,
     * which is always positive and no larger than INT_MAX.
     * For example:
     *  - `%.2k` sets `precision` to 2.
     *  - `%.k` sets `precision` to 0.
     *  - `%.999999999999k` sets `precision` to INT_MAX.
     *  - `%.-2k` cannot correctly recognize the `%k` format specifier.
     *
     * For a dynamic precision, `precision` is set to -1.
     * For example, `%.*k` has `use_precision` non-zero and `precision` is -1.
     * In this case, use `va_arg(*args, int)` to retrieve the dynamic precision.
     */
    int precision;

    /** \brief Points to the `%` symbol in the original format string */
    const char *start;

    /** \brief Points to the start of the type part of the format specifier in the original format string */
    const char *type;

    /** \brief Points to the end of the format specifier in the original format string */
    const char *end;
};

/**
 * \brief Configuration for custom format specifiers
 *
 * The format for C `printf` format specifiers is
 * `%[flags][width][.precision][length_modifier]conversion_specifier`.
 *
 * While C `printf` may consider `%lld` as a `%d` with `ll` modifier,
 * `k_printf` treats the `[length_modifier]conversion_specifier` as a whole
 * and calls it the specifier's `type`.
 * `k_printf` sees `%lld` and `%d` as different types.
 *
 * If you want to define your custom format specifier `%k`, its type name is `k`.
 * If `%llk` is not defined, it remains an unknown format specifier.
 * The custom specifier type name cannot start with any of the characters
 * from `%+-#0*` or a space, but other characters are allowed.
 * You can define a more distinctive specifier like `%{k}` instead of `%k`.
 *
 * You can overload the C `printf` format specifiers, but doing so will
 * remove the default behaviors like alignment, zero-padding, precision, etc.
 * If needed, you must implement them yourself.
 *
 * This configuration is used to tell `k_printf` which custom format specifiers
 * to match and the corresponding callback functions.
 */
struct k_printf_config {

    /**
     * \brief Match format specifier and return the callback if matched.
     *
     * You need to first implement the format specifier callback `k_printf_callback_fn`, and then
     * `fn_match_spec` will perform the matching. When `k_printf` encounters the `%` symbol,
     * it will first extract the modifier part of the specifier and then execute `fn_match_spec`.
     *
     * For example, if you have custom format specifiers `%k` and `%k22`, with corresponding
     * callbacks `fn_k` and `fn_k22`, you need to match whether the string begins with `k` or
     * `k22` in `fn_match_spec`. If it does, you should return the corresponding callback
     * and move the string pointer forward. If not, return `NULL` without moving the pointer.
     *
     * For example, when encountering `%+.3k22ss`, `k_printf` will first recognize `%+.3`, and then
     * hand over the matching of `k22ss` to you. You should recognize `k22`, move the string pointer
     * forward by 3 to skip the `k22` part, and return the callback `fn_k22`.
     *
     * You can use `k_printf_match_spec_helper` to help with the string matching.
     *
     * \param str The string pointer to be matched.
     * \return If the match is successful, the function should move the string pointer
     *         past the matched specifier and return the corresponding callback.
     *         Otherwise, it should return `NULL` and not move the string pointer.
     */
    k_printf_callback_fn (*fn_match_spec)(const char **str);
};

/**
 * \brief Structure for defining a pair of format specifier and callback,
 * used only by `k_printf_match_spec_helper`
 */
struct k_printf_spec_callback_tuple {

    /** \brief The format specifier type */
    const char *spec_type;

    /** \brief The corresponding callback function */
    k_printf_callback_fn fn_callback;
};

/**
 * \brief Match a format specifier and return the corresponding callback if matched.
 *
 * This function helps implement `k_printf_config->fn_match_spec`.
 *
 * It iterates through an array of format specifier configurations and matches each specifier
 * with the start of the string. If a match is found, the function moves the string pointer to
 * skip the matched specifier and returns the corresponding callback; If the match fails, the
 * function returns NULL without moving the string pointer.
 *
 * The `spec_configs` array should be a `k_printf_spec_callback_tuple` array, and the last item should be
 * a sentinel value `{ NULL, NULL }`. It is recommended to define this array as a static constant.
 *
 * If your format specifiers have a prefix-based hierarchy, write the longer specifiers first.
 * For example, if `%k` and `%kk` are custom format specifiers, place `%kk` before `%k`.
 */
k_printf_callback_fn k_printf_match_spec_helper(const struct k_printf_spec_callback_tuple *tuples, const char **str);

/**
 * \defgroup k_printf
 *
 * \brief The `k_printf` family of functions
 *
 * The functions in the `k_printf` family are generally similar to the corresponding
 * functions in the C `printf` family.
 *
 * The `config` is used to specify the configuration for the current formatted output.
 * If it is NULL, the default configuration will be used,
 * which only supports the C `printf` format specifiers.
 *
 * The `k_snprintf` function will only write to the buffer if `n` is within
 * the range of positive integers that can be represented by `int`.
 * Using `k_sprintf` is equivalent to using `k_snprintf` with `n` set to `INT_MAX`.
 *
 * The `k_asprintf` function allocates memory using `malloc` to store the formatted string,
 * and returns the string pointer through `get_s`, which the user is responsible for freeing.
 *
 * \param config The configuration to be used for the output.
 * \param file   The `FILE *` to write the formatted string to.
 * \param buf    The `char []` buffer to write the formatted string to.
 * \param n      The length of the `char []` buffer.
 * \param get_s  The pointer to return the dynamically allocated string.
 * \param fmt    The format string.
 * \param ...    The variable arguments corresponding to the format specifiers in the format string.
 * \param args   The variable argument list.
 * \return If successful, the function returns the length of the formatted string.
 *         If it fails, the function returns a negative value.
 *
 * @{
 */

int k_fprintf  (const struct k_printf_config *config, FILE *file, const char *fmt, ...);
int k_vfprintf (const struct k_printf_config *config, FILE *file, const char *fmt, va_list args);
int k_sprintf  (const struct k_printf_config *config, char *buf, const char *fmt, ...);
int k_vsprintf (const struct k_printf_config *config, char *buf, const char *fmt, va_list args);
int k_snprintf (const struct k_printf_config *config, char *buf, size_t n, const char *fmt, ...);
int k_vsnprintf(const struct k_printf_config *config, char *buf, size_t n, const char *fmt, va_list args);
int k_asprintf (const struct k_printf_config *config, char **get_s, const char *fmt, ...);
int k_vasprintf(const struct k_printf_config *config, char **get_s, const char *fmt, va_list args);

/** @} */

#endif
