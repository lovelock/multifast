/*
 * multifast.c:
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
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "pattern.h"
#include "walker.h"
#include "multifast.h"

#define STREAM_BUFFER_SIZE 4096

// Program configuration options
struct program_config configuration = {0, WORKING_MODE_SEARCH, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

char * get_outfile_name (const char * dir, const char * file);
int mkpath(const char * path, mode_t mode);

//*****************************************************************************
// FUNCTION: main
//*****************************************************************************

int main (int argc, char ** argv)
{
    int clopt; // Command line option
    AC_AUTOMATA_t * paca; // Aho-Corasick automata pointer
    int i;
    char *infpath, *outfpath;
    
    // Command line argument control
    if(argc < 4)
    {
        print_usage (argv[0]);
        exit(1);
    }

    // Read Command line options
    while ((clopt = getopt(argc, argv, "P:R:lndxrpfivh")) != -1)
    {
        switch (clopt)
        {
        case 'P':
            configuration.pattern_file_name = optarg;
            break;
        case 'R':
            configuration.w_mode = WORKING_MODE_REPLACE;
            configuration.output_dir = optarg;
            break;
        case 'l':
            configuration.lazy_search = 1;
            break;
        case 'n':
            configuration.output_show_item = 1;
            break;
        case 'd':
            configuration.output_show_dpos = 1;
            break;
        case 'x':
            configuration.output_show_xpos = 1;
            break;
        case 'r':
            configuration.output_show_reprv = 1;
            break;
        case 'p':
            configuration.output_show_pattern = 1;
            break;
        case 'f':
            configuration.find_first = 1;
            break;
        case 'i':
            configuration.insensitive = 1;
            break;
        case 'v':
            configuration.verbosity = 1;
            break;
        case '?':
        case 'h':
        default:
            print_usage (argv[0]);
            exit(1);
        }
    }
    configuration.input_files = argv + optind;
    configuration.input_files_num = argc - optind;

    /* Correct and normalize the command-line options */
    
    if (configuration.pattern_file_name == NULL ||
            configuration.input_files[0] == NULL)
    {
        print_usage (argv[0]);
        exit(1);
    }
    if (!(configuration.output_show_item || configuration.output_show_dpos ||
            configuration.output_show_xpos || configuration.output_show_reprv
            || configuration.output_show_pattern))
    {
        configuration.output_show_xpos = 1;
        configuration.output_show_reprv = 1;
    }

    if (configuration.lazy_search && 
            configuration.w_mode != WORKING_MODE_REPLACE)
    {
        fprintf (stderr, "Switch -l is not applicable. "
                "It operates in replace mode. Use switch -R\n");
        exit(1);
    }
    
    // Show program title
    if(configuration.verbosity)
    {
        printf("Loading Patterns From '%s'\n", configuration.pattern_file_name);
    }

    // Load patterns
    if (pattern_load (configuration.pattern_file_name, &paca))
        exit(1);
    
    if(configuration.verbosity)
        printf("Total Patterns: %lu\n", paca->total_patterns);

    if (configuration.w_mode==WORKING_MODE_SEARCH)
    {
        if (paca->total_patterns == 0)
        {
            printf ("No pattern to search!\n");
            return 1;
        }

        // Search
        if (opendir(configuration.input_files[0]))
            // if it is a directory
        {
            if (configuration.verbosity)
                printf("Searching directory %s:\n", configuration.input_files[0]);
            walker_find (configuration.input_files[0], paca);
        } 
        else // if it is not a directory
        {
            if (configuration.verbosity)
                printf("Searching %ld files\n", configuration.input_files_num);
            for (i=0; i<configuration.input_files_num; i++)
                search_file(configuration.input_files[i], paca);
        }
    }
    else if (configuration.w_mode==WORKING_MODE_REPLACE)
    {
        /* Replace Mode */
        if (paca->repdata.has_replacement==0)
        {
            printf ("No pattern was specified for replacement in the pattern file!\n");
            return 1;
        }
        
        for (i=0; i<configuration.input_files_num; i++)
        {
            infpath = configuration.input_files[i];
            outfpath = get_outfile_name (configuration.output_dir, infpath);
            
            if (!replace_file (paca, infpath, outfpath))
                printf("Successfully replaced: %s >> %s\n", infpath, outfpath);
        }
    }
    
    // Release
    // pattern_release ();
    // ac_automata_release(paca);

    return 0;
}

//*****************************************************************************
// FUNCTION: search_file
//*****************************************************************************

