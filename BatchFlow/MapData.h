#ifndef MAPDATA_H
#define MAPDATA_H

#include "../BaseNode/FrameworkData.h"
#include <map>
#include <string>
#include <memory>

struct MapData : public FrameworkData {
    std::map<std::string, std::shared_ptr<FrameworkData>> value;
};

#endif //MAPDATA_H
