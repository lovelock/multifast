/*
 * replace.c: Implements the replacement functionality
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

#include <string.h>

#include "node.h"
#include "ahocorasick.h"


/* Privates */

static void mf_repdata_do_replace 
    (AC_TRIE_t *thiz, size_t to_position);

static void mf_repdata_booknominee 
    (AC_TRIE_t *thiz, struct mf_replacement_nominee *new_nom);

static void mf_repdata_push_nominee 
    (AC_TRIE_t *thiz, struct mf_replacement_nominee *new_nom);

static void mf_repdata_grow_noms_array 
    (AC_TRIE_t *thiz);

static void mf_repdata_appendtext 
    (AC_TRIE_t *thiz, AC_TEXT_t *text);

static void mf_repdata_appendfactor 
    (AC_TRIE_t *thiz, size_t from, size_t to);

static void mf_repdata_savetobacklog 
    (AC_TRIE_t *thiz, size_t to_position_r);

static void mf_repdata_flush 
    (AC_TRIE_t *thiz);

static unsigned int mf_repdata_bookreplacements 
    (ACT_NODE_t *node);

/* Publics */

void mf_repdata_init (AC_TRIE_t *thiz);
void mf_repdata_reset (AC_TRIE_t *thiz);
void mf_repdata_release (AC_TRIE_t *thiz);
void mf_repdata_finalize (AC_TRIE_t *thiz);


/**
 * @brief Initializes the replacement data part of the trie
 * 
 * @param thiz
 *****************************************************************************/
void mf_repdata_init (AC_TRIE_t *thiz)
{
    struct mf_replacement_date *rd = &thiz->repdata;
    
    rd->buffer.astring = NULL;
    rd->buffer.length = 0;
    rd->backlog.astring = NULL;
    rd->backlog.length = 0;
    rd->has_replacement = 0;
    rd->curser = 0;
    
    rd->noms = NULL;
    rd->noms_capacity = 0;
    rd->noms_size = 0;
    
    rd->replace_mode = MF_REPLACE_MODE_DEFAULT;
}

/**
 * @brief Performs finalization tasks on replacement data.
 * Must be called when finalizing the trie itself
 * 
 * @param thiz
 *****************************************************************************/
void mf_repdata_finalize (AC_TRIE_t *thiz)
{
    struct mf_replacement_date *rd = &thiz->repdata;
    
    /* Bookmark replacement pattern for faster retrieval */
    rd->has_replacement = mf_repdata_bookreplacements (thiz->root);
    
    if (rd->has_replacement)
    {
        rd->buffer.astring = (AC_ALPHABET_t *) 
                malloc (MF_REPLACEMENT_BUFFER_SIZE * sizeof(AC_ALPHABET_t));
        
        rd->backlog.astring = (AC_ALPHABET_t *) 
                malloc (AC_PATTRN_MAX_LENGTH * sizeof(AC_ALPHABET_t));
        
        /* Backlog length is not bigger than the max pattern length */
    }
}

/**
 * @brief Bookmarks the to-be-replaced patterns for all nodes
 * 
 * @param node
 * @return 
 *****************************************************************************/
static unsigned int mf_repdata_bookreplacements (ACT_NODE_t *node)
{
    size_t i;
    unsigned int ret;
    
    ret = node_book_replacement (node);
    
    for (i = 0; i < node->outgoing_size; i++)
    {
        /* Recursively call itself to traverse all nodes */
        ret += mf_repdata_bookreplacements (node->outgoing[i].next);
    }
    
    return ret;
}

/**
 * @brief Resets the replacement data and prepares it for a new operation
 * 
 * @param thiz
 *****************************************************************************/
void mf_repdata_reset (AC_TRIE_t *thiz)
{
    struct mf_replacement_date *rd = &thiz->repdata;
    
    rd->buffer.length = 0;
    rd->backlog.length = 0;
    rd->curser = 0;
    rd->noms_size = 0;
}

/**
 * @brief Release the allocated resources to the replacement data
 * 
 * @param thiz
 *****************************************************************************/
void mf_repdata_release (AC_TRIE_t *thiz)
{
    struct mf_replacement_date *rd = &thiz->repdata;
    
    free((AC_ALPHABET_t *)rd->buffer.astring);
    free((AC_ALPHABET_t *)rd->backlog.astring);
    free(rd->noms);
}

