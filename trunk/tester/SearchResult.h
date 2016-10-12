#ifndef SEARCHRESULT_H
#define	SEARCHRESULT_H

#include <vector>
#include <string>

class SearchResult
{
    friend std::ostream & operator<<(std::ostream &os, SearchResult &rs);
    
public:
    SearchResult();
    SearchResult(const SearchResult& orig);
    virtual ~SearchResult();
    void add(size_t pos, std::string str);
    bool operator==(SearchResult &r);
    
private:
    
    struct ResultItem
    {
        ResultItem(size_t p, std::string s) 
            : thePosition(p), theString(s) {};
        
        size_t thePosition;
        std::string theString;
        
    };
    
    friend bool compareSR (SearchResult::ResultItem &l, 
        SearchResult::ResultItem &r);
    
private:
    void sort (void);
    
private:
    
    std::vector<ResultItem> m_results;
    bool m_sorted;
};

#endif	/* SEARCHRESULT_H */
