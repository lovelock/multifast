// Created by frost on 2024/4/12.
//

#ifndef MULTIFAST_EXAMPLES_DICT_H
#define MULTIFAST_EXAMPLES_DICT_H

#include "ahocorasick.h"

typedef struct word {
    unsigned int id;
    char *pattern;
    void* present;
} AC_WORD_t;

AC_TRIE_t *load_trie_from_dict(char *dict_path);

#endif //MULTIFAST_EXAMPLES_DICT_H
