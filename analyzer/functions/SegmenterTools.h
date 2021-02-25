#ifndef SEGMENTER_CPP_SEGMENTERTOOLS_H
#define SEGMENTER_CPP_SEGMENTERTOOLS_H

using namespace std;

namespace segment {

    //General Functions

    void displayMatrix(string name, cv::Mat mat);

    cv::Mat drawContour(cv::Mat img, vector<cv::Point> contour);

    void displayMatrixList(string prefix, vector<cv::Mat> matrices);

    void printMatrix(string name, cv::Mat mat);

    float angleBetween(const cv::Point v1, const cv::Point v2);

}
#endif //SEGMENTER_CPP_SEGMENTERTOOLS_H
