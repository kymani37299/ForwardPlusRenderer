#pragma once

#include <string>

namespace PathUtility
{
    inline std::string GetPathWitoutFile(const std::string& path)
    {
        return path.substr(0, 1 + path.find_last_of("\\/"));
    }

    inline std::string GetFileExtension(const std::string path)
    {
        size_t i = path.rfind('.', path.length());
        if (i != std::string::npos)
        {
            return path.substr(i + 1, path.length() - i);
        }
        return "";
    }
}