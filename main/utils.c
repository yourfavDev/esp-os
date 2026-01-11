/**
 * @file utils.c
 * @brief Implementation of utility functions.
 */

#include "utils.h"
#include <stdio.h> // For snprintf

void json_escape(const char *str, char *out, size_t out_size)
{
    if (!str || !out || out_size == 0)
    {
        if (out && out_size > 0) out[0] = '\0';
        return;
    }

    size_t j = 0;
    for (size_t i = 0; str[i] && j < out_size - 6; i++) // -6 for worst case \uXXXX and null
    {
        char c = str[i];
        switch (c)
        {
        case '"':
            out[j++] = '\\';
            out[j++] = '"';
            break;
        case '\\':
            out[j++] = '\\';
            out[j++] = '\\';
            break;
        case '\b':
            out[j++] = '\\';
            out[j++] = 'b';
            break;
        case '\f':
            out[j++] = '\\';
            out[j++] = 'f';
            break;
        case '\n':
            out[j++] = '\\';
            out[j++] = 'n';
            break;
        case '\r':
            out[j++] = '\\';
            out[j++] = 'r';
            break;
        case '\t':
            out[j++] = '\\';
            out[j++] = 't';
            break;
        default:
            // Control characters and other non-printables
            if (c < 32)
            {
                j += snprintf(&out[j], out_size - j, "\\u%04x", (unsigned int)c);
            }
            else
            {
                out[j++] = c;
            }
        }
    }
    // Ensure null termination
    if (j < out_size)
    {
        out[j] = '\0';
    }
    else
    {
        out[out_size - 1] = '\0';
    }
}
