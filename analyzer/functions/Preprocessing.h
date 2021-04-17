#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#include "../objects/SubImage.h"
#include "../objects/Image.h"

using namespace std;

namespace segment {
    vector<SubImage> startProcessingThread(Image *image, SubImage subImage, int kernelsize, int maxdist, int threshold1, int threshold2, int maxGmmIterations);
    cv::Mat runPreprocessing(Image *image, int kernelsize, int maxdist, int threshold1, int threshold2, int maxGmmIterations);
    vector<SubImage> splitMat(cv::Mat *mat, int numberSubMatX, int numberSubMatY);
    cv::Mat crop(cv::Mat *mat, int x, int y, int width, int height, int paddingWidth, int paddingHeight);
}
#endif //PREPROCESSING_H
