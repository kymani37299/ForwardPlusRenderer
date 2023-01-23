#pragma once
#pragma once

#include <algorithm>
#include <vector>
#include <string>
#include <stringapiset.h>

#include "Common.h"

namespace StringUtility
{
    inline std::wstring ToWideString(const std::string& s)
    {
        int len;
        int slength = (int)s.length() + 1;
        len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
        wchar_t* buf = new wchar_t[len];
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
        std::wstring r(buf);
        delete[] buf;
        return r;
    }

    inline void ReplaceAll(std::string& str, const std::string& from, const std::string& to) {
        if (from.empty())
            return;
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }
    }

    inline bool Contains(const std::string& string, const std::string& param)
    {
        return string.find(param) != std::string::npos;
    }

    inline std::vector<std::string> Split(std::string input, const std::string& delimiter)
    {
        std::vector<std::string> result;
        size_t pos = 0;
        while ((pos = input.find(delimiter)) != std::string::npos) {
            std::string token = input.substr(0, pos);
            result.push_back(token);
            input.erase(0, pos + delimiter.length());
        }
        result.push_back(input);
        return result;
    }

    inline std::string ToLower(const std::string& input)
    {
        std::string result = input;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    inline std::string ToUpper(const std::string& input)
    {
        std::string result = input;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::toupper(c); });
        return result;
    }

    template<typename IntType>
    inline std::string RepresentNumberWithSeparator(IntType number, const char separator)
    {
        if (number == 0) 
            return "0";

        std::string intRepresentation;
        uint8_t digitIndex = 0;
        while (number)
        {
            uint8_t digit = number % 10;
            number = number / 10;
            intRepresentation += (char)(digit + '0');

            digitIndex++;
            if (digitIndex == 3)
            {
                digitIndex = 0;
                intRepresentation += separator;
            }
        }
        std::reverse(intRepresentation.begin(), intRepresentation.end());
        return intRepresentation;
    }

}