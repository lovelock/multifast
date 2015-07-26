/*
 * replace.c: implements pattern replacement functions
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


/* Private functions */
static void acatm_repdata_booknominee (AC_AUTOMATA_t * thiz, 
        struct replacement_nominee * nnom);

static void acatm_repdata_appendtext (AC_AUTOMATA_t * thiz, 
        AC_TEXT_t * text);

static void acatm_repdata_appendfactor (AC_AUTOMATA_t * thiz, 
        size_t from, size_t to);

static void acatm_repdata_do_replace (AC_AUTOMATA_t * thiz, 
        size_t to_position);

static void acatm_repdata_savetobacklog (AC_AUTOMATA_t * thiz, 
        size_t to_position_r);

static void acatm_repdata_flush (AC_AUTOMATA_t * thiz);

static void acatm_repdata_grow_noms_array (AC_AUTOMATA_t * thiz);

static void acatm_repdata_add_nominee (AC_AUTOMATA_t * thiz, 
        struct replacement_nominee * new_nom);

/* Publics */
void acatm_repdata_init (AC_AUTOMATA_t * thiz);
void acatm_repdata_reset (AC_AUTOMATA_t * thiz);
void acatm_repdata_release (AC_AUTOMATA_t * thiz);
void acatm_repdata_finalize (AC_AUTOMATA_t * thiz);


/******************************************************************************
 * FUNCTION: acatm_repdata_init
******************************************************************************/
void acatm_repdata_init (AC_AUTOMATA_t * thiz)
{
    struct replacement_date * rd = &thiz->repdata;
    
    rd->buffer.astring = NULL;
    rd->buffer.length = 0;
    rd->backlog.astring = NULL;
    rd->backlog.length = 0;
    rd->has_replacement = 0;
    rd->curser = 0;
    
    rd->noms = NULL;
    rd->noms_capacity = 0;
    rd->noms_size = 0;
    
    rd->replace_mode = ACA_REPLACE_MODE_DEFAULT;
}

/******************************************************************************
 * FUNCTION: acatm_repdata_finalize
******************************************************************************/
void acatm_repdata_finalize (AC_AUTOMATA_t * thiz)
{
    unsigned int i;
    AC_NODE_t * node;
    struct replacement_date * rd = &thiz->repdata;
    
    /* Bookmark replacement pattern for faster retrieval */
    for (i=0; i < thiz->nodes_size; i++)
    {
        node = thiz->nodes[i];
        rd->has_replacement += node_set_replacement (node);
    }
    
    if (rd->has_replacement)
    {
        rd->buffer.astring = (AC_ALPHABET_t *) 
                malloc (REPLACEMENT_BUFFER_SIZE * sizeof(AC_ALPHABET_t));
        
        rd->backlog.astring = (AC_ALPHABET_t *) 
                malloc (AC_PATTRN_MAX_LENGTH * sizeof(AC_ALPHABET_t));
        
        /* Backlog length is not bigger than the max pattern length */
    }
}

/******************************************************************************
 * FUNCTION: acatm_repdata_reset
******************************************************************************/
void acatm_repdata_reset (AC_AUTOMATA_t * thiz)
{
    struct replacement_date * rd = &thiz->repdata;
    
    rd->buffer.length = 0;
    rd->backlog.length = 0;
    rd->curser = 0;
    rd->noms_size = 0;
    rd->replace_mode = ACA_REPLACE_MODE_DEFAULT;
}

/******************************************************************************
 * FUNCTION: acatm_repdata_release
******************************************************************************/
void acatm_repdata_release (AC_AUTOMATA_t * thiz)
{
    struct replacement_date * rd = &thiz->repdata;
    
    free((AC_ALPHABET_t *)rd->buffer.astring);
    free((AC_ALPHABET_t *)rd->backlog.astring);
    free(rd->noms);
}

/******************************************************************************
 * FUNCTION: acatm_repdata_flush
******************************************************************************/
void acatm_repdata_flush (AC_AUTOMATA_t * thiz)
{
    struct replacement_date * rd = &thiz->repdata;
    
    rd->cbf(&rd->buffer, rd->user);
    rd->buffer.length = 0;
}

/**
 * Extends the nominees array
 * @param thiz
 *************************************************************************** */
