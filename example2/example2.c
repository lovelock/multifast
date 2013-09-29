/*
 * example2.c: This program illustrates how to use ahocorasick library
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

char input_file [] =
    "MIIEowIBAAKCAQEAss4h3LUUdXaPJifvhPtaJb+hZD7Aq31liy8rgtS9E5ZiTVLa"
    "89crCrYH/T+6r0SIB69lsYJ4Et8bV9rH0EZ9OvXe6Fbc8J9OgYqqX962UzgnDFdp"
    "ETpkf6sBbsOYAomyyJv/2+zrDi8oLcNwTIjnGPAv6SUqyeeZVZOiFVUDZMD0Ar2J"
    "M/nEnFKwEF8EbSnKXsdxr7FHunQIRYN2at37Zbi0MhfU38ZssmlQc1nhrxA1JNQu"
    "Kd5QRtXI7CSBY1qMJBI8eMPIAvjOuUd3dyvHfg+wUK59RhFMpTTUu/e5PhPIJGZN"
    "fdepyHxfrD7XUFpIWIemZyzJIKnUa/f/k7nUZQIDAQABAoIBAEE6eZfuZzxCuA4P"
    "W42DtGs48UOEsPzejgCsgI8F1MQkhE/4/e8ee5E4vslgSlZOBeHn1o1fLzaTNYJj"
    "SaltTZoIv/y6U3dkZltTnrvCn3jUb0pNSQMD7k20sJ0btYSXfyU346rzTvypr1qi"
    "hYEmIEg0twWyjV2Co6lYZjSqAsbqENXPHUGvbLijMBsUgo5WYkxvwl6PAIjkd7Qq"
    "1okD537Tc4idodrTj8x/SiqOxHaYjXjIuWq2iKBujOvmPeOjfvHohBwQLekXHXyB"
    "0oDvPSrop2CDFq7Vjmtah/oOkhpl4IVDwFV2ihoL7MDBJqIOOaq21VarNn1jVLKb"
    "dQhwe7ECgYEA4BUBUB4BUkTOxc5hxqXRtMy/n2/Fi4sWelZKU51gF3EsAZ8qiWdP"
    "Q0SW0IN7QtMZMCnjEKeIguwsnHLEzdkegyQBktm9OsGegsLubJSxpq96lpvkrQTW"
    "u34d4fNgE4F/yt8ZVq/Yo6QK4oytjvq+piMFi/V9yWSc0la4P0j2Bv8CgYEAzEYj"
    "f4i1AU+Bw/xL/V+ZAk8mSWxSXhY2h3XQNtlfY7DCd8Q2d14GD24WyVRMRyt9rc4e"
    "BXG3oojMaLX9CvAHU1bujDCwspcWz3/4gyrBwvwBpvMqnq3Na6p8CU5pWHWbigWm"
    "rXcavX61hFq9RsIW3Gf1JXp1QCVPPsDp1hgUaJsCgYBPfgQeONa9KZ20vFchUlfH"
    "bM8Zr1wD+c56jbwGV5DsIAC9fopnfhe3RFDAHbSPazXHSUS89sHNuBzHT0uTvs89"
    "NHu+bbHccy1ZM+/C4yj2ec/iN+Fyo4HNt5rAOkc+BDWicWyavPz8aEhYQBGd5EPX"
    "yhrAoNEDrcaYM51fDfIBXwKBgQCKVETdrFnGlWyupz9eSUp4QdkPh4cPp8MtYB6r"
    "xe/Otng6Wmj31HgOIuLTW358A3uMIzQ5Q5SzQCgMEJFWwsxzJz9LN/2wMpiD04ka"
    "ae3keHs17x1BbzjYXA66zpqQCLRXdxQ0C5/UCuYoxrm+HNkWUF+2DYMw+RL8z+6J"
    "yKypWQKBgANI4wAjYwcJDw5poJFhjSDcpijHcljgUWJAWuRuBTNAfnJQ+q33uDRp"
    "bT7iTYAbSfPA2mdHM+iwyMaUA1OXga1q+BkwPvj9xgPNtJ55qm8bRl1goAgzcV+Q"
    "MUbohTOrjtfGq6nwacVAEN6C2LkbzyOmdK1PipP/SUWZ6P0Cfzka";

char buffer[256];

struct sample_param
{
    int anum;
    char achar;
};

/****************************************************************************/

//*** 1. Define a call-back function of the MATCH_CALBACK_t type
int match_handler(AC_MATCH_t * m, void * param)
{
    unsigned int j;

    /* example of sending parameter to call-back function */
    struct sample_param * myp = (struct sample_param *)param;

    if (myp->anum==1)
        printf ("@ %ld : ", m->position);
    else
        printf ("At %ld : ", m->position);

    for (j=0; j < m->match_num; j++)
        printf("%s (%s), ",
                m->patterns[j].rep.stringy,
                m->patterns[j].astring);

    printf("\n");

    switch (myp->achar)
    {
    case 'f': /* find first */
        return 1;
    case 'a': /* find all */
    default:
        return 0;
    }
}

/****************************************************************************/

int main (int argc, char ** argv)
{
    //*** 2. Define AC variables
    AC_AUTOMATA_t * acap;
    AC_PATTERN_t allpattern[] =
    {
        {"HaYjXjIuWq2", 0, {stringy: "one"}},
        {"CSBY1qM", 0, {stringy: "two"}},
        {"IRYN2at37Zbi", 0, {stringy: "tree"}},
        {"DFq7Vjmtah", 0, {stringy: "four"}},
        {"qAsbqENXPH", 0, {stringy: "five"}},
        {"YwcJDw5poJF", 0, {stringy: "six"}},
        {"38ZssmlQc1nhrxA1JNQuKd5Q", 0, {stringy: "seven"}},
    };
    AC_TEXT_t input_text = {0, 0};

    #define PATTERN_NUMBER (sizeof(allpattern)/sizeof(AC_PATTERN_t))

    unsigned int i;

    /* Sending parameter to call-back function */
    struct sample_param my_param;
    my_param.anum = 1;
    my_param.achar = 'a'; /* 'a': find all, 'f': find first */

    printf("Example 2: ahocorasick example program\n");

    //*** 3. Get a new automata
    acap = ac_automata_init ();

    //*** 4. add patterns to automata
    for (i=0; i<PATTERN_NUMBER; i++)
    {
        allpattern[i].length = strlen(allpattern[i].astring);
        ac_automata_add (acap, &allpattern[i]);
    }

    //*** 5. Finalize automata.
    ac_automata_finalize (acap);

    //*** 5.1 Display automata
    //ac_automata_display (acap, 's');

    /* This illustrates how to search big text chunk by chunk
     * in this example input buffer size is 256 and input file is pretty
     * bigger than that. in fact it imitate reading from input file.
     * in such situations searching must be done inside a loop. the loop
     * continues until it consumes all input file.
    **/
    char * chunk_start = input_file;
    char * end_of_file = input_file+sizeof(input_file);
    input_text.astring = buffer;

    /* Search loop */
    while (chunk_start<end_of_file)
    {
        //*** 6. Set input text
        input_text.length = (chunk_start<end_of_file)?
                sizeof(buffer):(sizeof(input_file)%sizeof(buffer));
        strncpy(input_text.astring, chunk_start, input_text.length);

        //*** 7. Do search
        if (ac_automata_search (acap, &input_text, 1, match_handler, (void *)(&my_param)))
            break;
        /* according to the return value of ac_automata_search() we decide to
         * continue or break loop. */

        chunk_start += sizeof(buffer);
    }

    //*** 9. Release automata
    ac_automata_release (acap);

    return 0;
}