/**
 * @brief Flushes out all the available stuff in the buffer to the user
 * 
 * @param thiz
 *****************************************************************************/
static void mf_repdata_flush (AC_TRIE_t *thiz)
{
    struct mf_replacement_date *rd = &thiz->repdata;
    
    rd->cbf(&rd->buffer, rd->user);
    rd->buffer.length = 0;
}

/**
 * @brief Extends the nominees array
 * 
 * @param thiz
 *****************************************************************************/
static void mf_repdata_grow_noms_array (AC_TRIE_t *thiz)
{
    const size_t grow_factor = 128;
    struct mf_replacement_date *rd = &thiz->repdata;
    
    if (rd->noms_capacity == 0)
    {
        rd->noms_capacity = grow_factor;
        rd->noms = (struct mf_replacement_nominee *) malloc 
                (rd->noms_capacity * sizeof(struct mf_replacement_nominee));
        rd->noms_size = 0;
    }
    else
    {
        rd->noms_capacity += grow_factor;
        rd->noms = (struct mf_replacement_nominee *) realloc (rd->noms, 
                rd->noms_capacity * sizeof(struct mf_replacement_nominee));
    }
}

/**
 * @brief Adds the nominee to the end of the nominee list
 * 
 * @param thiz
 * @param new_nom
 *****************************************************************************/
static void mf_repdata_push_nominee 
    (AC_TRIE_t *thiz, struct mf_replacement_nominee *new_nom)
{
    struct mf_replacement_date *rd = &thiz->repdata;
    struct mf_replacement_nominee *nomp;
    
    /* Extend the vector if needed */
    if (rd->noms_size == rd->noms_capacity)
        mf_repdata_grow_noms_array (thiz);
    
    /* Add the new nominee to the end */
    nomp = &rd->noms[rd->noms_size];
    nomp->pattern = new_nom->pattern;
    nomp->position = new_nom->position;
    rd->noms_size ++;
}

/**
 * @brief Tries to add the nominee to the end of the nominee list
 * 
 * @param thiz
 * @param new_nom
 *****************************************************************************/
static void mf_repdata_booknominee (AC_TRIE_t *thiz, 
        struct mf_replacement_nominee *new_nom)
{
    struct mf_replacement_nominee *prev_nom;
    struct mf_replacement_date *rd = &thiz->repdata;
    size_t prev_start_pos, prev_end_pos, new_start_pos;
    
    if (new_nom->pattern == NULL)
        return; /* This is not a to-be-replaced pattern; ignore it. */
    
    new_start_pos = new_nom->position - new_nom->pattern->ptext.length;
    
    switch (rd->replace_mode) 
    {
        case MF_REPLACE_MODE_LAZY:
            
            if (new_start_pos < rd->curser)
                return; /* Ignore the new nominee, because it overlaps with the 
                         * previous replacement */
            
            if (rd->noms_size > 0)
            {
                prev_nom = &rd->noms[rd->noms_size - 1];
                prev_end_pos = prev_nom->position;

                if (new_start_pos < prev_end_pos)
                    return;
            }
            break;
        
        case MF_REPLACE_MODE_DEFAULT:
        case MF_REPLACE_MODE_NORMAL:
        default:
            
            while (rd->noms_size > 0)
            {
                prev_nom = &rd->noms[rd->noms_size - 1];
                prev_start_pos = 
                        prev_nom->position - prev_nom->pattern->ptext.length;
                prev_end_pos = prev_nom->position;
                
                if (new_start_pos <= prev_start_pos)
                    rd->noms_size--;    /* Remove that nominee, because it is a
                                         * factor of the new nominee */
                else
                    break;  /* Get out the loop and add the new nominee */
            }
            break;
    }
    
    mf_repdata_push_nominee(thiz, new_nom);
}

/**
 * @brief Append the given text to the output buffer
 * 
 * @param thiz
 * @param text
 *****************************************************************************/
