/*
 * ahocorasick.c: implementation of ahocorasick library's functions
 * This file is part of multifast.
 *
    Copyright 2010-2013 Kamiar Kanani <kamiar.kanani@gmail.com>

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
#include <ctype.h>

#include "node.h"
#include "ahocorasick.h"

/* Allocation step for automata.all_nodes */
#define REALLOC_CHUNK_ALLNODES 200
#define REALLOC_PATTERN_ARRAY 50

/* Replacement buffer size */
#define REPLACEMENT_BUFFER_SIZE 2048

#if (REPLACEMENT_BUFFER_SIZE <= AC_PATTRN_MAX_LENGTH)
#error "REPLACEMENT_BUFFER_SIZE must be bigger than AC_PATTRN_MAX_LENGTH"
#endif

/* Private function prototype */
static void ac_automata_register_nodeptr
    (AC_AUTOMATA_t * thiz, AC_NODE_t * node);
static void ac_automata_register_pattern
    (AC_AUTOMATA_t * thiz, AC_PATTERN_t * patt);
static void ac_automata_union_matchstrs
    (AC_NODE_t * node);
static void ac_automata_set_failure
    (AC_AUTOMATA_t * thiz, AC_NODE_t * node, AC_ALPHABET_t * alphas);
static void ac_automata_traverse_setfailure
    (AC_AUTOMATA_t * thiz, AC_NODE_t * node, AC_ALPHABET_t * alphas);
static void ac_automata_reset (AC_AUTOMATA_t * thiz);
static void ac_automata_replacement_flush 
    (AC_AUTOMATA_t * thiz, AC_REPLACE_CALBACK_f callback, void * param);
static void ac_automata_replacement_append 
    (AC_AUTOMATA_t * thiz, AC_TEXT_t * text, AC_REPLACE_CALBACK_f callback, void * param);
static int ac_automata_mutual_factor_check (AC_AUTOMATA_t * thiz, AC_PATTERN_t * patt);

/******************************************************************************
 * FUNCTION: ac_automata_init
 * Initialize automata; allocate memories and set initial values
 * PARAMS:
******************************************************************************/
AC_AUTOMATA_t * ac_automata_init ()
{
    AC_AUTOMATA_t * thiz = (AC_AUTOMATA_t *)malloc(sizeof(AC_AUTOMATA_t));
    memset (thiz, 0, sizeof(AC_AUTOMATA_t));
    thiz->root = node_create ();
    thiz->all_nodes_max = REALLOC_CHUNK_ALLNODES;
    thiz->all_nodes = (AC_NODE_t **) malloc (thiz->all_nodes_max*sizeof(AC_NODE_t *));
    ac_automata_register_nodeptr (thiz, thiz->root);
    ac_automata_reset (thiz);
    thiz->total_patterns = 0;
    thiz->automata_open = 1;
    thiz->replacement_buffer.astring = NULL;
    thiz->replacement_buffer.length = 0;
    thiz->replacement_backlog.astring = NULL;
    thiz->replacement_backlog.length = 0;
    thiz->has_replacement = 0;
    thiz->patterns = (AC_PATTERN_t *) malloc (sizeof(AC_PATTERN_t)*REALLOC_PATTERN_ARRAY);
    thiz->patterns_maxcap = REALLOC_PATTERN_ARRAY;
    return thiz;
}

/******************************************************************************
 * FUNCTION: ac_automata_add
 * Adds pattern to the automata.
 * PARAMS:
 * AC_AUTOMATA_t * thiz: the pointer to the automata
 * AC_PATTERN_t * patt: the pointer to added pattern
 * RETUERN VALUE: AC_ERROR_t
 * the return value indicates the success or failure of adding action
******************************************************************************/
AC_STATUS_t ac_automata_add (AC_AUTOMATA_t * thiz, AC_PATTERN_t * patt)
{
    size_t i;
    AC_NODE_t * n = thiz->root;
    AC_NODE_t * next;
    AC_ALPHABET_t alpha;
    
    if(!thiz->automata_open)
        return ACERR_AUTOMATA_CLOSED;

    if (!patt->length)
        return ACERR_ZERO_PATTERN;

    if (patt->length > AC_PATTRN_MAX_LENGTH)
        return ACERR_LONG_PATTERN;
    
    if (patt->replacement.astring && 
            ac_automata_mutual_factor_check(thiz, patt))
        return ACERR_MUTUAL_FACTOR;
    
    for (i=0; i<patt->length; i++)
    {
        alpha = patt->astring[i];
        if ((next = node_find_next(n, alpha)))
        {
            n = next;
            continue;
        }
        else
        {
            next = node_create_next(n, alpha);
            next->depth = n->depth + 1;
            n = next;
            ac_automata_register_nodeptr(thiz, n);
        }
    }

    if(n->final)
        return ACERR_DUPLICATE_PATTERN;

    n->final = 1;
    node_register_matchstr(n, patt);
    ac_automata_register_pattern(thiz, patt);

    return ACERR_SUCCESS;
}

