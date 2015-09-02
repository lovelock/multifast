/*
 * example0.c
 * It shows how to use the _settext/_findnext interface of the ahocorasick API
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
#include "ahocorasick.h"

AC_ALPHABET_t * sample_patterns[] = {
    "city",
    "clutter",
    "ever",
    "experience",
    "neo",
    "one",
    "simplicity",
    "utter",
    "whatever",
};
#define PATTERN_COUNT (sizeof(sample_patterns)/sizeof(AC_ALPHABET_t *))

AC_ALPHABET_t * text1 = "experience the ease and simplicity of multifast";
AC_ALPHABET_t * text2 = "whatever you are be a good one";
AC_ALPHABET_t * text3 = "out of clutter, find simplicity";


int main (int argc, char ** argv)
{
    unsigned int j;
    AC_AUTOMATA_t *trie;
    AC_PATTERN_t patt;
    AC_TEXT_t chunk;
    AC_MATCH_t match;
    
    /* Get a new trie */
    trie = ac_automata_init ();
    
    for (j = 0; j < PATTERN_COUNT; j++)
    {
        /* Fill the pattern data */
        patt.ptext.astring = sample_patterns[j];
        patt.ptext.length = strlen(patt.ptext.astring);
        
        /* The replacement pattern is not applicable, so better to initialize 
         * it with 0 */
        patt.rtext.astring = NULL;
        patt.rtext.length = 0;
        
        /* Pattern identifier is optional */
        patt.id.u.number = j + 1;
        patt.id.type = AC_PATTID_TYPE_NUMBER;
        
        /* Add pattern to automata */
        ac_automata_add (trie, &patt, 0);
        
        /* We added pattern with copy option disabled. It means that the 
         * pattern memory must remain valid inside our program until the end of 
         * search. If you are using a temporary buffer for patterns then you 
         * may want to make a copy of it so you can use it later. */
    }
    
    /* Now the preprocessing stage ends. You must finalize the trie. Remember 
     * that you can not add patterns anymore. */
    ac_automata_finalize (trie);
    
    /* Finalizing the trie is the slowest part of the task. It may take a 
     * longer time for a very large number of patters */
    
    /* Display the trie if you wish */
    // ac_automata_display (atm);
    
    printf ("Searching: \"%s\"\n", text1);
    
    chunk.astring = text1;
    chunk.length = strlen(chunk.astring);
    
    /* Set the input text */
    ac_automata_settext (trie, &chunk, 0);
    
    /* The ownership of the input text belongs to the caller program. I.e. the
     * API does not make a copy of that. It must remain valid until the end
     * of search of the given chunk. */
    
    /* Find matches */
    while ((match = ac_automata_findnext(trie)).size)
    {
        printf ("@%2lu: ", match.position);
        
        for (j = 0; j < match.size; j++)
            printf("#%ld (%s), ", 
                    match.patterns[j].id.u.number, 
                    match.patterns[j].ptext.astring);
        
        /* CAUTION: the AC_PATTERN_t::ptext.astring pointer, points to the 
         * input string in our program. So we should preserve it until the 
         * end of search. */
        
        printf ("\n");
    }
    
    printf ("Searching: \"%s\"\n", text2);
    
    chunk.astring = text2;
    chunk.length = strlen(chunk.astring);
    
    /* Set the input text as a new search (keep = 0) */
    ac_automata_settext (trie, &chunk, 0);
    
    while ((match = ac_automata_findnext(trie)).size)
    {        
        printf ("@%2lu: ", match.position);
        
        for (j = 0; j < match.size; j++)
            printf("#%ld (%s), ", 
                    match.patterns[j].id.u.number, 
                    match.patterns[j].ptext.astring);
        
        printf ("\n");
    }
    
    printf ("Searching: \"%s\"\n", text3);
    
    chunk.astring = text3;
    chunk.length = strlen(chunk.astring);
    
    /* Set the input text as the successor chunk of the previous one */
    ac_automata_settext (trie, &chunk, 1);
    
    /* When the keep option (3rd argument) in set, then the automata considers 
     * that the given text is the next chunk of the previous text. To see the 
     * difference try it with 0 and compare the result */
    
    while ((match = ac_automata_findnext(trie)).size)
    {        
        printf ("@%2lu: ", match.position);
        
        for (j = 0; j < match.size; j++)
            printf("#%ld (%s), ", 
                    match.patterns[j].id.u.number, 
                    match.patterns[j].ptext.astring);
        
        printf ("\n");
    }
    
    /* You may release the automata after you have done with it. */
    ac_automata_release (trie);
        
    return 0;
}