static void mf_repdata_appendtext (AC_TRIE_t *thiz, AC_TEXT_t *text)
{
    struct mf_replacement_date *rd = &thiz->repdata;
    size_t remaining_bufspace = 0;
    size_t remaining_text = 0;
    size_t copy_len = 0;
    size_t copy_index = 0;
    
    while (copy_index < text->length)
    {
        remaining_bufspace = MF_REPLACEMENT_BUFFER_SIZE - rd->buffer.length;
        remaining_text = text->length - copy_index;
        
        copy_len = (remaining_bufspace >= remaining_text)? 
            remaining_text : remaining_bufspace;
        
        memcpy((void *)&rd->buffer.astring[rd->buffer.length],
                (void *)&text->astring[copy_index],
                copy_len * sizeof(AC_ALPHABET_t));
        
        rd->buffer.length += copy_len;
        copy_index += copy_len;
        
        if (rd->buffer.length == MF_REPLACEMENT_BUFFER_SIZE)
            mf_repdata_flush(thiz);
    }
}

/**
 * @brief Append a factor of the current text to the output buffer
 *  
 * @param thiz
 * @param from
 * @param to
 *****************************************************************************/
static void mf_repdata_appendfactor 
    (AC_TRIE_t *thiz, size_t from, size_t to)
{
    struct mf_replacement_date *rd = &thiz->repdata;
    AC_TEXT_t *instr = thiz->text;
    AC_TEXT_t factor;
    size_t backlog_base_pos;
    
    if (to < from)
        return;
    
    if (thiz->base_position <= from)
    {
        /* The backlog located in the input text part */
        factor.astring = &instr->astring[from - thiz->base_position];
        factor.length = to - from;
        mf_repdata_appendtext(thiz, &factor);
    }
    else
    {
        backlog_base_pos = thiz->base_position - rd->backlog.length;
        if (from < backlog_base_pos)
            return; /* shouldn't come here */
        
        if (to < thiz->base_position)
        {
            /* The backlog located in the backlog part */
            factor.astring = &rd->backlog.astring[from - backlog_base_pos];
            factor.length = to - from;
            mf_repdata_appendtext (thiz, &factor);
        }
        else
        {
            /* The factor is divided between backlog and input text */
            
            /* The backlog part */
            factor.astring = &rd->backlog.astring[from - backlog_base_pos];
            factor.length = rd->backlog.length - from + backlog_base_pos;
            mf_repdata_appendtext (thiz, &factor);
            
            /* The input text part */
            factor.astring = instr->astring;
            factor.length = to - thiz->base_position;
            mf_repdata_appendtext (thiz, &factor);
        }
    }
}

/**
 * @brief Saves the backlog part of the current text to the backlog buffer. The
 * backlog part is the part after @p bg_pos
 * 
 * @param thiz
 * @param bg_pos backlog position
 *****************************************************************************/
static void mf_repdata_savetobacklog (AC_TRIE_t *thiz, size_t bg_pos)
{
    size_t bg_pos_r; /* relative backlog position */
    AC_TEXT_t *instr = thiz->text;
    struct mf_replacement_date *rd = &thiz->repdata;
    
    if (thiz->base_position < bg_pos)
        bg_pos_r = bg_pos - thiz->base_position;
    else
        bg_pos_r = 0; /* the whole input text must go to backlog */
    
    if (instr->length == bg_pos_r)
        return; /* Nothing left for the backlog */
    
    if (instr->length < bg_pos_r)
        return; /* unexpected : assert (instr->length >= bg_pos_r) */
    
    /* Copy the part after bg_pos_r to the backlog buffer */
    memcpy( (AC_ALPHABET_t *)
            &rd->backlog.astring[rd->backlog.length], 
            &instr->astring[bg_pos_r], 
            instr->length - bg_pos_r );

    rd->backlog.length += instr->length - bg_pos_r;
}

/**
 * @brief Perform replacement operations on the non-backlog part of the current
 * text. In-range nominees will be replaced the original pattern and the result 
 * will be pushed to the output buffer.
 * 
 * @param thiz
 * @param to_position
 *****************************************************************************/
