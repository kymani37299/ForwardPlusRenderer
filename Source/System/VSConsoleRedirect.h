#pragma once

#include <iostream>
#include <Windows.h>

class VS_StreamBuffer : public std::streambuf 
{
public:
    virtual int_type overflow(int_type c = EOF) 
    {
        if (c != EOF) 
        {
            TCHAR buf[] = { c, '\0' };
            OutputDebugString(buf);
        }
        return c;
    }
};

class RedirectToVSConsoleScoped 
{
public:
    RedirectToVSConsoleScoped() 
    {
        m_DefaultStream = std::cout.rdbuf(&m_VSStream);
    }

    ~RedirectToVSConsoleScoped() 
    {
        std::cout.rdbuf(m_DefaultStream);
    }

private:
    VS_StreamBuffer m_VSStream;
    std::streambuf* m_DefaultStream;
};