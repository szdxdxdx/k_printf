#ifndef K_PRINTF_H
#define K_PRINTF_H

#include <stdio.h>
#include <stddef.h>

struct k_printf_config;

/**
 * \brief 将格式化字符串写入到标准输出流 `stdout`，并返回格式化后的字符串长度
 *
 * \ingroup k_printf
 *
 * \param config 本次输出使用的配置，若为 NULL 则使用默认配置。
 * \param fmt    格式字符串（格式说明符兼容 C `printf`）
 * \param ...    不定参数，根据格式字符串中的说明符，匹配要输出的值
 * \return 若成功，返回格式化后的字符串长度；若失败，返回负值。
 */
int k_printf(const struct k_printf_config *config, const char *fmt, ...);

struct k_printf_buf;
struct k_printf_spec;

/**
 * \brief 自定义格式说明符的回调函数
 *
 * 在回调中，`k_printf` 提供给你一个抽象的缓冲区接口 `k_printf_buf`，
 * 你需要使用 `k_printf_buf` 提供的函数往缓冲区中写入内容，
 * 不用考虑缓冲区类型是 `char []` 还是 `FILE *`。
 *
 * C `printf` 支持使用 `+-#0 *.*` 修饰格式说明符的转换行为，`k_printf` 也支持。
 * 在回调中，`k_printf` 会通过 `spec` 告诉你所识别到的格式说明符的详细信息。
 *
 * 对于你自定义的格式说明符，你需要自行实现这些修饰符号对应的功能。你可以重载这些符号的含义，
 * 例如，你可以认为 `+` 表示详细输出，`-` 表示简要输出，这取决于你自己的实现。
 * 若你的格式说明符不需要支持这些修饰功能，那你多半可以忽略 `spec`。
 *
 * 在回调中，`k_printf` 传递给你不定长参数列表的指针 `args`。
 * 这里传递的是指针，因为 `k_printf` 需要知道执行完你在回调后消耗了多少实参。
 * 你应通过 `va_arg(*args, type)` 从列表中获取一个 type 类型的实参。
 * 
 * 你要留意 C 在传递不定长参数时存在类型提升。例如 `char` 被提升为 `int`，
 * 若要读取一个 `char` 类型实参，应使用 `(char) va_arg(*args, int)`。
 *
 * 你应按需消耗实参。不要残留下应该由你来处理的实参，不要多消耗不属于你的实参，
 * 否则，后续的格式说明符会匹配到错误的实参，可能导致程序崩溃。
 *
 * \param buf  缓冲区，你应使用 `k_printf_buf` 提供的函数往缓冲区中写入内容
 * \param spec 提供当前格式说明符的详细信息
 * \param args 指向不定长参数列表的指针，你应按需消耗列表中的实参
 */
typedef void (*k_printf_callback_fn)(struct k_printf_buf *buf, const struct k_printf_spec *spec, va_list *args);

/** \brief 缓冲区接口，对 `char []` 和 `FILE *` 两类缓冲区统一的操作接口 */
struct k_printf_buf {

    /** \brief 往缓冲区中写入指定长度的字符串 */
    void (*fn_puts)(struct k_printf_buf *buf, const char *str, size_t len);

    /** \brief 往缓冲区格式化写入格式化字符串（格式说明符同 C `printf`） */
    void (*fn_printf)(struct k_printf_buf *buf, const char *fmt, ...);

    /** \brief 往缓冲区格式化写入格式化字符串（格式说明符同 C `printf`） */
    void (*fn_vprintf)(struct k_printf_buf *buf, const char *fmt, va_list args);

    /**
     * \brief 到目前为止已经打印出的字符数量（忽略缓冲区实际大小）
     *
     * C `printf` 一族的函数会返回格式化后的字符串长度，无论缓冲区是否能完全容纳格式化后的字符串。
     * 例如，当我们只关心格式化后的字符串长度，而不需要真正写入内容时，可以使用 `snprintf`，
     * 并指定使用 0 大小的缓冲区，`snprintf` 不会写入任何内容，只是返回格式化后的字符串长度。
     *
     * `k_printf` 一族的函数也有相同的行为。
     *
     * `n` 记录着到目前为止已打印出的字符数量（忽略缓冲区实际大小）。
     * `k_printf` 一族的函数返回值，就是将字符串格式化完毕后 `n` 的值。
     *
     * 若想实现与 `printf` 的 `%n` 相似的功能，你需要在你的回调中读取 `n`。
     * 若读取到 `n` 为负值，说明输出途中出现错误。
     */
    int n;
};

