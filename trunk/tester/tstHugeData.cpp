#include <iostream>
#include <string>
#include <set>
#include "ahocorasick.h"
#include "RandomString.h"

AC_TRIE_t * buildTrie(size_t pattern_nr);

int main (int argc, char **argv)
{
    AC_TRIE_t * trie;
    
    std::cout << "Testing 'HugeData'" << std::endl;
    
    trie = buildTrie(100000);
    
    ac_trie_release(trie);
    
    std::cout << "Done." << std::endl;
    
    return 0;
}

AC_TRIE_t * buildTrie(size_t pattern_nr)
{
    unsigned int i;
    RandomString rs(30, 80, 36);
    
    AC_TRIE_t *trie = ac_trie_create();
    AC_PATTERN_t patt;
    
    
    for (i = 0; i < pattern_nr; i++)
    {
        rs.roll();
        patt.ptext.astring = rs.getString().c_str();
        patt.ptext.length = rs.getString().size();
        patt.rtext.astring = NULL;
        patt.rtext.length = 0;
        patt.id.u.number = i + 1;
        patt.id.type = AC_PATTID_TYPE_NUMBER;
        
        /* Add pattern to automata */
        if (ac_trie_add (trie, &patt, 0) == ACERR_SUCCESS) {
            // std::cout << rs.getString() << " - Added Successfully" << std::endl;
            if ((i + 1) % 8000 == 0)
                std::cout << " " << i + 1 << " Added" << std::endl;
            else if ((i + 1) % 100 == 0)
                std::cout << "." << std::flush;
        } else {
            std::cout << std::endl << rs.getString() << " Failed" << std::endl;
            break;
        }
    }
    std::cout << "Total " << i << std::endl;
    std::cout << "Building Trie" << std::endl;
    
    ac_trie_finalize (trie);
        
    return trie;
}
