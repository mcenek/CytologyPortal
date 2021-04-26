#ifndef INITIALCELLSEGMENTATION_H
#define INITIALCELLSEGMENTATION_H

#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"

namespace segment {
    void associateClumpBoundariesWithCell(Clump *clump, int c, bool debug = false);

    bool testLineViability(cv::Point pixel, Clump *clump, Cell *cell);

    float getDistance(cv::Point a, cv::Point b);

    vector<float> getLineDistances(cv::Point start, cv::Point stop);

    cv::Point getMidpoint(const cv::Point& a, const cv::Point& b);

    //TODO: Fix this to work in all cases
    vector<cv::Point> interpolateLine(cv::Point start, cv::Point middle, cv::Point stop);

    void startInitialCellSegmentationThread(Image *image, Clump *clump, int clumpIdx, cv::RNG *rng, cv::Mat outimg, bool debug);

    cv::Mat runInitialCellSegmentation(Image *image, int threshold1, int threshold2, bool debug = false);
}

#endif //INITIALCELLSEGMENTATION_H