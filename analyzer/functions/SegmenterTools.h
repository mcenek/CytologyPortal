#ifndef SEGMENTERTOOLS_H
#define SEGMENTERTOOLS_H

#include "opencv2/opencv.hpp"

using namespace std;

namespace segment {

    //Returns a divergence of the given matrix
    cv::Mat calcDivergence(cv::Mat x, cv::Mat y);

    std::vector<cv::Mat> calcGradient(cv::Mat image);

    //Returns the magnitude for the given matricies
    cv::Mat calcGradientMagnitude(cv::Mat img);

    //Returns the magnitude for the given matricies
    cv::Mat calcMagnitude(cv::Mat x, cv::Mat y);

    cv::Mat getGradientX(vector<cv::Mat> gradient);

    cv::Mat getGradientY(vector<cv::Mat> gradient);

    bool hasOverlap(cv::Mat mat1, cv::Mat mat2);

    //General Functions

    void displayMatrix(string name, cv::Mat mat);

    cv::Mat drawContour(cv::Mat img, vector<cv::Point> contour);

    void displayMatrixList(string prefix, vector<cv::Mat> matrices);

    void printMatrix(string name, cv::Mat mat);

    float angleBetween(const cv::Point v1, const cv::Point v2);

    cv::Mat drawColoredContours(cv::Mat img, vector<vector<cv::Point>> *contours);

    cv::Mat* drawColoredContours(cv::Mat *img, vector<vector<cv::Point>> *contours, cv::RNG *rng);
}
#endif //SEGMENTERTOOLS_H
