#ifndef SEGMENTER_CPP_IMAGE_H
#define SEGMENTER_CPP_IMAGE_H


#include "opencv2/opencv.hpp"
#include "Clump.h"

using namespace std;

namespace segment {
    class Clump; //forward declaration
    class Image {
    public:
        cv::Mat mat;

        cv::Mat matPadded;
        int padding;
        string path;
        vector<Clump> clumps;

        Image(string path);

        cv::Mat padMatrix();
        cv::Mat readMatrix(string path);
        void createClumps(vector<vector<cv::Point>> clumpBoundaries);

        void showFinalResults();
    };
}

#endif //SEGMENTER_CPP_IMAGE_H
