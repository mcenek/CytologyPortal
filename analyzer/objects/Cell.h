#ifndef SEGMENTER_CPP_CELL_H
#define SEGMENTER_CPP_CELL_H

#include "opencv2/opencv.hpp"
#include "Clump.h"


using namespace std;

namespace segment {
    class Clump; //forward declaration
    class Cell {
    public:
        Clump *clump;
        cv::Vec3b color;
        cv::Point nucleusCenter;
        cv::Point geometricCenter; //Currently unassigned
        vector <cv::Point> cytoBoundary;
        cv::Mat cytoMask;
        cv::Mat initCytoBoundaryMask; //TODO: Might be pointless.. Keep tabs
        cv::Mat nucleusMask; //Used to avoid re-computing the nucleiMasks
        vector <cv::Point> nucleusBoundary;
        cv::Mat interpolatedContour;
        cv::Mat shapePrior;
        cv::Mat phi; //Used to store the evolving LSF front
        vector<cv::Point> finalContour;

        double phiArea;
        double nucleusArea;
        bool phiConverged;


        float calcMaxRadius(); //Used for shape priors
        cv::Vec3b generateColor();

        cv::Point computeCenter();
        void initializePhi();
        double getPhiArea();
        vector<cv::Point> getPhiContour();
        cv::Mat calcShapePrior();


    };
}


#endif //SEGMENTER_CPP_CELL_H