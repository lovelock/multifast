/*
 * replace.c: contains functions for pattern replacement
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
#include <ctype.h>

#include "node.h"
#include "ahocorasick.h"
#include "replace.h"


/* Private functions */
static void acatm_replace_booknominee (AC_AUTOMATA_t * thiz, 
        AC_PATTERN_t * nom, size_t pos);

static void acatm_repdata_appendtext (AC_AUTOMATA_t * thiz, 
        AC_TEXT_t * text, AC_REPLACE_CALBACK_f callback, void * param);

static void acatm_repdata_appendfactor (AC_AUTOMATA_t * thiz, 
        size_t from, size_t to, 
        AC_REPLACE_CALBACK_f callback, void * param);

static void acatm_repdata_do_replace (AC_AUTOMATA_t * thiz, size_t to_position,
        AC_REPLACE_CALBACK_f callback, void * param);

static void acatm_repdata_savetobacklog (AC_AUTOMATA_t * thiz, size_t to_position_r,
        AC_REPLACE_CALBACK_f callback, void * param);

static void acatm_repdata_flush (AC_AUTOMATA_t * thiz, 
        AC_REPLACE_CALBACK_f callback, void * param);

void acatm_repdata_init (AC_AUTOMATA_t * thiz);
void acatm_repdata_reset (AC_AUTOMATA_t * thiz);
void acatm_repdata_release (AC_AUTOMATA_t * thiz);
void acatm_repdata_finalize (AC_AUTOMATA_t * thiz);


/******************************************************************************
 * FUNCTION: ac_automata_replace
******************************************************************************/
void acatm_repdata_init (AC_AUTOMATA_t * thiz)
{
    thiz->repdata.buffer.astring = NULL;
    thiz->repdata.buffer.length = 0;
    thiz->repdata.backlog.astring = NULL;
    thiz->repdata.backlog.length = 0;
    thiz->repdata.has_replacement = 0;
    thiz->repdata.curser = 0;
    
    thiz->repdata.noms = (struct replacement_nominee *) malloc 
            (sizeof(struct replacement_nominee)*REPLACEMENT_NOMINEE_V_SIZE);
    thiz->repdata.noms_maxcap = REPLACEMENT_NOMINEE_V_SIZE;
    thiz->repdata.noms_count = 0;
}

/******************************************************************************
 * FUNCTION: acatm_repdata_finalize
******************************************************************************/
void acatm_repdata_finalize (AC_AUTOMATA_t * thiz)
{
    unsigned int i;
    AC_NODE_t * node;
    
    /* Bookmark replacement pattern for faster retrieval */
    for (i=0; i < thiz->all_nodes_num; i++)
    {
        node = thiz->all_nodes[i];
        thiz->repdata.has_replacement += node_set_replacement (node);
    }
    
    if (thiz->repdata.has_replacement)
    {
        thiz->repdata.buffer.astring = (AC_ALPHABET_t *) 
                malloc (REPLACEMENT_BUFFER_SIZE*sizeof(AC_ALPHABET_t));
        
        thiz->repdata.backlog.astring = (AC_ALPHABET_t *) 
                malloc (AC_PATTRN_MAX_LENGTH*sizeof(AC_ALPHABET_t));
        
        /* Backlog length is not bigger than the max pattern length */
    }
}

/******************************************************************************
 * FUNCTION: acatm_repdata_reset
******************************************************************************/
void acatm_repdata_reset (AC_AUTOMATA_t * thiz)
{
    thiz->repdata.buffer.length = 0;
    thiz->repdata.backlog.length = 0;
    thiz->repdata.curser = 0;
    thiz->repdata.noms_count = 0;
}

/******************************************************************************
 * FUNCTION: acatm_repdata_release
******************************************************************************/
void acatm_repdata_release (AC_AUTOMATA_t * thiz)
{
    free((AC_ALPHABET_t *)thiz->repdata.buffer.astring);
    free((AC_ALPHABET_t *)thiz->repdata.backlog.astring);
    free(thiz->repdata.noms);
}

/******************************************************************************
 * FUNCTION: ac_automata_replacement_flush
******************************************************************************/
void acatm_repdata_flush (AC_AUTOMATA_t * thiz, AC_REPLACE_CALBACK_f callback, void * param)
{
    callback(&thiz->repdata.buffer, param);
    thiz->repdata.buffer.length = 0;
}

