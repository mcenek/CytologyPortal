#ifndef INITIALCELLSEGMENTATION_H
#define INITIALCELLSEGMENTATION_H

#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"

namespace segment {
    bool testLineViability(cv::Point pixel, Clump *clump, Cell *cell);

    cv::Mat runInitialCellSegmentation(Image *image, int threshold1, int threshold2, bool debug = false);
}

#endif //INITIALCELLSEGMENTATION_H