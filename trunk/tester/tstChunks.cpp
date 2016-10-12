#include <iostream>
#include <string>
#include <set>
#include "RandomString.h"
#include "SearchResult.h"
#include "ahocorasick.h"

AC_TRIE_t *loadTrie (const std::set<std::string> &sampleChunks);
SearchResult searchMonoliticStr (AC_TRIE_t *trie, const std::string &input);
SearchResult searchChunkedStr (AC_TRIE_t *trie, const std::string &input);


int main(int argc, char **argv)
{
    std::set<std::string> sampleChunks;
    RandomString rs(100, 150, 4);
    int i, j;
    
    sampleChunks.clear();
    
    for (i = 0; i < 40; i++)
    {
        sampleChunks.insert(rs.getFactor(3, 5));
    }
    
    std::string input = rs.getString();
    
    AC_TRIE_t *trie = loadTrie(sampleChunks);
    
    SearchResult sr1 = searchMonoliticStr(trie, input);
    
    std::cout << "Testing 'Chunks'" << std::endl;
    
    for (j = 0; j < 100000; j++)
    {
        SearchResult sr2 = searchChunkedStr(trie, input);

        if (sr1 == sr2)
        {
            if ((j + 1) % 8000 == 0)
                std::cout << " " << j + 1 << " Passed" << std::endl;
            else if ((j + 1) % 100 == 0)
                std::cout << "." << std::flush;
        }
        else
        {
            std::cout << input << std::endl;

            std::cout << sr1;
            std::cout << std::endl;
            std::cout << sr2;
            std::cout << std::endl;
            return -1;
        }
    }
    
    std::cout << " " << j << " Passed" << std::endl;
    
    ac_trie_release(trie);
    
    return 0;
}

AC_TRIE_t *loadTrie (const std::set<std::string> &sampleChunks)
{
    unsigned int i = 0;
    AC_TRIE_t *trie = ac_trie_create();
    AC_PATTERN_t patt;
    
    for (std::set<std::string>::iterator it = sampleChunks.begin();
            it != sampleChunks.end(); ++it)
    {
        patt.ptext.astring = it->c_str();
        patt.ptext.length = it->size();
        patt.rtext.astring = NULL;
        patt.rtext.length = 0;
        patt.id.u.number = i + 1;
        patt.id.type = AC_PATTID_TYPE_NUMBER;
        
        /* Add pattern to automata */
        ac_trie_add (trie, &patt, 1);
        
        // std::cout << *it << " Added Successfully" << std::endl;
    }
    ac_trie_finalize (trie);
    
    return trie;
}

SearchResult searchMonoliticStr (AC_TRIE_t *trie, const std::string &input)
{
    SearchResult sr;
    AC_TEXT_t chunk;
    AC_MATCH_t match;
    
    chunk.astring = input.c_str();
    chunk.length = input.size();
    
    ac_trie_settext (trie, &chunk, 0);
    
    while ((match = ac_trie_findnext(trie)).size)
    {
        for (unsigned int j = 0; j < match.size; j++)
        {
            sr.add(match.position - match.patterns[j].ptext.length, 
                    std::string(match.patterns[j].ptext.astring));
        }
    }
    
    return sr;
}

SearchResult searchChunkedStr (AC_TRIE_t *trie, const std::string &input)
{
    SearchResult sr;
    RandomString ranstr;
    
    AC_TEXT_t chunk;
    AC_MATCH_t match;
    
    size_t index = 0, max;
    const char *input_text = input.c_str();
    size_t input_len = input.size();
        
    while (index < input_len)
    {
        max = (input_len - index) > 12 ? 12 : (input_len - index);
        
        chunk.astring = &input_text[index];
        chunk.length = ranstr.RandUInt (1, max);
        
        ac_trie_settext (trie, &chunk, index);
        
        while ((match = ac_trie_findnext(trie)).size)
        {
            for (unsigned int j = 0; j < match.size; j++)
            {
                sr.add(match.position - match.patterns[j].ptext.length, 
                        std::string(match.patterns[j].ptext.astring));
            }
        }
        
        index += chunk.length;
    }
    
    return sr;
}
