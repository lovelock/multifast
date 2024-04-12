//
// Created by frost on 2024/4/12.
//

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "dict.h"


void process_string(const char *start, int length, char **outString)
{
    *outString = (char*) malloc(length+1);
    if (*outString == NULL) {
        printf("Memory allocation failed\n");
        exit(1);
    }

    snprintf(*outString, length+1, "%.*s", length, start);
}

long process_id(const char *start, int length, char **outString)
{
    process_string(start, length, outString);
    char *endPtr;

    printf("number id: %s\n", *outString);

    long number = strtol(*outString, &endPtr, 10);

    // 检查转换是否成功
    if (endPtr == *outString) {
        printf("No valid conversion could be performed.\n");
    } else if (*endPtr != '\0') {
        printf("Partially converted to long: %ld, stopped at: %s\n", number, endPtr);
    } else {
        printf("Number successfully converted to long: %ld\n", number);
    }

    printf("converted %lu\n", number);

    return number;
}

void process_line(const char* line, size_t length, AC_TRIE_t *trie) {
    const char *start = line;

    int index = 0;
    int l = 0;
    char *outString = NULL;


    AC_PATTERN_t patt;
    for (int i = 0; line[i] != '\0'; ++i) {
        if (line[i] == '|') {
            ++index;

            if (index == 1) { // pattern
                process_string(start, l, &outString);
                patt.ptext.astring = outString;
                patt.ptext.length = strlen(patt.ptext.astring);
                printf("pattern astring: %s\n", patt.ptext.astring);
            }

            if (index == 2) { // id
                long number = process_id(start, l, &outString);
                patt.id.u.number = number;
                patt.id.type = AC_PATTID_TYPE_NUMBER;
            }

            start = line + i + 1;
            l = 0;
        } else {
            ++l;
        }
    }

    ac_trie_add(trie, &patt, 1);
}

AC_TRIE_t *load_trie_from_dict(char *dict_path)
{
    int fd = open(dict_path, O_RDONLY);
    if (fd == -1) {
        printf("File '%s' does not exist.\n", dict_path);
        exit(1);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        printf("Error getting file size");
        close(fd);
        exit(1);
    }

    char *file_in_memory = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_in_memory == MAP_FAILED) {
        printf("Error mmapping the file");
        close(fd);
        exit(1);
    }

    /* Get a new trie */
    AC_TRIE_t *trie = ac_trie_create ();
    // 现在文件的内容已经映射到内存中，可以像操作内存一样操作文件了
    const char *start = file_in_memory;
    for (size_t i = 0; i < sb.st_size; i++) {
        if (file_in_memory[i] == '\n') {
            // 发现一行的结尾，处理这一行
            process_line(start, &file_in_memory[i] - start, trie);
            start = &file_in_memory[i] + 1; // 移动到下一行的开始
        }
    }
    // 处理最后一行（如果文件不是以换行符结尾）
    if (start < file_in_memory + sb.st_size) {
        process_line(start, file_in_memory + sb.st_size - start, trie);
    }

    ac_trie_finalize(trie);

    // 使用完毕，释放资源
    if (munmap(file_in_memory, sb.st_size) == -1) {
        printf("Error un-mmapping the file");
    }
    close(fd);

    return trie;
}
