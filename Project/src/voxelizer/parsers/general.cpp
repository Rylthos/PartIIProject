#include "general.hpp"

namespace ParserImpl {

std::vector<std::string> split(std::string str, std::string delim)
{
    std::vector<std::string> components;

    size_t pos = 0;
    size_t newPos = 0;
    while (newPos != std::string::npos) {
        newPos = str.find(delim, pos);
        std::string sect = str.substr(pos, newPos - pos);
        pos = newPos + 1;

        if (sect.length() != 0) {
            components.push_back(sect);
        } else {
            components.push_back("");
        }
    }

    return components;
}

}