/******************************************************************************
 * FUNCTION: ac_automata_finalize
 * Locate the failure node for all nodes and collect all matched pattern for
 * every node. it also sorts outgoing edges of node, so binary search could be
 * performed on them. after calling this function the automate literally will
 * be finalized and you can not add new patterns to the automate.
 * PARAMS:
 * AC_AUTOMATA_t * thiz: the pointer to the automata
******************************************************************************/
void ac_automata_finalize (AC_AUTOMATA_t * thiz)
{
    unsigned int i, j;
    AC_ALPHABET_t alphas[AC_PATTRN_MAX_LENGTH];
    AC_NODE_t * node;

    ac_automata_traverse_setfailure (thiz, thiz->root, alphas);

    for (i=0; i < thiz->all_nodes_num; i++)
    {
        node = thiz->all_nodes[i];
        ac_automata_union_matchstrs (node);
        node_sort_edges (node);
    }
    /* Bookmark replacement pattern for faster retrieval */
    for (i=0; i < thiz->all_nodes_num; i++)
    {
        node = thiz->all_nodes[i];
        for (j=0; j < node->matched_patterns_num; j++)
        {
            if (node->matched_patterns[j].replacement.astring) {
                thiz->has_replacement++;
                node->to_be_replaced = &node->matched_patterns[j];
                /* Each final node has only one replacement pattern */
                break;
            }
        }
    }
    if (thiz->has_replacement)
    {
        thiz->replacement_buffer.astring = (AC_ALPHABET_t *) 
                malloc (REPLACEMENT_BUFFER_SIZE*sizeof(AC_ALPHABET_t));
        thiz->replacement_backlog.astring = (AC_ALPHABET_t *) 
                malloc (AC_PATTRN_MAX_LENGTH*sizeof(AC_ALPHABET_t));
        /* Backlog length can not be bigger than max pattern length */
    }
    thiz->automata_open = 0; /* do not accept patterns any more */
}

/******************************************************************************
 * FUNCTION: ac_automata_search
 * Search in the input text using the given automata. on match event it will
 * call the call-back function. and the call-back function in turn after doing
 * its job, will return an integer value to ac_automata_search(). 0 value means
 * continue search, and non-0 value means stop search and return to the caller.
 * PARAMS:
 * AC_AUTOMATA_t * thiz: the pointer to the automata
 * AC_TEXT_t * txt: the input text that must be searched
 * int keep: is the input text the successive chunk of the previous given text
 * void * param: this parameter will be send to call-back function. it is
 * useful for sending parameter to call-back function from caller function.
 * RETURN VALUE:
 * -1: failed; automata is not finalized
 *  0: success; input text was searched to the end
 *  1: success; input text was searched partially. (callback broke the loop)
******************************************************************************/
int ac_automata_search (AC_AUTOMATA_t * thiz, AC_TEXT_t * text, int keep, 
        AC_MATCH_CALBACK_f callback, void * param)
{
    size_t position;
    AC_NODE_t * current;
    AC_NODE_t * next;
    AC_MATCH_t match;

    if (thiz->automata_open)
        /* you must call ac_automata_locate_failure() first */
        return -1;
    
    thiz->text = 0;

    if (!keep)
        ac_automata_reset(thiz);
        
    position = 0;
    current = thiz->current_node;

    /* This is the main search loop.
     * it must be as lightweight as possible. */
    while (position < text->length)
    {
        if (!(next = node_findbs_next(current, text->astring[position])))
        {
            if(current->failure_node /* we are not in the root node */)
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
            match.match_num = current->matched_patterns_num;
            match.patterns = current->matched_patterns;
            /* we found a match! do call-back */
            if (callback(&match, param))
                return 1;
        }
    }

    /* save status variables */
    thiz->current_node = current;
    thiz->base_position += position;
    return 0;
}

