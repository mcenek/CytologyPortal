#ifndef SUBIMAGE_H
#define SUBIMAGE_H
#include "opencv2/opencv.hpp"

namespace segment {

    class SubImage {
    public:
        cv::Mat mat;
        int i;
        int j;
        int maxI;
        int maxJ;
        int paddingWidth;
        int paddingHeight;
        SubImage(cv::Mat *mat, int i, int j, int subMatWidth, int subMatHeight, int paddingWidth, int paddingHeight);
        cv::Mat undoPadding();
        cv::Mat crop(cv::Mat *mat, int x, int y, int width, int height, int paddingWidth, int paddingHeight);
    };
}


#endif //SUBIMAGE_H