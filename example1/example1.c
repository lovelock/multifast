/*
 * example1.c: This program illustrates how to use ahocorasick library
 * This file is part of multifast.
 *
    Copyright 2010-2012 Kamiar Kanani <kamiar.kanani@gmail.com>

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

//*** 0. Include the header
#include "ahocorasick.h"

/* Sample patterns */
AC_ALPHABET_t * allstr[] = {
    "rec",
    "cent",
    "ece",
    "ce",
    "recent",
    "nt",
};

#define PATTERN_NUMBER (sizeof(allstr)/sizeof(AC_ALPHABET_t *))

/* Input text */
AC_ALPHABET_t * input_text = {"She recently graduated from college"};

/****************************************************************************/

//*** 1. Define a call-back function of the MATCH_CALBACK_t type
int match_handler(AC_MATCH_t * m, void * param)
{
    unsigned int j;

    printf ("@ %ld : ", m->position);

    for (j=0; j < m->match_num; j++)
        printf("%ld (%s), ", m->patterns[j].rep.number, m->patterns[j].astring);

    /* CAUTION: be careful about using m->matched_patterns[j].astring
     * if 'astring' has permanent allocation inside your program's
     * memory area, you can use this form. otherwise it will point to
     * an incorrect memory place. in this case you must reconstruct
     * the recognized pattern from the input text.
    **/
    printf("\n");

    /* to find all matches always return 0 */
    return 0;
    /* return 0 : continue searching
     * return none zero : stop searching
     * as soon as you get enough from search results, you can stop search and
     * return from ac_automata_search() and continue the rest of your program.
     * e.g. if you only need first N matches, define a counter and return none
     * zero after the counter exceeds N.
    **/
}

/****************************************************************************/

int main (int argc, char ** argv)
{
    unsigned int i;

    //*** 2. Define AC variables: AC_AUTOMATA_t *, AC_PATTERN_t, and AC_TEXT_t
    AC_AUTOMATA_t * acap;
    AC_PATTERN_t tmp_patt;
    AC_TEXT_t tmp_text;

    //*** 3. Get a new automata
    acap = ac_automata_init (match_handler);

    //*** 4. add patterns to automata
    for (i=0; i<PATTERN_NUMBER; i++)
    {
        tmp_patt.astring = allstr[i];
        tmp_patt.rep.number = i+1; // optional
        tmp_patt.length = strlen(tmp_patt.astring);
        ac_automata_add (acap, &tmp_patt);
    }

    //*** 5. Finalize automata.
    ac_automata_finalize (acap);
    /* after you add all patterns you must finalize automata
     * from now you can not add patterns anymore.
    **/

    //*** 5.1 Display automata
    //ac_automata_display (acap, 'n');
    /* if you are interested to see the automata, call ac_automata_display().
     * the second argument determine how to interpret pattern'
     * representative. 'n': as number, 's': as string. because we use the
     * union as integer (tmp_patt.rep.number) so we used 'n'.
    **/

    //*** 6. Set input text
    tmp_text.astring = input_text;
    tmp_text.length = strlen(tmp_text.astring);

    //*** 7. Do search
    ac_automata_search (acap, &tmp_text, 0);
    /* here we pass 0 to our callback function.
     * if there are variables to pass to call-back function,
     * you can define a struct that enclose those variables and
     * send the pointer of the struct as a parameter.
    **/

    //*** 8. Reset
    ac_automata_reset(acap);
    /* if you want to do another search with same automata
     * you must reset automata.
    **/

    //*** 9. Release automata
    ac_automata_release (acap);
    /* if you have finished with the automata and will not use it in the rest
     * of your program please release it.
    **/

    return 0;
}
