/*
 * node.c: implementation of automata node
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
#include <stdlib.h>
#include "node.h"

/* Private function prototype */
void node_init         (AC_NODE_t * thiz);
int  node_edge_compare (const void * l, const void * r);
int  node_has_matchstr (AC_NODE_t * thiz, AC_PATTERN_t * newstr);
void node_grow_outgoing_vector (AC_NODE_t * thiz);
void node_grow_matchstr_vector (AC_NODE_t * thiz);

/******************************************************************************
 * FUNCTION: node_create
 * Create the node
******************************************************************************/
struct AC_NODE * node_create(void)
{
    AC_NODE_t * thiz;
    thiz = (AC_NODE_t *) malloc (sizeof(AC_NODE_t));
    node_init(thiz);
    node_assign_id(thiz);
    return thiz;
}

/******************************************************************************
 * FUNCTION: node_init
 * Initialize node
******************************************************************************/
void node_init(AC_NODE_t * thiz)
{
    thiz->id = 0;
    thiz->final = 0;
    thiz->failure_node = NULL;
    thiz->depth = 0;
    
    thiz->matched_patterns = NULL;
    thiz->matched_patterns_max = 0;
    thiz->matched_patterns_num = 0;
    
    thiz->outgoing = NULL;
    thiz->outgoing_max = 0;
    thiz->outgoing_degree = 0;
    
    thiz->to_be_replaced = NULL;
}

/******************************************************************************
 * FUNCTION: node_release
 * Release node
******************************************************************************/
void node_release(AC_NODE_t * thiz)
{
    free(thiz->matched_patterns);
    free(thiz->outgoing);
    free(thiz);
}

/******************************************************************************
 * FUNCTION: node_find_next
 * Find out the next node for a given Alpha to move. this function is used in
 * the pre-processing stage in which edge array is not sorted. so it uses
 * linear search.
******************************************************************************/
AC_NODE_t * node_find_next(AC_NODE_t * thiz, AC_ALPHABET_t alpha)
{
    int i;

    for (i=0; i < thiz->outgoing_degree; i++)
    {
        if(thiz->outgoing[i].alpha == alpha)
            return (thiz->outgoing[i].next);
    }
    return NULL;
}

/******************************************************************************
 * FUNCTION: node_findbs_next
 * Find out the next node for a given Alpha. this function is used after the
 * pre-processing stage in which we sort edges. so it uses Binary Search.
******************************************************************************/
AC_NODE_t * node_findbs_next (AC_NODE_t * thiz, AC_ALPHABET_t alpha)
{
    int min, max, mid;
    AC_ALPHABET_t amid;

    min = 0;
    max = thiz->outgoing_degree - 1;

    while (min <= max)
    {
        mid = (min+max) >> 1;
        amid = thiz->outgoing[mid].alpha;
        if (alpha > amid)
            min = mid + 1;
        else if (alpha < amid)
            max = mid - 1;
        else
            return (thiz->outgoing[mid].next);
    }
    return NULL;
}

/******************************************************************************
 * FUNCTION: node_has_matchstr
 * Determine if a final node contains a pattern in its accepted pattern list
 * or not. return values: 1 = it has, 0 = it hasn't
******************************************************************************/
int node_has_matchstr (AC_NODE_t * thiz, AC_PATTERN_t * newstr)
{
    unsigned short i;
    size_t j;
    AC_PATTERN_t * str;

    for (i=0; i < thiz->matched_patterns_num; i++)
    {
        str = &thiz->matched_patterns[i];

        if (str->ptext.length != newstr->ptext.length)
            continue;

        for (j=0; j<str->ptext.length; j++)
            if(str->ptext.astring[j] != newstr->ptext.astring[j])
                continue;

        if (j == str->ptext.length)
            return 1;
    }
    return 0;
}

/******************************************************************************
 * FUNCTION: node_create_next
 * Create the next node for the given alpha.
******************************************************************************/
AC_NODE_t * node_create_next (AC_NODE_t * thiz, AC_ALPHABET_t alpha)
{
    AC_NODE_t * next;
    next = node_find_next (thiz, alpha);
    if (next)
    /* The edge already exists */
        return NULL;
    /* Otherwise register new edge */
    next = node_create ();
    node_register_outgoing(thiz, next, alpha);

    return next;
}

