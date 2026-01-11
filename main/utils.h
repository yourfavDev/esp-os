/**
 * @file utils.h
 * @brief A collection of utility functions used across the project.
 */

#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

/**
 * @brief Escapes a string for inclusion in a JSON document.
 *
 * This function takes a raw string and produces a new string where special
 * characters (like quotes, backslashes, and newlines) are properly escaped
 * according to JSON string standards.
 *
 * @param[in]  str The input string to escape.
 * @param[out] out The output buffer to write the escaped string to.
 * @param[in]  out_size The size of the output buffer.
 */
void json_escape(const char *str, char *out, size_t out_size);

#endif // UTILS_H
