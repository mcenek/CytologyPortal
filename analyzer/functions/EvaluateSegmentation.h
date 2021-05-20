#ifndef EVALUATESEGMENTATION_H
#define EVALUATESEGMENTATION_H

#include "opencv2/opencv.hpp"
#include "EvaluateSegmentation.h"
#include "../objects/Image.h"

using namespace std;

namespace segment {
    double calcContoursArea(vector<vector<cv::Point>> contours);

    double calcDice(cv::Mat estimated, cv::Mat groundTruth);

    double calcTotalDice(cv::Mat estimated, cv::Mat groundTruth);

    int* findMaxDiceLocation(vector<vector<double>> allDice);

    double evaluateSegmentation(Image *image);
}

#endif //EVALUATESEGMENTATION_H