/** \brief 格式说明符 */
struct k_printf_spec {

    /** \brief `-` 左对齐 */
    unsigned int left_justified : 1;

    /** \brief `+` 始终显示符号 */
    unsigned int sign_prepended : 1;

    /** \brief ` ` 空格填充 */
    unsigned int space_padded : 1;

    /** \brief `0` 零填充 */
    unsigned int zero_padding : 1;

    /** \brief `#` 特殊格式修饰 */
    unsigned int alternative_form : 1;

    /** \brief `*` 使用最小宽度 */
    unsigned int use_min_width : 1;

    /** \brief `.*` 使用精度 */
    unsigned int use_precision : 1;

    /**
     * \brief 最小宽度
     *
     * 只有 `use_min_width` 为非 0 时，`min_width` 才有意义。
     *
     * 假定 `%k` 是你自定义的格式说明符，则：
     *
     * 若是静态指定的最小宽度，则 `min_width` 保存着该宽度值，一定为正，最大为 INT_MAX。
     * 例如：
     * 遇到 `%2k` 时，`min_width` 的值为 2。
     * 遇到 `%02k` 时，`0` 被识别成零填充修饰符，`min_width` 的值为 2。
     * 遇到 `%-2k` 时，`-` 被识别成左对齐修饰符，`min_width` 的值为 2。
     * 遇到 `%999999999999k` 时，`min_width` 的值为 INT_MAX。
     *
     * 若是动态指定的最小宽度，则 `min_width` 为 -1。
     * 例如，遇到 `%*k` 时，`use_min_width` 为非 0，`min_width` 为 -1，
     * 此时你应使用 `va_arg(*args, int)` 获取动态指定的最小宽度。
     */
    int min_width;

    /**
     * \brief 精度
     *
     * 只有 `use_precision` 为非 0 时，`precision` 才有意义。
     *
     * 假定 `%k` 是你自定义的格式说明符，则：
     *
     * 若是静态指定的精度，则 `precision` 保存着该精度值，一定为正，最大为 INT_MAX。
     * 例如：
     * 遇到 `%.2k` 时，`precision` 的值为 2。
     * 遇到 `%.k` 时，`precision` 的值为 0。
     * 遇到 `%.999999999999k` 时，`precision` 的值为 INT_MAX。
     * 遇到 `%.-2k` 时，无法正确识别出格式说明符`%k`。
     *
     * 若是动态指定的精度，则 `precision` 为 -1。
     * 例如，遇到 `%.*k` 时，`use_precision` 为非 0，`precision` 为 -1，
     * 此时你应使用 `va_arg(*args, int)` 获取动态指定的精度。
     */
    int precision;

    /** \brief 指向原格式字符串中该格式说明符 `%` 符号的位置 */
    const char *start;

    /** \brief 指向原格式字符串中该格式说明符的类型部分的起始位置 */
    const char *type;

    /** \brief 指向原格式字符串中该格式说明符的结束位置 */
    const char *end;
};

/**
 * \brief 自定义格式说明符的配置
 *
 * C `printf` 的格式说明符格式为 `%[标志][最小宽度][.精度][长度修饰符]转换指示符`，
 * 也许 C `printf` 认为 `%lld` 是经过 `ll` 修饰的 `%d`，认为 `%lld` 和 `%d` 同属一类？
 * 但 `k_printf` 把 `[长度修饰符]转换指示符` 视为一个整体，称作格式说明符的 `类型`。
 * `k_printf` 认为 `%lld` 和 `%d` 是两种不同的类型。
 *
 * 假定你自定义的格式说明符为 `%k`，则它的类型名是 `k`（不含 `%`）。
 * 若你没有定义 `%llk`，则 `%llk` 仍是未知的格式说明符。
 * 自定义格式说明符的类型名不能以 `%+-#0*` 中的任一个字符或是空格开头。
 * 你可以自定义格式指示符为 `%{k}`，这比 `%k` 更显眼。
 *
 * 你可以重载 C `printf` 的格式说明符，但会失去所有修饰符的默认行为，
 * 不再支持左右对齐、零填充、精度等。若需要，你得自己再实现它们。
 *
 * 你将通过此配置来告知 `k_printf` 要匹配哪些自定义格式说明符，并告知对应的回调。
 */
