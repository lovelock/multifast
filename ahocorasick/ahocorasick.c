/*
 * ahocorasick.c: Implements the A. C. Automata functionalities
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
#include <stdlib.h>
#include <string.h>

#include "node.h"
#include "ahocorasick.h"


/* Privates */

static void ac_automata_register_nodeptr 
    (AC_AUTOMATA_t *thiz, AC_NODE_t *node);

static void ac_automata_register_pattern 
    (AC_AUTOMATA_t *thiz, AC_PATTERN_t *patt);

static void ac_automata_set_failure 
    (AC_AUTOMATA_t *thiz, AC_NODE_t *node, AC_ALPHABET_t *alphas);

static void ac_automata_traverse_setfailure 
    (AC_AUTOMATA_t *thiz, AC_NODE_t *node, AC_ALPHABET_t *alphas);

static void ac_automata_traverse_collect 
    (AC_AUTOMATA_t *thiz, AC_NODE_t *node);

static void ac_automata_reset 
    (AC_AUTOMATA_t *thiz);

/* Friends */

extern void acatm_repdata_init (AC_AUTOMATA_t *thiz);
extern void acatm_repdata_reset (AC_AUTOMATA_t *thiz);
extern void acatm_repdata_release (AC_AUTOMATA_t *thiz);
extern void acatm_repdata_finalize (AC_AUTOMATA_t *thiz);


/**
 * @brief Initializes the automata; allocates memories and sets initial values
 * 
 * @return 
 *****************************************************************************/
AC_AUTOMATA_t *ac_automata_init ()
{
    AC_AUTOMATA_t *thiz = (AC_AUTOMATA_t *) malloc (sizeof(AC_AUTOMATA_t));
    
    thiz->root = node_create ();
    
    thiz->nodes = NULL;
    thiz->nodes_capacity = 0;
    thiz->nodes_size = 0;
    
    thiz->patterns = NULL;
    thiz->patterns_capacity = 0;
    thiz->patterns_size = 0;
    
    ac_automata_register_nodeptr (thiz, thiz->root);
    
    acatm_repdata_init (thiz);
    ac_automata_reset (thiz);    
    thiz->text = NULL;
    thiz->position = 0;
    
    thiz->automata_open = 1;
    
    return thiz;
}

/**
 * @brief Adds pattern to the automata.
 * 
 * @param Thiz pointer to the automata
 * @param Patt pointer to the pattern
 * 
 * @return The return value indicates the success or failure of adding action
 *****************************************************************************/
AC_STATUS_t ac_automata_add (AC_AUTOMATA_t *thiz, AC_PATTERN_t *patt)
{
    size_t i;
    AC_NODE_t *n = thiz->root;
    AC_NODE_t *next;
    AC_ALPHABET_t alpha;
    
    if(!thiz->automata_open)
        return ACERR_AUTOMATA_CLOSED;

    if (!patt->ptext.length)
        return ACERR_ZERO_PATTERN;

    if (patt->ptext.length > AC_PATTRN_MAX_LENGTH)
        return ACERR_LONG_PATTERN;
    
    for (i = 0; i < patt->ptext.length; i++)
    {
        alpha = patt->ptext.astring[i];
        if ((next = node_find_next (n, alpha)))
        {
            n = next;
            continue;
        }
        else
        {
            next = node_create_next (n, alpha);
            next->depth = n->depth + 1;
            n = next;
            ac_automata_register_nodeptr (thiz, n);
        }
    }

    if(n->final)
        return ACERR_DUPLICATE_PATTERN;

    n->final = 1;
    node_accept_pattern (n, patt);
    ac_automata_register_pattern (thiz, patt);

    return ACERR_SUCCESS;
}

/**
 * @brief Finalizes the preprocessing stage of building automata
 * 
 * Locates the failure node for all nodes and collects all matched 
 * pattern for each node. It also sorts outgoing edges of node, so binary 
 * search could be performed on them. After calling this function the automate 
 * literally will be finalized and you can not add new patterns to the 
 * automate.
 * 
 * @param thiz pointer to the automata
 *****************************************************************************/
void ac_automata_finalize (AC_AUTOMATA_t *thiz)
{
    AC_ALPHABET_t alphas[AC_PATTRN_MAX_LENGTH]; 
    
    /* 'alphas' defined here, because ac_automata_traverse_setfailure() calls
     * itself recursively */
    
    ac_automata_traverse_setfailure (thiz, thiz->root, alphas);
    
    ac_automata_traverse_collect (thiz, thiz->root);
    
    acatm_repdata_finalize (thiz);
    
    thiz->automata_open = 0; /* Do not accept patterns any more */
}