void acatm_repdata_grow_noms_array (AC_AUTOMATA_t * thiz)
{
    const size_t grow_factor = 128;
    struct replacement_date *rd = &thiz->repdata;
    
    if (rd->noms_capacity == 0)
    {
        rd->noms_capacity = grow_factor;
        rd->noms = (struct replacement_nominee *) malloc 
                (rd->noms_capacity * sizeof(struct replacement_nominee));
        rd->noms_size = 0;
    }
    else
    {
        rd->noms_capacity += grow_factor;
        rd->noms = (struct replacement_nominee *) realloc (rd->noms, 
                rd->noms_capacity * sizeof(struct replacement_nominee));
    }
}

/**
 * 
 * @param thiz
 * @param new_nom
 *************************************************************************** */
void acatm_repdata_add_nominee 
    (AC_AUTOMATA_t * thiz, struct replacement_nominee * new_nom)
{
    struct replacement_date *rd = &thiz->repdata;
    struct replacement_nominee *nomp;
    
    /* Extend the vector if needed */
    if (rd->noms_size == rd->noms_capacity)
        acatm_repdata_grow_noms_array (thiz);
    
    /* Add the new nominee to the end */
    nomp = &rd->noms[rd->noms_size];
    nomp->pattern = new_nom->pattern;
    nomp->position = new_nom->position;
    rd->noms_size ++;
}

/******************************************************************************
 * FUNCTION: acatm_repdata_booknominee
******************************************************************************/
void acatm_repdata_booknominee (AC_AUTOMATA_t * thiz, 
        struct replacement_nominee * new_nom)
{
    struct replacement_nominee *prev_nom;
    struct replacement_date *rd = &thiz->repdata;
    size_t prev_start_pos, prev_end_pos, new_start_pos;
    
    if (new_nom->pattern == NULL)
        return; /* This is not a to-be-replaced pattern; ignore it. */
    
    new_start_pos = new_nom->position - new_nom->pattern->ptext.length;
    
    switch (rd->replace_mode) 
    {
        case ACA_REPLACE_MODE_LAZY:
            
            if (new_start_pos < rd->curser)
                return; /* Ignore the new nominee, because it overlaps with the 
                         * previous replacement */
            
            if (rd->noms_size > 0)
            {
                prev_nom = &rd->noms[rd->noms_size-1];
                prev_end_pos = prev_nom->position;

                if (new_start_pos < prev_end_pos)
                    return;
            }
            break;
        
        case ACA_REPLACE_MODE_DEFAULT:
        case ACA_REPLACE_MODE_NORMAL:
        default:
            
            while (rd->noms_size > 0)
            {
                prev_nom = &rd->noms[rd->noms_size-1];
                prev_start_pos = 
                        prev_nom->position - prev_nom->pattern->ptext.length;
                prev_end_pos = prev_nom->position;
                
                if (new_start_pos <= prev_start_pos)
                    rd->noms_size --;  /* Remove that nominee, because it is a
                                         * factor of the new nominee */
                else
                    break;  /* Get out the loop and add the new nominee */
            }
            break;
    }
    
    acatm_repdata_add_nominee(thiz, new_nom);
}

/******************************************************************************
 * FUNCTION: acatm_repdata_appendtext
******************************************************************************/
void acatm_repdata_appendtext (AC_AUTOMATA_t * thiz, AC_TEXT_t * text)
{
    struct replacement_date *rd = &thiz->repdata;
    size_t remaining_bufspace = 0;
    size_t remaining_text = 0;
    size_t copy_len = 0;
    size_t copy_index = 0;
    
    while (copy_index < text->length)
    {
        remaining_bufspace = REPLACEMENT_BUFFER_SIZE - rd->buffer.length;
        remaining_text = text->length - copy_index;
        
        copy_len = (remaining_bufspace >= remaining_text)? 
            remaining_text:remaining_bufspace;

        memcpy((void *)&rd->buffer.astring[rd->buffer.length],
                (void *)&text->astring[copy_index],
                copy_len*sizeof(AC_ALPHABET_t));
        
        rd->buffer.length += copy_len;
        copy_index += copy_len;
        
        if (rd->buffer.length == REPLACEMENT_BUFFER_SIZE)
            acatm_repdata_flush(thiz);
    }
}

/******************************************************************************
 * FUNCTION: acatm_repdata_appendfactor
******************************************************************************/
void acatm_repdata_appendfactor (AC_AUTOMATA_t * thiz, size_t from, size_t to)
{
    struct replacement_date *rd = &thiz->repdata;
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
        acatm_repdata_appendtext(thiz, &factor);
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
            acatm_repdata_appendtext (thiz, &factor);
        }
        else
        {
            /* The factor is divided between backlog and input text */
            
            /* The backlog part */
            factor.astring = &rd->backlog.astring[from - backlog_base_pos];
            factor.length = rd->backlog.length - from + backlog_base_pos;
            acatm_repdata_appendtext (thiz, &factor);
            
            /* The input text part */
            factor.astring = instr->astring;
            factor.length = to - thiz->base_position;
            acatm_repdata_appendtext (thiz, &factor);
        }
    }
}

