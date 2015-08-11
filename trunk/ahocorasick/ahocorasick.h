/*
 * ahocorasick.h: The main ahocorasick header file.
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

#ifndef _AUTOMATA_H_
#define _AUTOMATA_H_

#include "replace.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
struct aca_node;
struct mpool;

/* 
 * The A.C. Automata data structure 
 */
typedef struct ac_automata
{
    struct aca_node *root;      /**< The root node of the trie */
    
    size_t patterns_count;      /**< Total patterns in the automata */
    
    /* It is possible to search a long input chunk by chunk. In order to
     * connect these chunks and make a continuous view of the input, we need 
     * the following variables.
     */
    struct aca_node *current_node; /**< Current node */
    size_t base_position; /**< Represents the position of the current chunk,
                           * related to whole input text */
    
    AC_TEXT_t *text;    /**< A helper variable to hold the input text */
    size_t position;    /**< A helper variable to hold the current position in 
                         * the given chunk */
    
    struct replacement_date repdata;    /**< Replacement data structure */
    
    short automata_open; /**< This flag indicates that if automata is finalized 
                          * or not. After finalizing the automata you can not 
                          * add pattern to automata anymore. */
    
    struct mpool *mp;   /**< Memory pool */
    
} AC_AUTOMATA_t;

/* 
 * The API of MultiFast
 */

AC_AUTOMATA_t *ac_automata_init ();
AC_STATUS_t ac_automata_add (AC_AUTOMATA_t *thiz, AC_PATTERN_t *patt, int copy);
void ac_automata_finalize (AC_AUTOMATA_t *thiz);
void ac_automata_release (AC_AUTOMATA_t *thiz);
void ac_automata_display (AC_AUTOMATA_t *thiz);

int  ac_automata_search (AC_AUTOMATA_t *thiz, AC_TEXT_t *text, int keep, 
        AC_MATCH_CALBACK_f callback, void *param);

void ac_automata_settext (AC_AUTOMATA_t *thiz, AC_TEXT_t *text, int keep);
AC_MATCH_t *ac_automata_findnext (AC_AUTOMATA_t *thiz);

int  ac_automata_replace (AC_AUTOMATA_t *thiz, AC_TEXT_t *text, 
        ACA_REPLACE_MODE_t mode, AC_REPLACE_CALBACK_f callback, void *param);
void ac_automata_flush (AC_AUTOMATA_t *thiz);


#ifdef __cplusplus
}
#endif

#endif
