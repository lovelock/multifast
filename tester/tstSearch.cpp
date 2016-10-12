#include <iostream>
#include <string>
#include <set>
#include "RandomString.h"
#include "SearchResult.h"
#include "ahocorasick.h"

SearchResult findUsingAC (const std::set<std::string> &sampleChunks, 
        const std::string &input);

SearchResult findUsingStr (const std::set<std::string> &sampleChunks, 
        const std::string &input);

int main (int argc, char **argv)
{
    std::set<std::string> sampleChunks;
    RandomString rs(100, 150, 4);
    int i, j;
    
    // std::cout << rs << std::endl;
    // std::cout << rs.roll() << std::endl;
    std::cout << "Testing 'Search'" << std::endl;
    
    for (j = 0; j < 100000; j++)
    {
        sampleChunks.clear();
        
        for (i = 0; i < 40; i++)
        {
            sampleChunks.insert(rs.getFactor(3, 5));
        }

        std::string input = rs.getString();

        SearchResult sr1 = findUsingStr(sampleChunks, input);
        SearchResult sr2 = findUsingAC(sampleChunks, input);



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
    
    return 0;
}

SearchResult findUsingAC (const std::set<std::string> &sampleChunks, 
        const std::string &input)
{
    SearchResult sr;
    unsigned int i = 0;
    AC_TRIE_t *trie = ac_trie_create();
    AC_PATTERN_t patt;
    AC_TEXT_t chunk;
    AC_MATCH_t match;
    
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
        ac_trie_add (trie, &patt, 0);
        
        // std::cout << *it << " Added Successfully" << std::endl;
    }
    ac_trie_finalize (trie);
    
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
    
    ac_trie_release(trie);
    
    return sr;
}

SearchResult findUsingStr (const std::set<std::string> &sampleChunks, 
        const std::string &input)
{
    SearchResult sr;
    
    for (std::set<std::string>::iterator it = sampleChunks.begin();
            it != sampleChunks.end(); ++it)
    {
        size_t pos = 0;
        
        do 
        {
            pos = input.find(*it, pos);
            if (pos == std::string::npos)
                break;
            sr.add(pos++, *it);
        }
        while (true);
    }
    
    return sr;
}
