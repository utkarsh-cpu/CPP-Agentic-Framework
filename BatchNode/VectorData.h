#ifndef VECTORDATA_H
#define VECTORDATA_H

#include "../BaseNode/FrameworkData.h"
#include <vector>
#include <memory>

struct VectorData : public FrameworkData {
    std::vector<std::shared_ptr<FrameworkData>> value;
};

#endif //VECTORDATA_H
