#ifndef STRINGDATA_H
#define STRINGDATA_H

#include "../BaseNode/FrameworkData.h"
#include <string>

struct StringData : public FrameworkData {
    std::string value;
};

#endif //STRINGDATA_H
