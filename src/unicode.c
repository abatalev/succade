#include "unicode.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

uint32_t hex_to_bin(char c) {
    if (c >= '0' && c <= '9') 
        return ((uint32_t)c) - '0';
    else if (c >= 'a' && c <= 'f')
        return ((uint32_t)c) - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
        return ((uint32_t)c) - 'A' + 10;
    return 0;
}

void parse_unicode_escape_sequences(const char* str, char* res) {
    size_t k = 0;
    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        if (str[i] != '\\') {
            res[k++] = str[i];
            continue;
        }

        if (i + 1 < len && str[i + 1] == '\\') {
            res[k++] = '\\';
            i += 1;
        } else if (i + 5 < len && str[i + 1] == 'u') {
            uint32_t unicode_character =
                hex_to_bin(str[i + 2]) << 12 | hex_to_bin(str[i + 3]) <<  8 |
                hex_to_bin(str[i + 4]) <<  4 | hex_to_bin(str[i + 5]) <<  0;
            res[k++] = 0xE0 | ((unicode_character >> 12) & 0x0F);
            res[k++] = 0x80 | ((unicode_character >>  6) & 0x3F);
            res[k++] = 0x80 | ((unicode_character >>  0) & 0x3F);
            i += 5;
        } else if (i + 9 < len && str[i + 1] == 'U') {
            uint32_t unicode_character =
                hex_to_bin(str[i + 2]) << 28 | hex_to_bin(str[i + 3]) << 24 |
                hex_to_bin(str[i + 4]) << 20 | hex_to_bin(str[i + 5]) << 16 |
                hex_to_bin(str[i + 6]) << 12 | hex_to_bin(str[i + 7]) <<  8 |
                hex_to_bin(str[i + 8]) <<  4 | hex_to_bin(str[i + 9]) <<  0;
            res[k++] = 0xF0 | ((unicode_character >> 18) & 0x07);
            res[k++] = 0x80 | ((unicode_character >> 12) & 0x3F);
            res[k++] = 0x80 | ((unicode_character >>  6) & 0x3F);
            res[k++] = 0x80 | ((unicode_character >>  0) & 0x3F);
            i += 9;
       }
    }
    res[k++] = 0;
}

char * parse_unicode(const char* str) {
    char * res;
    if (str == NULL) {
        res = malloc(1);
        res[0] = 0;
        return res;
    }

    res = malloc(strlen(str)+1);
    parse_unicode_escape_sequences(str,res);
    return res;
}