/**
 * @brief Search in the input text using the given automata.
 * 
 * @param thiz pointer to the automata
 * @param text input text that must be searched
 * @param keep is the input text the successive chunk of the previous given text
 * @param callback when a match occurs this function will be called. The 
 * call-back function in turn after doing its job, will return an integer 
 * value: 0 means continue search, and non-0 value means stop search and return 
 * to the caller.
 * @param param this parameter will be send to call-back function
 * 
 * @return
 * -1:  failed; automata is not finalized
 *  0:  success; input text was searched to the end
 *  1:  success; input text was searched partially. (callback broke the loop)
 *****************************************************************************/
int ac_automata_search (AC_AUTOMATA_t *thiz, AC_TEXT_t *text, int keep, 
        AC_MATCH_CALBACK_f callback, void *param)
{
    size_t position;
    AC_NODE_t *current;
    AC_NODE_t *next;
    AC_MATCH_t match;

    if (thiz->automata_open)
        /* You must finalize the automata first. */
        return -1;
    
    thiz->text = 0;

    if (!keep)
        ac_automata_reset (thiz);
        
    position = 0;
    current = thiz->current_node;

    /* This is the main search loop.
     * It must be kept as lightweight as possible.
     */
    while (position < text->length)
    {
        if (!(next = node_find_next_bs (current, text->astring[position])))
        {
            if(current->failure_node /* We are not in the root node */)
                current = current->failure_node;
            else
                position++;
        }
        else
        {
            current = next;
            position++;
        }

        if (current->final && next)
        /* We check 'next' to find out if we have come here after a alphabet
         * transition or due to a fail transition. in second case we should not 
         * report match, because it has already been reported */
        {
            /* Found a match! */
            match.position = position + thiz->base_position;
            match.size = current->matched_size;
            match.patterns = current->matched;
            
            /* Do call-back */
            if (callback(&match, param))
                return 1;
        }
    }

    /* Save status variables */
    thiz->current_node = current;
    thiz->base_position += position;
    
    return 0;
}

/**
 * @brief sets the input text to be searched by a function call to _findnext()
 * 
 * @param thiz
 * @param text
 * @param keep
 *****************************************************************************/
void ac_automata_settext (AC_AUTOMATA_t *thiz, AC_TEXT_t *text, int keep)
{
    thiz->text = text;
    if (!keep)
        ac_automata_reset (thiz);
    thiz->position = 0;
}

/**
 * @brief finds the next match in the input text which is set by _settext()
 * 
 * @param thiz
 * @return 
 *****************************************************************************/
AC_MATCH_t *ac_automata_findnext (AC_AUTOMATA_t *thiz)
{
    size_t position;
    AC_NODE_t *current;
    AC_NODE_t *next;
    static AC_MATCH_t match;
    AC_TEXT_t *txt = thiz->text;
    
    if (thiz->automata_open)
        return 0;
    
    if (!txt)
        return 0;
    
    position = thiz->position;
    current = thiz->current_node;
    match.size = 0;

    /* This is the main search loop.
     * it must be as lightweight as possible. */
    while (position < txt->length)
    {
        if (!(next = node_find_next_bs (current, txt->astring[position])))
        {
            if (current->failure_node /* we are not in the root node */)
                current = current->failure_node;
            else
                position++;
        }
        else
        {
            current = next;
            position++;
        }

        if (current->final && next)
        /* We check 'next' to find out if we came here after a alphabet
         * transition or due to a fail. in second case we should not report
         * matching because it was reported in previous node */
        {
            match.position = position + thiz->base_position;
            match.size = current->matched_size;
            match.patterns = current->matched;
            break;
        }
    }

    /* save status variables */
    thiz->current_node = current;
    thiz->position = position;
    
    if (!match.size)
        /* We came here due to reaching to the end of the input text,
         * not a loop break */
        thiz->base_position += position;
    
    return match.size ? &match : 0;
}

/**
 * @brief reset the automata and make it ready for doing new search
 * 
 * @param thiz pointer to the automata
 *****************************************************************************/
void ac_automata_reset (AC_AUTOMATA_t *thiz)
{
    thiz->current_node = thiz->root;
    thiz->base_position = 0;
    acatm_repdata_reset (thiz);
}

/**
 * @brief Release all allocated memories to the automata
 * 
 * @param thiz pointer to the automata
 *****************************************************************************/
void ac_automata_release (AC_AUTOMATA_t *thiz)
{
    size_t i;
    AC_NODE_t *n;

    acatm_repdata_release (thiz);
    
    for (i = 0; i < thiz->nodes_size; i++)
    {
        n = thiz->nodes[i];
        node_release (n);
    }
    if (thiz->nodes) 
        free(thiz->nodes);
    if (thiz->patterns)
        free(thiz->patterns);
    free(thiz);
}

