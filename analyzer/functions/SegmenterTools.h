#ifndef SEGMENTERTOOLS_H
#define SEGMENTERTOOLS_H

using namespace std;

namespace segment {

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