static void mf_repdata_do_replace (AC_TRIE_t *thiz, size_t to_position)
{
    unsigned int index;
    struct mf_replacement_nominee *nom;
    struct mf_replacement_date *rd = &thiz->repdata;
    
    if (to_position < thiz->base_position)
        return;
    
    /* Replace the candidate patterns */
    if (rd->noms_size > 0)
    {
        for (index = 0; index < rd->noms_size; index++)
        {
            nom = &rd->noms[index];
            
            if (to_position <= (nom->position - nom->pattern->ptext.length))
                break;
            
            /* Append the space before pattern */
            mf_repdata_appendfactor (thiz, rd->curser, /* from */
                    nom->position - nom->pattern->ptext.length /* to */);
            
            /* Append the replacement instead of the pattern */
            mf_repdata_appendtext(thiz, &nom->pattern->rtext);
            
            rd->curser = nom->position;
        }
        rd->noms_size -= index;
        
        /* Shift the array to the left to eliminate the consumed nominees */
        if (rd->noms_size && index)
        {
            memcpy (&rd->noms[0], &rd->noms[index], 
                    rd->noms_size * sizeof(struct mf_replacement_nominee));
            /* TODO: implement a circular queue */
        }
    }
    
    /* Append the chunk between the last pattern and to_position */
    if (to_position > rd->curser)
    {
        mf_repdata_appendfactor (thiz, rd->curser, to_position);
        
        rd->curser = to_position;
    }
    
    if (thiz->base_position <= rd->curser)
    {
        /* we consume the whole backlog or none of it */
        rd->backlog.length = 0;
    }
}

/**
 * @brief Replaces the patterns in the given text with their correspondence
 * replacement in the A.C. Trie
 * 
 * @param thiz
 * @param instr
 * @param mode
 * @param callback
 * @param param
 * @return 
 *****************************************************************************/
int multifast_replace (AC_TRIE_t *thiz, AC_TEXT_t *instr, 
        MF_REPLACE_MODE_t mode, MF_REPLACE_CALBACK_f callback, void *param)
{
    ACT_NODE_t *current;
    ACT_NODE_t *next;
    struct mf_replacement_nominee nom;
    
    size_t position_r = 0;  /* Relative current position in the input string */
    size_t backlog_pos = 0; /* Relative backlog position in the input string */
    
    if (thiz->trie_open)
        return -1; /* ac_trie_finalize() must be called first */
    
    if (!thiz->repdata.has_replacement)
        return -2; /* Trie doesn't have any to-be-replaced pattern */
    
    thiz->repdata.cbf = callback;
    thiz->repdata.user = param;
    thiz->repdata.replace_mode = mode;
    
    thiz->text = instr; /* Record the input string in a public variable 
                         * for convenience */
    
    current = thiz->last_node;
    
    /* Main replace loop: 
     * Find patterns and bookmark them 
     */
    while (position_r < instr->length)
    {
        if (!(next = node_find_next_bs(current, instr->astring[position_r])))
        {
            /* Failed to follow a pattern */
            if(current->failure_node)
                current = current->failure_node;
            else
                position_r++;
        }
        else
        {
            current = next;
            position_r++;
        }
        
        if (current->final && next)
        {
            /* Bookmark nominee patterns for replacement */
            nom.pattern = current->to_be_replaced;
            nom.position = thiz->base_position + position_r;
            
            mf_repdata_booknominee (thiz, &nom);
        }
    }
    
    /* 
     * At the end of input chunk, if the tail of the chunk is a prefix of a
     * pattern, then we must keep it in the backlog buffer and wait for the 
     * next chunk to decide about it. */
    
    backlog_pos = thiz->base_position + instr->length - current->depth;
    
    /* Now replace the patterns up to the backlog_pos point */
    mf_repdata_do_replace (thiz, backlog_pos);
    
    /* Save the remaining to the backlog buffer */
    mf_repdata_savetobacklog (thiz, backlog_pos);
    
    /* Save status variables */
    thiz->last_node = current;
    thiz->base_position += position_r;
    
    return 0;
}

/**
 * @brief Flushes the remaining data back to the user and ends the replacement
 * operation.
 * 
 * @param thiz
 *****************************************************************************/
void multifast_flush (AC_TRIE_t *thiz)
{
    mf_repdata_do_replace (thiz, thiz->base_position);
    mf_repdata_flush (thiz);
    mf_repdata_reset (thiz);
    thiz->last_node = thiz->root;
    thiz->base_position = 0;
}
