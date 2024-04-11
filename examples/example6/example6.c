/*
 * example1.c: It explains how to use the _search function of the ahocorasick 
 * library
 * 
 * This file is part of multifast.
 *
    Copyright 2010-2015 Kamiar Kanani <kamiar.kanani@gmail.com>

    multifast is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    multifast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with multifast.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "ahocorasick.h"

AC_ALPHABET_t *sample_patterns[] = {
    "city",
    "clutter",
    "ever",
    "experience",
    "neo",
    "one",
    "simplicity",
    "utter",
    "whatever",
    "中文"
};
#define PATTERN_COUNT (sizeof(sample_patterns)/sizeof(AC_ALPHABET_t *))

AC_ALPHABET_t *chunks[3] = {
    "experience the ease and simplicity of multifast中文",
    "whatever you are be a good one",
    "out of clutter, find simplicity"
};

/* Define a call-back function of type AC_MATCH_CALBACK_t */
int match_handler (AC_MATCH_t * matchp, void * param);

typedef struct child_params
{
    AC_TRIE_t *trie;
    AC_ALPHABET_t *alphabet;
} CHILD_PARAMS_t;

typedef struct match_params {
    size_t match_count;
} MATCH_PARAMS_t;

void *child_handler(void * args)
{
    CHILD_PARAMS_t * params = (CHILD_PARAMS_t *)args;
    printf ("Searching: \"%s\" in thread: %lu\n", params->alphabet, (unsigned long int)pthread_self());

    AC_SEARCH_PAYLOAD_t *search_node = ac_search_payload_create(params->trie, params->alphabet);

    /* The 5th option is forwarded to the callback function. you can pass any
     * user parameter to the callback function using this argument.
     *
     * In this example we don't send a parameter to callback function.
     *
     * A typical practice is to define a structure that encloses all the
     * variables you want to send to the callback function.
     */


    MATCH_PARAMS_t *matchParams;
    matchParams = (MATCH_PARAMS_t *) malloc(sizeof(MATCH_PARAMS_t));
    matchParams->match_count = 0;

    /* Search */
    ac_trie_search_thread_safe(params->trie, search_node, 0, match_handler, matchParams);

    printf("Found %lu matches in \"%s\"\n", matchParams->match_count, params->alphabet);

    /* when the keep option (3rd argument) in set, then the automata considers
     * that the given text is the next chunk of the previous text. To see the
     * difference try it with 0 and compare the result */
    pthread_exit(NULL);
}

int main (int argc, char **argv)
{
    unsigned int i;    
    AC_TRIE_t *trie;
    AC_PATTERN_t patt;

    /* Get a new trie */
    trie = ac_trie_create ();
    
    for (i = 0; i < PATTERN_COUNT; i++)
    {
        /* Fill the pattern data */
        patt.ptext.astring = sample_patterns[i];
        patt.ptext.length = strlen(patt.ptext.astring);
        
        /* The replacement pattern is not applicable in this program, so better 
         * to initialize it with 0 */
        patt.rtext.astring = NULL;
        patt.rtext.length = 0;
        
        /* Pattern identifier is optional */
        patt.id.u.number = i + 1;
        patt.id.type = AC_PATTID_TYPE_NUMBER;
        
        /* Add pattern to automata */
        ac_trie_add (trie, &patt, 0);
        
        /* We added pattern with copy option disabled. It means that the 
         * pattern memory must remain valid inside our program until the end of 
         * search. If you are using a temporary buffer for patterns then you 
         * may want to make a copy of it so you can use it later. */
    }
    
    /* Now the preprocessing stage ends. You must finalize the trie. Remember 
     * that you can not add patterns anymore. */
    ac_trie_finalize (trie);
    
    /* Finalizing the trie is the slowest part of the task. It may take a 
     * longer time for a very large number of patters */
    
    /* Display the trie if you wish */
    // ac_trie_display (trie);

    pthread_t threads[3];

    int t;
    for (t = 0; t < 3; t++) {
        CHILD_PARAMS_t *params = (CHILD_PARAMS_t *)malloc(sizeof(CHILD_PARAMS_t));
        params->trie = trie;
        params->alphabet = chunks[t];
        pthread_create(&threads[t], NULL, child_handler, params);
    }

    for (t = 0; t < 3; t++) {
        pthread_join(threads[t], NULL);
    }
    /* You may release the automata after you have done with it. */
    ac_trie_release (trie);
    
    return 0;
}

void print_match (AC_MATCH_t *m, MATCH_PARAMS_t* params)
{
    unsigned int j;
    
    printf ("@%2lu found: ", m->position);

    for (j = 0; j < m->size; j++)
    {
        printf("#%ld \"%.*s\", ", m->patterns[j].id.u.number,
            (int)m->patterns[j].ptext.length, m->patterns[j].ptext.astring);

        params->match_count++;
        /* CAUTION: the AC_PATTERN_t::ptext.astring pointers, point to the
         * sample patters in our program, since we added patterns with copy 
         * option disabled.
         */        
    }
    
    printf (" in thread: %lu\n", (unsigned long int)pthread_self());
}

int match_handler (AC_MATCH_t * matchp, void * param)
{
    MATCH_PARAMS_t *matchParams = (MATCH_PARAMS_t*)param;
    print_match (matchp, matchParams);
    
    return 0;
    /* Zero return value means that it will continue search 
     * Non-zero return value means that it will stop search
     * 
     * As you get enough from search results, you can stop search and return 
     * from _search() and continue the rest of your program. E.g. if you only 
     * need first N matches, define a counter and return non-zero value after 
     * the counter exceeds N. To find all matches always return 0 
     */
}