/******************************************************************************
 * FUNCTION: ac_automata_replace
******************************************************************************/
int ac_automata_replace  (AC_AUTOMATA_t * thiz, AC_TEXT_t * text, int keep, 
        AC_REPLACE_CALBACK_f callback, void * param)
{
    size_t position = 0;
    size_t flush_index = 0;
    size_t backlog_index = 0;
    AC_NODE_t * current;
    AC_NODE_t * next;
    AC_TEXT_t ttxt;
    
    if (thiz->automata_open)
        /* you must call ac_automata_finalize() first */
        return -1;
    
    if (!thiz->has_replacement)
        return -2;
    
    thiz->text = NULL; /* Do not work in settext/findnext mode */
    
    if (!keep)
        ac_automata_reset(thiz);
    
    current = thiz->current_node;
    
    /* Main replace loop. */
    while (position < text->length)
    {
        if (!(next = node_findbs_next(current, text->astring[position])))
        {
            if(current->failure_node)
                /* We are not in the root node */
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
         * transition or due to a fail. in second case we should not replace
         * the text because it was replaced in the previous node */
        {
            if (current->to_be_replaced)
            {
                if (current->to_be_replaced->length > position)
                {
                    /* assert(thiz->replacement_bloglen > 0) */
                    /* toss away backlog */
                    thiz->replacement_backlog.length = 0;
                    ac_automata_replacement_append(thiz, &current->to_be_replaced->replacement, callback, param);
                }
                else
                {
                    /* append backlog */
                    if(thiz->replacement_backlog.length)
                    {
                        ac_automata_replacement_append(thiz, &thiz->replacement_backlog, callback, param);
                        thiz->replacement_backlog.length = 0;
                    }
                    /* append the chunk before pattern */
                    if(position - current->to_be_replaced->length > flush_index)
                    {
                        ttxt.astring = &text->astring[flush_index];
                        ttxt.length = position - current->to_be_replaced->length - flush_index;
                        ac_automata_replacement_append(thiz, &ttxt, callback, param);
                    }
                    /* append the pattern */
                    ac_automata_replacement_append(thiz, &current->to_be_replaced->replacement, callback, param);
                }
                flush_index = position;
            }
        }
    }
    
    backlog_index = (text->length > current->depth)?
        (text->length - current->depth):0;
    
    if (flush_index < backlog_index)
    {
        ttxt.astring = &text->astring[flush_index];
        ttxt.length = backlog_index - flush_index;
        ac_automata_replacement_append(thiz, &ttxt, callback, param);
    }

    /* Copy the remaining to backlog */
    memcpy((AC_ALPHABET_t *)thiz->replacement_backlog.astring, 
            &text->astring[backlog_index], text->length-backlog_index);
    thiz->replacement_backlog.length = text->length-backlog_index;
    
    /* Save status variables */
    thiz->current_node = current;
    thiz->base_position += position;
    
    return 0;
}

/******************************************************************************
 * FUNCTION: ac_automata_repflush
******************************************************************************/
void ac_automata_rflush (AC_AUTOMATA_t * thiz, 
        AC_REPLACE_CALBACK_f callback, void * param)
{
    /* append backlog */
    if(thiz->replacement_backlog.length)
    {
        ac_automata_replacement_append(thiz, &thiz->replacement_backlog, callback, param);
        thiz->replacement_backlog.length = 0;
    }
    ac_automata_replacement_flush(thiz, callback, param);
}

/******************************************************************************
 * FUNCTION: ac_automata_settext
******************************************************************************/
void ac_automata_settext (AC_AUTOMATA_t * thiz, AC_TEXT_t * text, int keep)
{
    thiz->text = text;
    if (!keep)
        ac_automata_reset(thiz);
    thiz->position = 0;
}

/******************************************************************************
 * FUNCTION: ac_automata_findnext
******************************************************************************/
AC_MATCH_t * ac_automata_findnext (AC_AUTOMATA_t * thiz)
{
    size_t position;
    AC_NODE_t * current;
    AC_NODE_t * next;
    static AC_MATCH_t match;
    
    if (thiz->automata_open)
        return 0;
    
    if (!thiz->text)
        return 0;
    
    position = thiz->position;
    current = thiz->current_node;
    match.match_num = 0;

    /* This is the main search loop.
     * it must be as lightweight as possible. */
    while (position < thiz->text->length)
    {
        if (!(next = node_findbs_next(current, thiz->text->astring[position])))
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
            match.match_num = current->matched_patterns_num;
            match.patterns = current->matched_patterns;
            break;
        }
    }

    /* save status variables */
    thiz->current_node = current;
    thiz->position = position;
    
    if (!match.match_num)
        /* if we came here due to reaching to the end of input text
         * not a loop break
         */
        thiz->base_position += position;
    
    return match.match_num?&match:0;
}

