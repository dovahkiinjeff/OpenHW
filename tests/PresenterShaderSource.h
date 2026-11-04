#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include "../src/Modern/GoodGodRaysShader.h"

// Match the runtime presenter source assembly. Keeping shader bodies in their
// production headers also lets the offscreen checks compile the actual port.
inline std::string readProductionPresenterShader(const char *path)
{
    std::ifstream file(path);
    if (!file) throw std::runtime_error("production presenter source unavailable");
    std::stringstream contents;
    contents << file.rdbuf();
    const std::string source = contents.str();
    const char *markers[] = {
        "const char presenterBaseShader[] = R\"(",
        "const char presenterShader[] = R\"("
    };
    size_t begin = std::string::npos;
    for (const char *marker : markers)
    {
        begin = source.find(marker);
        if (begin != std::string::npos)
        {
            begin += std::char_traits<char>::length(marker);
            break;
        }
    }
    if (begin == std::string::npos)
        throw std::runtime_error("presenter shader block missing");
    const size_t end = source.find(")\";", begin);
    if (end == std::string::npos)
        throw std::runtime_error("presenter shader terminator missing");
    std::string shader = source.substr(begin, end - begin);
    shader += hwmodern::goodGodRaysShader;
    return shader;
}