/******************************************************************************
 * FUNCTION: acatm_repdata_savetobacklog
******************************************************************************/
void acatm_repdata_savetobacklog (AC_AUTOMATA_t * thiz, size_t bg_pos)
{
    size_t bg_pos_r; /* relative backlog position */
    AC_TEXT_t *instr = thiz->text;
    struct replacement_date * rd = &thiz->repdata;
    
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

/******************************************************************************
 * FUNCTION: acatm_repdata_do_replace
******************************************************************************/
void acatm_repdata_do_replace (AC_AUTOMATA_t * thiz, size_t to_position)
{
    unsigned int index;
    struct replacement_nominee *nom;
    struct replacement_date *rd = &thiz->repdata;
    
    if (to_position < thiz->base_position)
        return;
    
    /* Replace the candidate patterns */
    if (rd->noms_size > 0)
    {
        for (index=0; index < rd->noms_size; index++)
        {
            nom = &rd->noms[index];
            
            if (to_position <= (nom->position - nom->pattern->ptext.length))
                break;
            
            /* Append the space before pattern */
            acatm_repdata_appendfactor (thiz,
                    rd->curser, /* from */
                    nom->position - nom->pattern->ptext.length /* to */);
            
            /* Append the replacement instead of the pattern */
            acatm_repdata_appendtext(thiz, 
                    &nom->pattern->rtext);
            
            rd->curser = nom->position;
        }
        rd->noms_size -= index;
        
        if (rd->noms_size && index) {
            /* Shift the array to the left to eliminate the consumed nominees 
             * TODO: use a circular queue
             */
            memcpy (&rd->noms[0], 
                    &rd->noms[index], 
                    rd->noms_size * sizeof(struct replacement_nominee));
        }
    }
    
    /* Append the chunk between the last pattern and to_position */
    if (to_position > rd->curser)
    {
        acatm_repdata_appendfactor (thiz, rd->curser, to_position);
        
        rd->curser = to_position;
    }
    
    if (thiz->base_position <= rd->curser)
    {
        /* we consume the whole backlog or none of it */
        rd->backlog.length = 0;
    }
}

/******************************************************************************
 * FUNCTION: ac_automata_replace
******************************************************************************/
int ac_automata_replace (AC_AUTOMATA_t * thiz, AC_TEXT_t * instr, 
        ACA_REPLACE_MODE_t mode, AC_REPLACE_CALBACK_f callback, void * param)
{
    AC_NODE_t * current;
    AC_NODE_t * next;
    struct replacement_nominee nom;
    
    size_t position_r = 0;  /* Relative current position in the input string */
    size_t backlog_pos = 0; /* Relative backlog position in the input string */
    
    if (thiz->automata_open)
        return -1; /* ac_automata_finalize() must be called first */
    
    if (!thiz->repdata.has_replacement)
        return -2; /* Automata doesn't have any to-be-replaced pattern */
    
    thiz->repdata.cbf = callback;
    thiz->repdata.user = param;
    thiz->repdata.replace_mode = mode;
    
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
            nom.pattern = current->to_be_replaced;
            nom.position = thiz->base_position + position_r;
            
            acatm_repdata_booknominee (thiz, &nom);
        }
    }
    
    /* 
     * At the end of input chunk, if the tail of the chunk is a prefix of a
     * pattern, then we must keep it in the backlog buffer and wait for the 
     * next chunk to decide about it. */
    
    backlog_pos = thiz->base_position + instr->length - current->depth;
    
    /* Now replace the patterns up to the backlog_pos point */
    acatm_repdata_do_replace (thiz, backlog_pos);
    
    /* Save the remaining to the backlog buffer */
    acatm_repdata_savetobacklog (thiz, backlog_pos);
    
    /* Save status variables */
    thiz->current_node = current;
    thiz->base_position += position_r;
    
    return 0;
}

/******************************************************************************
 * FUNCTION: ac_automata_flush
******************************************************************************/
void ac_automata_flush (AC_AUTOMATA_t * thiz)
{
    acatm_repdata_do_replace (thiz, thiz->base_position);
    acatm_repdata_flush (thiz);
    acatm_repdata_reset (thiz);
    thiz->current_node = thiz->root;
    thiz->base_position = 0;
}
