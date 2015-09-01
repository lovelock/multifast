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
#include "mpool.h"

/* Privates */

static void ac_automata_set_failure
    (AC_NODE_t *node, AC_ALPHABET_t *alphas);

static void ac_automata_traverse_setfailure 
    (AC_NODE_t *node, AC_ALPHABET_t *prefix);

static void ac_automata_traverse_action 
    (AC_NODE_t *node, void(*func)(AC_NODE_t *), int top_down);

static void ac_automata_reset 
    (AC_AUTOMATA_t *thiz);

static int ac_automata_match_handler 
    (AC_MATCH_t * matchp, void * param);

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
AC_AUTOMATA_t *ac_automata_init (void)
{
    AC_AUTOMATA_t *thiz = (AC_AUTOMATA_t *) malloc (sizeof(AC_AUTOMATA_t));
    thiz->mp = mpool_create(0);
    
    thiz->root = node_create (thiz);
    
    thiz->patterns_count = 0;
    
    acatm_repdata_init (thiz);
    ac_automata_reset (thiz);    
    thiz->text = NULL;
    thiz->position = 0;
    
    thiz->wm = AC_WORKING_MODE_SEARCH;
    thiz->automata_open = 1;
    
    return thiz;
}

/**
 * @brief Adds pattern to the automata.
 * 
 * @param Thiz pointer to the automata
 * @param Patt pointer to the pattern
 * @param copy should automata make a copy of patten strings or not, if not, 
 * then user must keep the strings valid for the life-time of the automata. If
 * the pattern are available in the user program then call the function with 
 * copy = 0 and do not waste memory.
 * 
 * @return The return value indicates the success or failure of adding action
 *****************************************************************************/
AC_STATUS_t ac_automata_add (AC_AUTOMATA_t *thiz, AC_PATTERN_t *patt, int copy)
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
        }
    }
    
    if(n->final)
        return ACERR_DUPLICATE_PATTERN;
    
    n->final = 1;
    node_accept_pattern (n, patt, copy);
    thiz->patterns_count++;
    
    return ACERR_SUCCESS;
}

/**
 * @brief Finalizes the preprocessing stage and gets the automata ready
 * 
 * Locates the failure node for all nodes and collects all matched 
 * pattern for each node. It also sorts outgoing edges of node, so binary 
 * search could be performed on them. After calling this function the automate 
 * will be finalized and you can not add new patterns to the automate.
 * 
 * @param thiz pointer to the automata
 *****************************************************************************/
void ac_automata_finalize (AC_AUTOMATA_t *thiz)
{
    AC_ALPHABET_t prefix[AC_PATTRN_MAX_LENGTH]; 
    
    /* 'prefix' defined here, because ac_automata_traverse_setfailure() calls
     * itself recursively */
    ac_automata_traverse_setfailure (thiz->root, prefix);
    
    ac_automata_traverse_action (thiz->root, node_collect_matches, 1);
    acatm_repdata_finalize (thiz);
    
    thiz->automata_open = 0; /* Do not accept patterns any more */
}

/**
 * @brief Search in the input text using the given automata.
 * 
 * @param thiz pointer to the automata
 * @param text input text to be searched
 * @param keep indicated that if the input text the successive chunk of the 
 * previous given text or not
 * @param callback when a match occurs this function will be called. The 
 * call-back function in turn after doing its job, will return an integer 
 * value, 0 means continue search, and non-0 value means stop search and return 
 * to the caller.
 * @param user this parameter will be send to the call-back function
 * 
 * @return
 * -1:  failed; automata is not finalized
 *  0:  success; input text was searched to the end
 *  1:  success; input text was searched partially. (callback broke the loop)
 *****************************************************************************/