struct k_printf_config {

    /**
     * \brief 匹配字符串开头的格式说明符，若匹配成功则移动字符串指针，并返回对应的回调
     *
     * 你需要先编写格式说明符的回调 `k_printf_callback_fn`，然后在 `fn_match_spec` 完成其匹配工作。
     * `k_printf` 遇到 `%` 符号时，会先提取格式说明符的修饰部分，再执行 `fn_match_spec`。
     *
     * 假定你有一组自定义的格式说明符 `%k` 和 `%k22`，假定对应的回调分别为 `fn_k` 和 `fn_k22`。
     * 你需要在 `fn_match_spec` 中匹配字符串的开头是否为 `k` 或是 `k22`，
     * 若是，你需要返回对应的回调，并移动字符串指针。若都不是，你需要返回 NULL，且不要移动字符串指针。
     *
     * 例如，遇到 `%+.3k22ss` 时，`k_printf` 会先识别 `%+.3` 的部分，再交由你来识别 `k22ss` 的部分。
     * 你应该识别出 `k22`，正确移动字符串指针 +3 越过 `k22` 的部分，并返回其回调 `fn_k22`。
     *
     * 你可以使用 `k_printf_match_spec_helper` 帮助你完成字符串匹配工作。
     *
     * \param str 字符串指针
     * \return 若匹配成功，函数应移动字符串指针跳过该说明符，并返回对应回调，
     *         否则函数应返回 NULL，且不移动字符串指针。
     */
    k_printf_callback_fn (*fn_match_spec)(const char **str);
};

/** \brief 用于定义一对格式说明符与回调，仅用于 `k_printf_match_spec_helper` */
struct k_printf_spec_callback_tuple {

    /** \brief 格式说明符类型 */
    const char *spec_type;

    /** \brief 对应的回调函数 */
    k_printf_callback_fn fn_callback;
};

/**
 * \brief 匹配格式说明符，若匹配成功则移动字符串指针，并返回对应的回调
 *
 * 本函数可以帮助你完成 `k_printf_config->fn_match_spec` 的实现。
 *
 * 函数顺序遍历一组格式说明符配置项，将每个说明符与字符串开头进行匹配。
 * 若匹配成功，则移动字符串指针跳过该说明符，函数返回相应的回调；
 * 若匹配失败，函数返回 NULL，且不移动字符串指针。
 *
 * 传递的 `tuples` 应是一个 `k_printf_spec_callback_tuple` 数组，
 * 且要求数组最后一项是哨兵值 `{ NULL, NULL }`。建议将该数组定义为静态常量。
 *
 * 若你的格式说明符有前缀包含关系，请把长的格式说明符写在前面。
 * 假定 `%k` 和 `%kk` 都是你自定义的格式说明符，请把 `%kk` 放在 `%k` 前面。
 */
k_printf_callback_fn k_printf_match_spec_helper(const struct k_printf_spec_callback_tuple *tuples, const char **str);

/**
 * \defgroup k_printf
 *
 * \brief `k_printf` 家族
 *
 * `k_printf` 家族中各函数的用法大致可参考 C `printf` 家族中对应的函数。
 *
 * `config` 参数用来指明本次格式化输出使用的配置。
 * 若为 NULL 则使用默认配置，仅支持 C `printf` 的格式指示符。
 *
 * 只有 `n` 处在 int 所能表示的正数范围内时，`k_snprintf` 才会往缓冲区写入内容。
 * 使用 `k_sprintf` 等同于在使用 `k_snprintf` 且指定 `n` 为 INT_MAX。
 *
 * `k_asprintf` 使用 `malloc` 分配缓冲区来存储格式化后的字符串，
 * 通过 `get_s` 返回该字符串指针，由用户负责释放。
 *
 * \param config 本次输出使用的配置
 * \param file   将格式化字符串到写入 `FILE *`
 * \param buf    将格式化字符串到写入 `char []`
 * \param n      `char []` 缓冲区的长度
 * \param get_s  返回动态分配的字符串的指针
 * \param fmt    格式字符串
 * \param ...    不定参数，根据格式字符串中的说明符，匹配要输出的值
 * \param args   不定长参数列表
 * \return 若成功，函数返回格式化后的字符串长度（忽略缓冲区的实际长度）；若失败，函数返回负值。
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
