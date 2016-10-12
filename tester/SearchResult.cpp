#include <iostream>
#include <algorithm>
#include "SearchResult.h"

SearchResult::SearchResult()
{
    m_sorted = true;
}

SearchResult::SearchResult(const SearchResult& orig)
{
    m_results = orig.m_results;
    m_sorted = orig.m_sorted;
}

SearchResult::~SearchResult()
{
}

void SearchResult::add(size_t pos, std::string str)
{
    ResultItem ri(pos, str);
    m_results.push_back(ri);
    m_sorted = false;
}

bool compareSR (SearchResult::ResultItem &l, SearchResult::ResultItem &r)
{
    return (l.thePosition < r.thePosition); 
}

void SearchResult::sort(void)
{
    if (m_sorted)
        return;
    
    std::sort (m_results.begin(), m_results.end(), compareSR);
    m_sorted = true;
}

bool SearchResult::operator==(SearchResult &r)
{
    SearchResult &l = *this;
    l.sort();
    r.sort();
    
    if (l.m_results.size() != r.m_results.size())
        return false;
    
    std::vector<ResultItem>::iterator lit = l.m_results.begin();
    std::vector<ResultItem>::iterator rit = r.m_results.begin();
    
    do
    {
        if (lit->thePosition != rit->thePosition || 
                lit->theString != lit->theString)
            return false;
        
        ++lit;
        ++rit;
    } while (lit != l.m_results.end() && rit != r.m_results.end());
    
    return true;
}

std::ostream & operator<<(std::ostream &os, SearchResult &rs)
{
    rs.sort();
    
    for (std::vector<SearchResult::ResultItem>::iterator  
        it = rs.m_results.begin(); it != rs.m_results.end(); ++it)
    {
        std::cout << (*it).thePosition << " " << (*it).theString << std::endl;
    }
}
