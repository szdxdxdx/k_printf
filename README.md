

# k_printf

## 中文文档 Chinese document

k_printf 是对 C 标准库中的 printf 的包装，允许自定义格式说明符和对应回调。

示例：
```C

/* 第一步，编写自定义格式说明符 `%arr` 的回调函数，用于打印 int 数组 */
void my_spec_arr(struct k_printf_buf *printf_buf, const struct k_printf_spec *spec, va_list *args) {

    /* 在回调中，使用 `k_printf_buf->fn_tbl` 提供的函数，
     * 往缓冲区 `printf_buf `中写入内容。
     * 不需要考虑缓冲区类型是 `char []` 还是 `FILE *`。
     */
}

/* 第二步，编写 k_printf 的配置，注册自定义格式说明符 `%arr` */
struct k_printf_config config = { ... };

/* 第三步，使用 k_printf */

int arr[] = { 1, 2, 3, 4, 5 };
k_printf(config, "%s %arr", "arr[] =", arr, 5);

```

详细示例见 `src/example.c`

## 英文文档 English document

`k_printf` is a wrapper around the C standard library's `printf`,
allowing custom format specifier and corresponding callback functions.

Example:
```C

/* Step 1: Write a callback function for the custom format specifier `%arr` to print an int array */
void my_spec_arr(struct k_printf_buf *printf_buf, const struct k_printf_spec *spec, va_list *args) {
    /* In the callback, use functions from `k_printf_buf->fn_tbl` to write content to `printf_buf` 
     * without worrying about whether the buffer is of type `char[]` or `FILE *`. 
     */
}

/* Step 2: Define the `k_printf` configuration and register the custom format specifier `%arr` */
struct k_printf_config config = { ... };

/* Step 3: Use `k_printf` */
int arr[] = { 1, 2, 3, 4, 5 };
k_printf(config, "%s %arr", "arr[] =", arr, 5);

```

Detailed examples can be found in `src/example.c`.

I used ChatGPT to translate the example documentation `src/example.c` and the header file `src/k_printf.h`.
The translated results can be found in `/en` folder.
You can replace the corresponding files in the `/src` folder with them.