/******************************************************************************
 * FUNCTION: node_register_matchstr
 * Adds the pattern to the list of accepted pattern.
******************************************************************************/
void node_register_matchstr (AC_NODE_t * thiz, AC_PATTERN_t * str)
{
    AC_PATTERN_t * patt;
    
    /* Check if the new pattern already exists in the node list */
    if (node_has_matchstr(thiz, str))
        return;

    /* Manage memory */
    if (thiz->matched_patterns_num == thiz->matched_patterns_max)
        node_grow_matchstr_vector (thiz);
    
    patt = &thiz->matched_patterns[thiz->matched_patterns_num];
    
    patt->ptext.astring = str->ptext.astring;
    patt->ptext.length = str->ptext.length;
    patt->rtext.astring = str->rtext.astring;
    patt->rtext.length = str->rtext.length;
    patt->title = str->title;
    
    thiz->matched_patterns_num++;
}

/******************************************************************************
 * FUNCTION: node_register_outgoing
 * Establish an edge between two nodes
******************************************************************************/
void node_register_outgoing
    (AC_NODE_t * thiz, AC_NODE_t * next, AC_ALPHABET_t alpha)
{
    struct edge * oe;
    
    if(thiz->outgoing_degree == thiz->outgoing_max)
        node_grow_outgoing_vector (thiz);
    
    oe = &thiz->outgoing[thiz->outgoing_degree];
    oe->alpha = alpha;
    oe->next = next;
    thiz->outgoing_degree++;
}

/******************************************************************************
 * FUNCTION: node_assign_id
 * assign a unique ID to the node (used for debugging purpose).
******************************************************************************/
void node_assign_id (AC_NODE_t * thiz)
{
    static int unique_id = 1;
    thiz->id = unique_id ++;
}

/******************************************************************************
 * FUNCTION: node_edge_compare
 * Comparison function for qsort. see man qsort.
******************************************************************************/
int node_edge_compare (const void * l, const void * r)
{
    /* According to man page:
     * The comparison function must return an integer less than, equal to, or
     * greater than zero if the first argument is considered to be
     * respectively less than, equal to, or greater than the second. if  two
     * members compare as equal, their order in the sorted array is undefined.
     *
     * NOTE: Because edge alphabets are unique in every node we ignore
     * equivalence case.
    **/
    if ( ((struct edge *)l)->alpha >= ((struct edge *)r)->alpha )
        return 1;
    else
        return -1;
}

/******************************************************************************
 * FUNCTION: node_sort_edges
 * sorts edges alphabets.
******************************************************************************/
void node_sort_edges (AC_NODE_t * thiz)
{
    qsort ((void *)thiz->outgoing, thiz->outgoing_degree, sizeof(struct edge),
            node_edge_compare);
}

/******************************************************************************
 * FUNCTION: node_set_replacement
 * 
******************************************************************************/
int node_set_replacement (AC_NODE_t * node)
{
    unsigned int j;
    AC_PATTERN_t * pattern;
    AC_PATTERN_t * longest = NULL;
    
    if(!node->final)
        return 0;

    for (j=0; j < node->matched_patterns_num; j++)
    {
        pattern = &node->matched_patterns[j];
        
        if (pattern->rtext.astring != NULL)
        {
            if (!longest)
                longest = pattern;
            else if (pattern->ptext.length > longest->ptext.length)
                longest = pattern;
        }
    }
    
    node->to_be_replaced = longest;
    
    return longest?1:0;
}

/******************************************************************************
 * FUNCTION: node_grow_outgoing_vector
 * 
******************************************************************************/
void node_grow_outgoing_vector (AC_NODE_t * thiz)
{
    int grow_factor = (8 / (thiz->depth + 1)) + 1;
    
    /* The outgoing edges of nodes grow with different pace in different
     * depths; the shallower nodes the bigger outgoing number of nodes.
     * So for efficiency (speed & memory usage), we apply a measure to 
     * manage different growth rate.
     */
    
    if (thiz->outgoing_max == 0)
    {
        thiz->outgoing_max = grow_factor;
        thiz->outgoing = (struct edge *) malloc 
                (thiz->outgoing_max * sizeof(struct edge));
    }
    else
    {
        thiz->outgoing_max += grow_factor;
        thiz->outgoing = (struct edge *) realloc (
                thiz->outgoing, 
                thiz->outgoing_max * sizeof(struct edge));
    }
}

/******************************************************************************
 * node_grow_matchstr_vector
 * 
******************************************************************************/
void node_grow_matchstr_vector (AC_NODE_t * thiz)
{
    if (thiz->matched_patterns_max == 0)
    {
        thiz->matched_patterns_max = 1;
        thiz->matched_patterns = (AC_PATTERN_t *) malloc 
                (thiz->matched_patterns_max * sizeof(AC_PATTERN_t));
    }
    else
    {
        thiz->matched_patterns_max += 2;
        thiz->matched_patterns = (AC_PATTERN_t *) realloc (
                thiz->matched_patterns,
                thiz->matched_patterns_max * sizeof(AC_PATTERN_t));
    }
}
