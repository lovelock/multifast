/*
 * example4.c: This is an example program
 * It shows how to use the ac_automata_replace() function
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
#include "ahocorasick.h"

#define PATTERN(p,r)    {{p,sizeof(p)-1},{r,sizeof(r)-1},{{0},0}}
#define CHUNK(c)        {c,sizeof(c)-1}

AC_PATTERN_t patterns[] = {
    PATTERN("city", "[S1]"),    /* Replace "simplicity" with "[p1]" */
    PATTERN("the ", ""),        /* Replace "the " with an empty string */
    PATTERN("and", NULL),       /* Do not replace "and" */
    PATTERN("experience", "[practice]"),
    PATTERN("exp", "[S2]"),
    PATTERN("multi", "[S3]"),
    PATTERN("ease", "[S4]"),
};

AC_TEXT_t input_chunks[] = {
    CHUNK("experience "),
    CHUNK("the ease "),
    CHUNK("and simplicity "),
    CHUNK("of multifast"),
};

#define PATTERN_NUMBER (sizeof(patterns)/sizeof(AC_PATTERN_t))
#define CHUNK_NUMBER (sizeof(input_chunks)/sizeof(AC_TEXT_t))

/* 1. Listener (call-back) function */
void listener (AC_TEXT_t * text, void * user)
{
    /* Just print it */
    printf ("%.*s", (int)text->length, text->astring);
}

int main (int argc, char ** argv)
{
    unsigned int i;
    
    /* 2. AC variables */
    AC_TRIE_t * atm;
    
    /* 3. Get an automata */
    atm = ac_trie_create ();
    
    /* 4. Add patterns to the automata */
    for (i=0; i<PATTERN_NUMBER; i++)
        if (ac_trie_add (atm, &patterns[i], 0)!=ACERR_SUCCESS)
            printf("Failed to add pattern \"%.*s\"\n", 
                    (int)patterns[i].ptext.length, patterns[i].ptext.astring);
    
    /* 5. Finalize the automata */
    ac_trie_finalize (atm);
    
    printf("Normal replace mode:\n");

    /* 6. Call replace */
    for (i=0; i<CHUNK_NUMBER; i++)
        multifast_replace (atm, 
                &input_chunks[i], MF_REPLACE_MODE_NORMAL, listener, 0);
    
    /* 7. Flush the buffer at the end (after the last chunk was fed) */
    multifast_rep_flush (atm, 0);
    
    printf("\nLazy replace mode:\n");
    
    /* -. Call replace */
    for (i=0; i<CHUNK_NUMBER; i++)
        multifast_replace (atm, 
                &input_chunks[i], MF_REPLACE_MODE_LAZY, listener, 0);
    
    /* -. Flush the buffer at the end (after the last chunk was fed) */
    multifast_rep_flush (atm, 0);
    
    printf("\n");
    
    /* 8. Release the automata after you have done with it */
    ac_trie_release (atm);
    
    return 0;
}