int search_file (const char * filename, AC_AUTOMATA_t * paca)
{
    int fd_input; // Input file descriptor
    static AC_TEXT_t intext; // input text
    static AC_ALPHABET_t in_stream_buffer[STREAM_BUFFER_SIZE];
    static struct match_param mparm; // Match parameters
    ssize_t num_read; // Number of byes read from input file

    intext.astring = in_stream_buffer;

    // Open input file
    if (!strcmp(configuration.input_files[0], "-"))
    {
        fd_input = 0; // read from stdin
    }
    else if ((fd_input = open(filename, O_RDONLY|O_NONBLOCK))==-1)
    {
        fprintf(stderr, "Cannot read from input file '%s'\n", filename);
        return -1;
    }

    // Reset the parameter
    mparm.item = 0;
    mparm.total_match = 0;
    mparm.fname = fd_input?(char *)filename:NULL;

    int keep = 0;
    // loop to load and search the input file repeatedly, chunk by chunk
    do
    {
        // Read a chunk from input file
        num_read = read (fd_input, (void *)in_stream_buffer, STREAM_BUFFER_SIZE);
        if (num_read<0)
        {
            fprintf(stderr, "Error while reading from '%s'\n", filename);
            return -1;
        }
        
        intext.length = num_read;

        // Handle case sensitivity
        if (configuration.insensitive)
            lower_case(in_stream_buffer, intext.length);

        // Break loop if call-back function has done its work
        if (ac_automata_search (paca, &intext, keep, match_handler, &mparm))
            break;
        keep = 1;
    } while (num_read == STREAM_BUFFER_SIZE);

    close (fd_input);

    return 0;
}

/******************************************************************************
 * FUNCTION: replace_file
 *****************************************************************************/

int replace_file (AC_AUTOMATA_t * paca, const char * infile, const char * outfile)
{
    int fd_input; // Input file descriptor
    int fd_output; // output file descriptor
    static AC_TEXT_t intext; // input text
    static AC_ALPHABET_t in_stream_buffer[STREAM_BUFFER_SIZE];
    static struct match_param uparm; // user parameters
    ssize_t num_read; // Number of byes read from input file
    struct stat file_stat;
    
    intext.astring = in_stream_buffer;

    // Open input file
    if (!strcmp(configuration.input_files[0], "-"))
    {
        fd_input = 0; // read from stdin
    }
    else if ((fd_input = open(infile, O_RDONLY|O_NONBLOCK))==-1)
    {
        fprintf(stderr, "Cannot read from input file '%s'\n", infile);
        return -1;
    }
    
    if (fstat(fd_input, &file_stat))
    { 
        fprintf(stderr, "Cannot get file stat for '%s'\n", infile);
        close(fd_input);
        return -1; 
    }
    
    if (S_ISDIR(file_stat.st_mode))
    {
        fprintf(stderr, "Directories is not supported in replace mode: skipped '%s'\n", infile);
        close(fd_input);
        return -1;
    }
    
    // open output file
    if (outfile)
    {
        if ((fd_output = open(outfile, O_WRONLY|O_CREAT|O_TRUNC, file_stat.st_mode))==-1)
        {
            fprintf(stderr, "Cannot open '%s' for writing\n", outfile);
            return -1;
        }
    }
    else
    {
        fd_output = 1; // sent output to stdout
    }
    
    // Reset the parameter
    uparm.item = 0;
    uparm.total_match = 0;
    uparm.fname = NULL; // note used
    uparm.out_file_d = fd_output;

    // loop to load and search the input file repeatedly, chunk by chunk
    do
    {
        // Read a chunk from input file
        num_read = read (fd_input, (void *)in_stream_buffer, STREAM_BUFFER_SIZE);
        if (num_read<0)
        {
            fprintf(stderr, "Error while reading from '%s'\n", infile);
            return -1;
        }
        if (num_read==0)
            break;
        
        intext.length = num_read;

        // Handle case sensitivity
        if (configuration.insensitive)
            lower_case(in_stream_buffer, num_read);

        if (ac_automata_replace (paca, &intext, replace_listener, &uparm))
            /* Break loop if call-back function has done its work */
            break;
        
    } while (1);
    
    ac_automata_rflush (paca, replace_listener, &uparm);

    close (fd_input);
    close (fd_output);

    return 0;
}

//*****************************************************************************
// FUNCTION: lower_case
//*****************************************************************************

