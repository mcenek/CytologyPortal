#ifndef IMAGE_H
#define IMAGE_H

#include "opencv2/opencv.hpp"
#include "Clump.h"
#include "boost/filesystem.hpp"

using namespace std;

namespace segment {
    class Clump; //forward declaration
    class Image {
    public:
        cv::Mat mat;

        cv::Mat matPadded;
        int padding;

        boost::filesystem::path path;
        vector<Clump> clumps;

        Image(string path);

        cv::Mat padMatrix();
        cv::Mat readMatrix();
        void createClumps(vector<vector<cv::Point>> clumpBoundaries);

        void showFinalResults();
    };
}

#endif //IMAGE_H
