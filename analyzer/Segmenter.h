#ifndef SEGMENTER_H
#define SEGMENTER_H

#include "objects/Clump.h"

using namespace std;

namespace segment {
    class Segmenter {

    public:
        // Quickshift params
        int kernelsize;
        int maxdist;
        // Canny params
        int threshold1;
        int threshold2;
        // GMM params
        int maxGmmIterations;
        // GMM post processing params
        double minAreaThreshold;
        // MSER params
        int delta, minArea, maxArea; //minArea was 120
        double maxVariation, minDiversity;
        double minCircularity;


    private:
        // internal attributes
        bool debug = true; //TODO - Add this as a command line arg
        cv::Scalar pink;
        int allContours = -1;
        bool totalTimed = true;


    public:
        Segmenter(int kernelsize, int maxdist, int thres1, int thres2, int maxGmmIterations, int minAreaThreshold,
                  int delta, int minArea, int maxArea, double maxVariation, double minDiversity, double minCircularity);

        void setCommonValues();

        void runSegmentation(string fileName);
        double* createNucleiHist(Image image, int delta, int minArea, int maxArea, double maxVariation, double minDiversity, double minCircularity, bool debug);
        void Segmenter::optimizeParameters(string filename1, string filename2);
    };
}

#endif //SEGMENTER_H