/******************************************************************************
 * FUNCTION: ac_automata_reset
 * reset the automata and make it ready for doing new search on a new text.
 * when you finished with the input text, you must reset automata state for
 * new input, otherwise it will not work.
 * PARAMS:
 * AC_AUTOMATA_t * thiz: the pointer to the automata
******************************************************************************/
void ac_automata_reset (AC_AUTOMATA_t * thiz)
{
    thiz->current_node = thiz->root;
    thiz->base_position = 0;
    thiz->replacement_buffer.length = 0;
    thiz->replacement_backlog.length = 0;
}

/******************************************************************************
 * FUNCTION: ac_automata_replacement_flush
******************************************************************************/
static void ac_automata_replacement_flush 
    (AC_AUTOMATA_t * thiz, AC_REPLACE_CALBACK_f callback, void * param)
{
    if(thiz->replacement_buffer.length==0)
        return;
    callback(&thiz->replacement_buffer, param);
    thiz->replacement_buffer.length = 0;
}

/******************************************************************************
 * FUNCTION: ac_automata_replacement_append
******************************************************************************/
static void ac_automata_replacement_append 
    (AC_AUTOMATA_t * thiz, AC_TEXT_t * text, AC_REPLACE_CALBACK_f callback, void * param)
{
    size_t remaining_bufspace = 0;
    size_t remaining_text = 0;
    size_t copy_len = 0;
    size_t flush_index = 0;
    
    while (flush_index < text->length)
    {
        remaining_bufspace = REPLACEMENT_BUFFER_SIZE - thiz->replacement_buffer.length;
        remaining_text = text->length - flush_index;
        
        copy_len = (remaining_bufspace >= remaining_text)? remaining_text:remaining_bufspace;

        memcpy(
                (void *)&thiz->replacement_buffer.astring[thiz->replacement_buffer.length],
                (void *)&text->astring[flush_index],
                copy_len*sizeof(AC_ALPHABET_t) );
        
        thiz->replacement_buffer.length += copy_len;
        flush_index += copy_len;
        
        if (thiz->replacement_buffer.length == REPLACEMENT_BUFFER_SIZE)
            ac_automata_replacement_flush(thiz, callback, param);
    }
}

/******************************************************************************
 * FUNCTION: ac_automata_mutual_factor_check
******************************************************************************/
static int ac_automata_mutual_factor_check (AC_AUTOMATA_t * thiz, AC_PATTERN_t * patt)
{
    AC_PATTERN_t *smaller, *bigger;
    size_t i, js, jb;
    
    if (!patt->replacement.astring)
        return 0;
    
    for (i = 0; i <thiz->total_patterns; i++)
    {
        if (!thiz->patterns[i].replacement.astring)
            continue;
        
        if(thiz->patterns[i].length<patt->length)
        {
            smaller = &thiz->patterns[i];
            bigger = patt;
        }
        else
        {
            bigger = &thiz->patterns[i];
            smaller = patt;
        }
        /* check for factor relationship */
        for (js=0, jb=0; jb < bigger->length && js < smaller->length; jb++)
        {
            if (bigger->astring[jb]==smaller->astring[js])
                js++;
            else
                js=0;
            
            if (js==smaller->length)
                return 1;
        }
    }
    return 0;
}

/******************************************************************************
 * FUNCTION: ac_automata_release
 * Release all allocated memories to the automata
 * PARAMS:
 * AC_AUTOMATA_t * thiz: the pointer to the automata
******************************************************************************/
void ac_automata_release (AC_AUTOMATA_t * thiz)
{
    unsigned int i;
    AC_NODE_t * n;

    for (i=0; i < thiz->all_nodes_num; i++)
    {
        n = thiz->all_nodes[i];
        node_release(n);
    }
    free(thiz->all_nodes);
    free((AC_ALPHABET_t *)thiz->replacement_buffer.astring);
    free((AC_ALPHABET_t *)thiz->replacement_backlog.astring);
    free(thiz->patterns);
    free(thiz);
}

