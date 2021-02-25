#ifndef SEGMENTER_CPP_INITIALCELLSEGMENTATION_H
#define SEGMENTER_CPP_INITIALCELLSEGMENTATION_H

#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"

namespace segment {
    void associateClumpBoundariesWithCell(Clump *clump, int c, bool debug = false);

    bool testLineViability(cv::Point pixel, Clump *clump, Cell *cell);

    vector<float> getLineDistances(cv::Point start, cv::Point stop);

    cv::Point getMidpoint(const cv::Point& a, const cv::Point& b);

    //TODO: Fix this to work in all cases
    vector<cv::Point> interpolateLine(cv::Point start, cv::Point middle, cv::Point stop);

    void runInitialCellSegmentation(vector<Clump> *clumps, int threshold1, int threshold2, bool debug = false);
}

#endif //SEGMENTER_CPP_INITIALCELLSEGMENTATION_H


/*
 *                             float angle = angleBetween(startPoint, endPoint);
                            cv::Point midpoint = getMidpoint(startPoint, endPoint);
                            double width = cv::norm(startPoint-midpoint);
                            double height = 50;
                            cv::RotatedRect rotatedRect = cv::RotatedRect(midpoint, cv::Size2f(width,height), angle);
                            cv::ellipse(temp, rotatedRect, cv::Scalar(255), -1);
 */