/******************************************************************************
 * FUNCTION: ac_automata_add_nominee
******************************************************************************/
void acatm_replace_booknominee (AC_AUTOMATA_t * thiz, 
        AC_PATTERN_t * nom, size_t current_pos)
{
    struct replacement_nominee * prev_nom;
    size_t newsize;
    
    if (nom == NULL)
        return;
    
    while (thiz->repdata.noms_count > 0)
    {
        prev_nom = &thiz->repdata.noms[thiz->repdata.noms_count-1];
        
        if (current_pos - nom->ptext.length <= prev_nom->position - prev_nom->pattern->ptext.length)
            /* Remove that nominee, because it is a factor of the new nominee */
            thiz->repdata.noms_count --;
        else
            break;
    }
    
    if (thiz->repdata.noms_count >= thiz->repdata.noms_maxcap)
    {
        /* Extend the vector */
        thiz->repdata.noms_maxcap += REPLACEMENT_NOMINEE_V_SIZE;
        newsize = thiz->repdata.noms_maxcap * sizeof(struct replacement_nominee);
        thiz->repdata.noms = (struct replacement_nominee *) 
                realloc (thiz->repdata.noms, newsize);
    }

    thiz->repdata.noms[thiz->repdata.noms_count].pattern = nom;
    thiz->repdata.noms[thiz->repdata.noms_count].position = current_pos;
    thiz->repdata.noms_count ++;
}

/******************************************************************************
 * FUNCTION: ac_automata_replacement_append_pattern
******************************************************************************/
void acatm_repdata_appendtext 
    (AC_AUTOMATA_t * thiz, AC_TEXT_t * text, AC_REPLACE_CALBACK_f callback, void * param)
{
    size_t remaining_bufspace = 0;
    size_t remaining_text = 0;
    size_t copy_len = 0;
    size_t copy_index = 0;
    
    while (copy_index < text->length)
    {
        remaining_bufspace = REPLACEMENT_BUFFER_SIZE - thiz->repdata.buffer.length;
        remaining_text = text->length - copy_index;
        
        copy_len = (remaining_bufspace >= remaining_text)? remaining_text:remaining_bufspace;

        memcpy(
                (void *)&thiz->repdata.buffer.astring[thiz->repdata.buffer.length],
                (void *)&text->astring[copy_index],
                copy_len*sizeof(AC_ALPHABET_t) );
        
        thiz->repdata.buffer.length += copy_len;
        copy_index += copy_len;
        
        if (thiz->repdata.buffer.length == REPLACEMENT_BUFFER_SIZE)
            acatm_repdata_flush(thiz, callback, param);
    }
}

/******************************************************************************
 * FUNCTION: acatm_repdata_appendfactor
******************************************************************************/
void acatm_repdata_appendfactor 
    (AC_AUTOMATA_t * thiz, size_t from, size_t to, 
        AC_REPLACE_CALBACK_f callback, void * param)
{
    AC_TEXT_t * instr = thiz->text;
    AC_TEXT_t factor;
    size_t backlog_base_pos;
    
    if (to < from)
        return;
    
    if (thiz->base_position <= from)
    {
        /* The backlog located in the input text part */
        factor.astring = &instr->astring[from - thiz->base_position];
        factor.length = to - from;
        acatm_repdata_appendtext(thiz, &factor, callback, param);
    }
    else
    {
        backlog_base_pos = thiz->base_position - thiz->repdata.backlog.length;
        if (from < backlog_base_pos)
            return; /* shouldn't come here */
        
        if (to < thiz->base_position)
        {
            /* The backlog located in the backlog part */
            factor.astring = &thiz->repdata.backlog.astring[from - backlog_base_pos];
            factor.length = to - from;
            acatm_repdata_appendtext (thiz, &factor, callback, param);
        }
        else
        {
            /* The factor is divided between backlog and input text */
            
            /* The backlog part */
            factor.astring = &thiz->repdata.backlog.astring[from - backlog_base_pos];
            factor.length = thiz->repdata.backlog.length - from + backlog_base_pos;
            acatm_repdata_appendtext (thiz, &factor, callback, param);
            
            /* The input text part */
            factor.astring = instr->astring;
            factor.length = to - thiz->base_position;
            acatm_repdata_appendtext (thiz, &factor, callback, param);
        }
    }
}

/******************************************************************************
 * FUNCTION: ac_automata_replacement_flush
******************************************************************************/
void acatm_repdata_savetobacklog (AC_AUTOMATA_t * thiz, size_t bg_pos, 
        AC_REPLACE_CALBACK_f callback, void * param)
{
    size_t bg_pos_r; /* relative backlog position */
    AC_TEXT_t * instr = thiz->text;
    
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
            &thiz->repdata.backlog.astring[thiz->repdata.backlog.length], 
            &instr->astring[bg_pos_r], 
            instr->length - bg_pos_r );

    thiz->repdata.backlog.length += instr->length - bg_pos_r;
}