/**
 * @brief Prints the automata to output in human readable form. It is useful 
 * for debugging purpose.
 * 
 * @param thiz pointer to the automata
 * @param repcast 'n': prints title as a number, 's': print title as a string
 *****************************************************************************/
void ac_automata_display (AC_AUTOMATA_t *thiz, AC_TITLE_DISPOD_t dispmod)
{
    node_display (thiz->root, dispmod);
}

/**
 * @brief Adds the node pointer to the node array
 * 
 * @param thiz pointer to the automata
 * @param node
 *****************************************************************************/
static void ac_automata_register_nodeptr (AC_AUTOMATA_t *thiz, AC_NODE_t *node)
{
    const size_t grow_factor = 200;
    
    if (thiz->nodes_capacity == 0)
    {
        thiz->nodes_capacity = grow_factor;
        thiz->nodes = (AC_NODE_t **) malloc 
                (thiz->nodes_capacity * sizeof(AC_NODE_t *));
        thiz->nodes_size = 0;
    }
    else if (thiz->nodes_size == thiz->nodes_capacity)
    {
        thiz->nodes_capacity += grow_factor;
        thiz->nodes = realloc
                (thiz->nodes, thiz->nodes_capacity * sizeof(AC_NODE_t *));
    }
    thiz->nodes[thiz->nodes_size++] = node;
}

/**
 * @brief Adds pattern to the pattern array
 * 
 * @param thiz
 * @param patt
 *****************************************************************************/
static void ac_automata_register_pattern
    (AC_AUTOMATA_t *thiz, AC_PATTERN_t *patt)
{
    const size_t grow_factor = 50;
    
    if (thiz->patterns_capacity == 0)
    {
        thiz->patterns_capacity = grow_factor;
        thiz->patterns = (AC_PATTERN_t *) malloc 
                (thiz->patterns_capacity * sizeof(AC_PATTERN_t));
        thiz->patterns_size = 0;
    }
    else if (thiz->patterns_size == thiz->patterns_capacity)
    {
        thiz->patterns_capacity += grow_factor;
        thiz->patterns = (AC_PATTERN_t *) realloc (thiz->patterns, 
                thiz->patterns_capacity * sizeof(AC_PATTERN_t));
    }
    thiz->patterns[thiz->patterns_size++] = *patt;
}

/**
 * @brief Finds and bookmarks the failure transition for the given node.
 * 
 * @param thiz
 * @param node
 * @param alphas
 *****************************************************************************/
static void ac_automata_set_failure
    (AC_AUTOMATA_t *thiz, AC_NODE_t *node, AC_ALPHABET_t *alphas)
{
    size_t i, j;
    AC_NODE_t *n;

    for (i = 1; i < node->depth; i++)
    {
        n = thiz->root;
        for (j = i; j < node->depth && n; j++)
            n = node_find_next (n, alphas[j]);
        if (n)
        {
            node->failure_node = n;
            break;
        }
    }
    
    if (!node->failure_node)
        node->failure_node = thiz->root;
}

/**
 * @brief Sets the failure transition node for all nodes
 * 
 * Traverse all automata nodes using DFS (Depth First Search), meanwhile it set
 * the failure node for every node it passes through. this function is called 
 * after adding last pattern to automata.
 * 
 * @param thiz
 * @param node
 * @param alphas
 *****************************************************************************/
static void ac_automata_traverse_setfailure
    (AC_AUTOMATA_t *thiz, AC_NODE_t *node, AC_ALPHABET_t *alphas)
{
    size_t i;
    AC_NODE_t *next;
    
    for (i = 0; i < node->outgoing_size; i++)
    {
        alphas[node->depth] = node->outgoing[i].alpha;
        next = node->outgoing[i].next;
        
        /* At every node look for its failure node */
        ac_automata_set_failure (thiz, next, alphas);
        
        /* Recursively call itself to traverse all nodes */
        ac_automata_traverse_setfailure (thiz, next, alphas);
    }
}

/**
 * @brief traverses through all nodes by DFS and collect the matched patterns
 * of failure nodes
 * 
 * @param thiz
 * @param node
 *****************************************************************************/
static void ac_automata_traverse_collect (AC_AUTOMATA_t *thiz, AC_NODE_t *node)
{
    size_t i;
    
    node_collect_matches (node);
    node_sort_edges (node);
    
    for (i = 0; i < node->outgoing_size; i++)
    {        
        /* Recursively call itself to traverse all nodes */
        ac_automata_traverse_collect (thiz, node->outgoing[i].next);
    }
}
