#ifndef EXPORT_H
#define EXPORT_H

#include "../thirdparty/nlohmann/json.hpp"
#include "boost/filesystem.hpp"
#include "../objects/Image.h"

namespace segment {
    void exportResults(Image *image);
}

#endif //EXPORT_H
