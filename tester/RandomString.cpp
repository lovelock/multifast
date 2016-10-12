#include <cstdlib>
#include <ctime>
#include <iostream>
#include "RandomString.h"

RandomString::RandomString(void)
{
    Randomize();
    m_minLen = 1;
    m_maxLen = 256;
    m_setSize = 0;
    roll(RandUInt(m_minLen, m_maxLen), m_setSize);
}

RandomString::RandomString(int setSize)
{
    Randomize();
    m_minLen = 1;
    m_maxLen = 256;
    m_setSize = setSize;
    roll(RandUInt(m_minLen, m_maxLen), m_setSize);
}

RandomString::RandomString(size_t len, int setSize)
{
    Randomize();
    m_minLen = len;
    m_maxLen = len;
    m_setSize = setSize;
    roll(RandUInt(m_minLen, m_maxLen), m_setSize);
}

RandomString::RandomString(size_t minLen, size_t maxLen, int setSize)
{
    Randomize();
    m_minLen = minLen;
    m_maxLen = maxLen;
    m_setSize = setSize;
    roll(RandUInt(m_minLen, m_maxLen), m_setSize);
}

RandomString::~RandomString()
{
}

void RandomString::Randomize(void)
{
    srand (time(NULL));
}

unsigned int RandomString::RandLimit(unsigned int limit)
{
    int retval, divisor;
    
    divisor = RAND_MAX / (limit + 1);
    
    do { 
        retval = rand() / divisor;
    } while (retval > limit);
    
    return retval;
}

unsigned int RandomString::RandUInt(unsigned int min, unsigned int max)
{
    if (min == max)
        return min;
    
    if (min < max)
        return min + RandLimit (max - min);
    
    return max + RandLimit (min - max);
}

RandomString & RandomString::roll (void)
{
    roll(RandUInt(m_minLen, m_maxLen), m_setSize);
    return *this;
}

RandomString & RandomString::roll (size_t len, int setSize)
{
    if (setSize > 26 || setSize <= 0)
        setSize = 26;
    
    const int min = (int)'A';
    const int max = (int)'A' + setSize - 1;    
    size_t i;
    
    m_string.clear();
    
    for (i = 0; i < len; i++)
       m_string.push_back((char) RandUInt (min, max));
    
    return *this;
}

RandomString & RandomString::roll 
    (size_t minLen, size_t maxLen, int setSize)
{
    return roll(RandUInt(minLen, maxLen), setSize);
}

std::string RandomString::getFactor(size_t min, size_t max)
{
    size_t patIndex, patLen;
    
    do {
        patIndex = RandLimit(m_string.size() - 1);
        patLen = RandUInt(min, max);
    } while (patIndex + patLen > m_string.size());
    
    return m_string.substr(patIndex, patLen);
}

std::ostream & operator<<(std::ostream &os, const RandomString& rs)
{
    char ch;
    int i = 0;
    const unsigned int llen = 60;
    const char * str = rs.m_string.c_str();    
    
    std::cout << "char * str =\n\t\"";
    
    while ((ch = str[i++]))
    {
        std::cout << ch;
        if (i % llen == 0) 
            std::cout << "\"\n\t\"";
    }
    
    std::cout << "\";\n";
}