void lower_case (char * s, size_t l)
{
    size_t i;
    for(i=0; i<l; i++)
        s[i] = tolower(s[i]);
}

//*****************************************************************************
// FUNCTION: mkpath
//*****************************************************************************

int mkpath(const char * path, mode_t mode)
{
    char * p = (char*)path;

    /* Do mkdir for each slash until end of string or error */
    while (*p != '\0')
    {
        /* Skip first character */
        p++;

        /* Find first slash or end */
        while(*p != '\0' && *p != '/') p++;

        /* Remember value from p */
        char v = *p;

        /* Write end of string at p */
        *p = '\0';

        /* Create folder from path to '\0' inserted at p */
        if(mkdir(path, mode) == -1 && errno != EEXIST)
        {
            *p = v;
            return -1;
        }

        /* Restore path to it's former glory */
        *p = v;
    }

    return 0;
}

//*****************************************************************************
// FUNCTION: get_outfile_name
//*****************************************************************************
char * get_outfile_name (const char * dir, const char * inpath)
{
#define PATH_BUFFER_LENGTH 1024
    static char * buffer = NULL; /* Lives as the program does; never released */
    static size_t bufsize = 0;
    size_t dirlen = 0, pathlen = 0;
    struct stat st;
    char *fname;
    __mode_t md = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; /* Default mode */
        
    if (dir==NULL || *dir=='\0' || !strcmp(dir,"-"))
        return NULL;
        /* Send to the standard output */
    
    if (inpath && *inpath=='\0')
        return NULL; /* unexpected: assert */
    
    /* Manage buffer allocation
     * the lifetime of the buffer equals to the program life time */
    if (buffer==NULL)
    {
        bufsize = PATH_BUFFER_LENGTH;
        buffer = (char *)malloc(bufsize*sizeof(char));
    }
    dirlen = strlen(dir);
    pathlen = dirlen + strlen(inpath) + 1;
    
    /* Grow memory if needed */
    if (bufsize < pathlen)
    {
        bufsize = ((pathlen/PATH_BUFFER_LENGTH)+1)*PATH_BUFFER_LENGTH;
        buffer = realloc (buffer, bufsize);
    }
    
    *buffer = '\0';
    strcpy(buffer, dir);
    if (buffer[dirlen-1]!='/') {
        buffer[dirlen++] = '/';
        buffer[dirlen] = '\0';
    }
    
    if ((fname = rindex (inpath, (int)'/')))
    {
        *fname='\0';
        if (*inpath!='\0') {
            stat(inpath, &st);
            md = st.st_mode;
        }
        strcat (buffer, *inpath=='/'?inpath+1:inpath);
    }
    
    mkpath (buffer, md);
    
    if (fname != NULL) {
        *fname='/';
        strcat (buffer, fname);
    } else {
        strcat (buffer, *inpath=='/'?inpath+1:inpath);
    }
    
    return buffer;
}

//*****************************************************************************
// FUNCTION: print_usage
//*****************************************************************************

void print_usage (char * progname)
{
    printf("Usage : %s -P pattern_file [-R out_dir [-l] | -n[d|x]rpvfi] [-h] file1 [file2 ...]\n", progname);
}

//*****************************************************************************
// FUNCTION: match_handler
//*****************************************************************************

int match_handler (AC_MATCH_t * m, void * param)
{
    unsigned int j;
    struct match_param * mparm = (struct match_param *)param;
    
    for (j=0; j < m->match_num; j++)
    {
        //if (mparm->item==0)
        if (mparm->fname)
            printf ("%s: ", mparm->fname);

        if (configuration.output_show_item)
            printf("#%ld ", ++mparm->item);

        if (configuration.output_show_dpos)
            printf("@%ld ", m->position - m->patterns[j].ptext.length + 1);

        if (configuration.output_show_xpos)
            printf("@%08X ", (unsigned int)(m->position - m->patterns[j].ptext.length + 1));

        if (configuration.output_show_reprv)
            printf("%s ", m->patterns[j].title.stringy);

        if (configuration.output_show_pattern)
            pattern_print (&m->patterns[j]);

        printf("\n");
    }

    mparm->total_match += m->match_num;

    if (configuration.find_first)
        return 1; // Find First Match
    else
        return 0; // Find all matches
}

/******************************************************************************
 * FUNCTION: replace_listener
 *****************************************************************************/

int replace_listener (AC_TEXT_t * text, void * user)
{
    write (((struct match_param *)user)->out_file_d, 
            text->astring, text->length);

    return 0;
}
