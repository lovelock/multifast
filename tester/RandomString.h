#ifndef RANDOMSTRING_H
#define	RANDOMSTRING_H

#include <string>

class RandomString
{
    friend std::ostream & operator<<(std::ostream &os, const RandomString& rs);

public:
    
    RandomString(void);
    RandomString(int setSize);
    RandomString(size_t len, int setSize);
    RandomString(size_t minLen, size_t maxLen, int setSize);
    virtual ~RandomString();
    
    RandomString & roll (void);
    RandomString & roll(size_t len, int setSize);
    RandomString & roll(size_t minLen, size_t maxLen, int setSize);
    
    std::string & getString(void) { return m_string; };
    
    std::string getFactor(size_t min, size_t max);
    
    unsigned int RandLimit(unsigned int limit);
    unsigned int RandUInt(unsigned int min, unsigned int max);
    
private:
    
    void Randomize(void);
    
private:
    
    std::string m_string;
    size_t m_minLen;
    size_t m_maxLen;
    int m_setSize;
    
};

#endif	/* RANDOMSTRING_H */