/******************************************************************************
 * FUNCTION: acatm_repdata_do_replace
******************************************************************************/
void acatm_repdata_do_replace (AC_AUTOMATA_t * thiz, size_t to_position,
        AC_REPLACE_CALBACK_f callback, void * param)
{
    unsigned int index;
    struct replacement_nominee * nom;
    
    if (to_position < thiz->base_position)
        return;
    
    /* Replace the candidate patterns */
    if (thiz->repdata.noms_count > 0)
    {
        for (index=0; index < thiz->repdata.noms_count; index++)
        {
            nom = &thiz->repdata.noms[index];
            
            if (to_position < nom->position)
                break;
            
            /* Append the space before pattern */
            acatm_repdata_appendfactor (thiz,
                    thiz->repdata.curser, /* from */
                    nom->position - nom->pattern->ptext.length, /* to */
                    callback, param);
            
            /* Append the replacement instead of the pattern */
            acatm_repdata_appendtext(thiz, 
                    &nom->pattern->rtext, 
                    callback, param);
            
            thiz->repdata.curser = nom->position;
        }
        thiz->repdata.noms_count -= index;
        
        if (thiz->repdata.noms_count && index) {
            /* Shift the array to the left to eliminate the consumed nominees 
             * TODO: use a circular queue
             */
            memcpy (&thiz->repdata.noms[0], 
                    &thiz->repdata.noms[index], 
                    thiz->repdata.noms_count*sizeof(struct replacement_nominee));
        }
    }
    
    /* Append the chunk between the last pattern and to_position */
    if (to_position > thiz->repdata.curser)
    {
        acatm_repdata_appendfactor (thiz, thiz->repdata.curser, to_position, 
                callback, param);
        
        thiz->repdata.curser = to_position;
    }
    
    if (thiz->base_position <= thiz->repdata.curser)
    {
        /* we consume the whole backlog or none of it */
        thiz->repdata.backlog.length = 0;
    }
}

/******************************************************************************
 * FUNCTION: ac_automata_replace
******************************************************************************/
int ac_automata_replace (AC_AUTOMATA_t * thiz, AC_TEXT_t * instr, 
        AC_REPLACE_CALBACK_f callback, void * param)
{
    AC_NODE_t * current;
    AC_NODE_t * next;
    
    size_t position_r = 0;  /* Relative current position in the input string */
    size_t backlog_pos = 0; /* Relative backlog position in the input string */
    
    if (thiz->automata_open)
        return -1; /* ac_automata_finalize() must be called first */
    
    if (!thiz->repdata.has_replacement)
        return -2; /* Automata doesn't have any to-be-replaced pattern */
    
    thiz->text = instr; /* Record the input string in a public variable 
                         * for convenience */
    
    current = thiz->current_node;
    
    /* Main replace loop: 
     * Find patterns and bookmark them 
     */
    while (position_r < instr->length)
    {
        if (!(next = node_findbs_next(current, instr->astring[position_r])))
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
            acatm_replace_booknominee (thiz, 
                    current->to_be_replaced, 
                    thiz->base_position + position_r);
        }
    }
    
    /* 
     * At the end of input chunk, if the tail of the chunk is a prefix of a
     * pattern, then we must keep it in the backlog buffer and wait for the 
     * next chunk to decide about it. */
    
    backlog_pos = thiz->base_position + instr->length - current->depth;
    
    /* Now replace the patterns up to the backlog_pos point */
    acatm_repdata_do_replace (thiz, backlog_pos, callback, param);
    
    /* Save the remaining to the backlog buffer */
    acatm_repdata_savetobacklog (thiz, backlog_pos, callback, param);
    
    /* Save status variables */
    thiz->current_node = current;
    thiz->base_position += position_r;
    
    return 0;
}

/******************************************************************************
 * FUNCTION: ac_automata_rflush
******************************************************************************/
void ac_automata_rflush (AC_AUTOMATA_t * thiz, 
        AC_REPLACE_CALBACK_f callback, void * param)
{
    acatm_repdata_do_replace (thiz, thiz->base_position, 
            callback, param);
    
    acatm_repdata_flush (thiz, callback, param);
    acatm_repdata_reset (thiz);
    thiz->current_node = thiz->root;
    thiz->base_position = 0;
}