/******************************************************************************
 * FUNCTION: ac_automata_display
 * Prints the automata to output in human readable form. it is useful for
 * debugging purpose.
 * PARAMS:
 * AC_AUTOMATA_t * thiz: the pointer to the automata
 * char repcast: 'n': print AC_REP_t as number, 's': print AC_REP_t as string
******************************************************************************/
void ac_automata_display (AC_AUTOMATA_t * thiz, char repcast)
{
    unsigned int i, j;
    AC_NODE_t * n;
    struct edge * e;
    AC_PATTERN_t sid;

    printf("---------------------------------\n");

    for (i=0; i<thiz->all_nodes_num; i++)
    {
        n = thiz->all_nodes[i];
        printf("NODE(%3d)/----fail----> NODE(%3d)\n",
                n->id, (n->failure_node)?n->failure_node->id:1);
        for (j=0; j<n->outgoing_degree; j++)
        {
            e = &n->outgoing[j];
            printf("         |----(");
            if(isgraph(e->alpha))
                printf("%c)---", e->alpha);
            else
                printf("0x%x)", e->alpha);
            printf("--> NODE(%3d)\n", e->next->id);
        }
        if (n->matched_patterns_num) {
            printf("Accepted patterns: {");
            for (j=0; j<n->matched_patterns_num; j++)
            {
                sid = n->matched_patterns[j];
                if(j) printf(", ");
                switch (repcast)
                {
                case 'n':
                    printf("%ld", sid.rep.number);
                    break;
                case 's':
                    printf("%s", sid.rep.stringy);
                    break;
                }
            }
            printf("}\n");
        }
        printf("---------------------------------\n");
    }
}

/******************************************************************************
 * FUNCTION: ac_automata_register_nodeptr
 * Adds the node pointer to all_nodes.
******************************************************************************/
static void ac_automata_register_nodeptr (AC_AUTOMATA_t * thiz, AC_NODE_t * node)
{
    if(thiz->all_nodes_num >= thiz->all_nodes_max)
    {
        thiz->all_nodes_max += REALLOC_CHUNK_ALLNODES;
        thiz->all_nodes = realloc
                (thiz->all_nodes, thiz->all_nodes_max*sizeof(AC_NODE_t *));
    }
    thiz->all_nodes[thiz->all_nodes_num++] = node;
}

/******************************************************************************
 * FUNCTION: ac_automata_register_pattern
******************************************************************************/
static void ac_automata_register_pattern
    (AC_AUTOMATA_t * thiz, AC_PATTERN_t * patt)
{
    thiz->patterns_maxcap += REALLOC_PATTERN_ARRAY;
    if (thiz->total_patterns==REALLOC_PATTERN_ARRAY)
        thiz->patterns = (AC_PATTERN_t *) realloc 
                (thiz->patterns, thiz->patterns_maxcap*sizeof(AC_PATTERN_t));
    
    thiz->patterns[thiz->total_patterns] = *patt;
    thiz->total_patterns++;
}

/******************************************************************************
 * FUNCTION: ac_automata_union_matchstrs
 * Collect accepted patterns of the node. the accepted patterns consist of the
 * node's own accepted pattern plus accepted patterns of its failure node.
******************************************************************************/
static void ac_automata_union_matchstrs (AC_NODE_t * node)
{
    unsigned int i;
    AC_NODE_t * m = node;
    
    while ((m = m->failure_node))
    {
        for (i=0; i < m->matched_patterns_num; i++)
            node_register_matchstr(node, &(m->matched_patterns[i]));

        if (m->final)
            node->final = 1;
    }
    // TODO : sort matched_patterns? is that necessary? I don't think so.
}

/******************************************************************************
 * FUNCTION: ac_automata_set_failure
 * find failure node for the given node.
******************************************************************************/
static void ac_automata_set_failure
    (AC_AUTOMATA_t * thiz, AC_NODE_t * node, AC_ALPHABET_t * alphas)
{
    unsigned int i, j;
    AC_NODE_t * m;

    for (i=1; i < node->depth; i++)
    {
        m = thiz->root;
        for (j=i; j < node->depth && m; j++)
            m = node_find_next (m, alphas[j]);
        if (m)
        {
            node->failure_node = m;
            break;
        }
    }
    if (!node->failure_node)
        node->failure_node = thiz->root;
}

/******************************************************************************
 * FUNCTION: ac_automata_traverse_setfailure
 * Traverse all automata nodes using DFS (Depth First Search), meanwhile it set
 * the failure node for every node it passes through. this function must be
 * called after adding last pattern to automata. i.e. after calling this you
 * can not add further pattern to automata.
******************************************************************************/
static void ac_automata_traverse_setfailure
    (AC_AUTOMATA_t * thiz, AC_NODE_t * node, AC_ALPHABET_t * alphas)
{
    unsigned int i;
    AC_NODE_t * next;

    for (i=0; i < node->outgoing_degree; i++)
    {
        alphas[node->depth] = node->outgoing[i].alpha;
        next = node->outgoing[i].next;

        /* At every node look for its failure node */
        ac_automata_set_failure (thiz, next, alphas);

        /* Recursively call itself to traverse all nodes */
        ac_automata_traverse_setfailure (thiz, next, alphas);
    }
}