int ac_automata_search (AC_AUTOMATA_t *thiz, AC_TEXT_t *text, int keep, 
        AC_MATCH_CALBACK_f callback, void *user)
{
    size_t position;
    AC_NODE_t *current;
    AC_NODE_t *next;
    AC_MATCH_t match;

    if (thiz->automata_open)
        return -1;  /* Automata must be finalized first. */
    
    if (thiz->wm == AC_WORKING_MODE_FINDNEXT)
        position = thiz->position;
    else
        position = 0;
    
    current = thiz->current_node;
    
    if (!keep)
        ac_automata_reset (thiz);
    
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
            if (callback(&match, user))
            {
                if (thiz->wm == AC_WORKING_MODE_FINDNEXT) {
                    thiz->position = position;
                    thiz->current_node = current;
                }
                return 1;
            }
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
 * @param thiz The pointer to the automata
 * @param text The text to be searched. The owner of the text is the 
 * calling program and no local copy is made, so it must be valid until you 
 * have done with it.
 * @param keep Indicates that if the given text is the sequel of the previous
 * one or not; 1: it is, 0: it is not
 *****************************************************************************/
void ac_automata_settext (AC_AUTOMATA_t *thiz, AC_TEXT_t *text, int keep)
{
    if (!keep)
        ac_automata_reset (thiz);
    
    thiz->text = text;
    thiz->position = 0;
}

/**
 * @brief finds the next match in the input text which is set by _settext()
 * 
 * @param thiz The pointer to the automata
 * @return A pointer to the matched structure
 *****************************************************************************/
AC_MATCH_t *ac_automata_findnext (AC_AUTOMATA_t *thiz)
{
    static AC_MATCH_t match;
    
    thiz->wm = AC_WORKING_MODE_FINDNEXT;
    match.size = 0;
    
    ac_automata_search (thiz, thiz->text, 1, 
            ac_automata_match_handler, (void *)&match);
    
    thiz->wm = AC_WORKING_MODE_SEARCH;
    
    return match.size ? &match : NULL;
}

/**
 * @brief Release all allocated memories to the automata
 * 
 * @param thiz pointer to the automata
 *****************************************************************************/
void ac_automata_release (AC_AUTOMATA_t *thiz)
{
    /* It must be called with a 0 top-down parameter */
    ac_automata_traverse_action (thiz->root, node_release_vectors, 0);
    
    acatm_repdata_release (thiz);
    mpool_free(thiz->mp);
    free(thiz);
}

/**
 * @brief Prints the automata to output in human readable form. It is useful 
 * for debugging purpose.
 * 
 * @param thiz pointer to the automata
 *****************************************************************************/
void ac_automata_display (AC_AUTOMATA_t *thiz)
{
    ac_automata_traverse_action (thiz->root, node_display, 1);
}

/**
 * @brief the match handler function used in _findnext function
 * 
 * @param matchp
 * @param param
 * @return 
 *****************************************************************************/
static int ac_automata_match_handler (AC_MATCH_t * matchp, void * param)
{
    AC_MATCH_t * mp = (AC_MATCH_t *)param;
    mp->position = matchp->position;
    mp->patterns = matchp->patterns;
    mp->size = matchp->size;
    return 1;
}

/**
 * @brief reset the automata and make it ready for doing new search
 * 
 * @param thiz pointer to the automata
 *****************************************************************************/
static void ac_automata_reset (AC_AUTOMATA_t *thiz)
{
    thiz->current_node = thiz->root;
    thiz->base_position = 0;
    acatm_repdata_reset (thiz);
}

/**
 * @brief Finds and bookmarks the failure transition for the given node.
 * 
 * @param node the node pointer
 * @param prefix The array that contain the prefix that leads the path from
 * root the the node.
 *****************************************************************************/
static void ac_automata_set_failure
    (AC_NODE_t *node, AC_ALPHABET_t *prefix)
{
    size_t i, j;
    AC_NODE_t *n;
    AC_NODE_t *root = node->atm->root;
    
    if (node == root)
        return; /* Failure transition is not defined for the root */
    
    for (i = 1; i < node->depth; i++)
    {
        n = root;
        for (j = i; j < node->depth && n; j++)
            n = node_find_next (n, prefix[j]);
        if (n)
        {
            node->failure_node = n;
            break;
        }
    }
    
    if (!node->failure_node)
        node->failure_node = root;
}

/**
 * @brief Sets the failure transition node for all nodes
 * 
 * Traverse all automata nodes using DFS (Depth First Search), meanwhile it set
 * the failure node for every node it passes through. this function is called 
 * after adding last pattern to automata.
 * 
 * @param node The pointer to the root node
 * @param prefix The array that contain the prefix that leads the path from
 * root the the node
 *****************************************************************************/
static void ac_automata_traverse_setfailure 
    (AC_NODE_t *node, AC_ALPHABET_t *prefix)
{
    size_t i;
    
    /* In each node, look for its failure node */
    ac_automata_set_failure (node, prefix);
    
    for (i = 0; i < node->outgoing_size; i++)
    {
        prefix[node->depth] = node->outgoing[i].alpha; /* Make the prefix */
        
        /* Recursively call itself to traverse all nodes */
        ac_automata_traverse_setfailure (node->outgoing[i].next, prefix);
    }
}

/**
 * @brief Traverses the automata using DFS method and applies the 
 * given @param func on all nodes. At top level it should be called by 
 * sending the the root node.
 * 
 * @param node Pointer to automata root node
 * @param func The function that must be applied to all nodes
 * @param top_down Indicates that if the action should be applied to the note
 * itself and then to its children or vise versa.
 *****************************************************************************/
static void ac_automata_traverse_action 
    (AC_NODE_t *node, void(*func)(AC_NODE_t *), int top_down)
{
    size_t i;
    
    if (top_down)
        func (node);
    
    for (i = 0; i < node->outgoing_size; i++)
        /* Recursively call itself to traverse all nodes */
        ac_automata_traverse_action (node->outgoing[i].next, func, top_down);
    
    if (!top_down)
        func (node);
}
