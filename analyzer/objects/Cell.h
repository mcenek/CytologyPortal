#ifndef CELL_H
#define CELL_H

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
        vector<Cell *> neighbors;
        vector<cv::Point> originalCytoBoundary;
        vector<cv::Point> cytoBoundary;
        vector<cv::Point> cytoAssocs;
        cv::Mat cytoMask;
        //cv::Mat nucleusMask; //Used to avoid re-computing the nucleiMasks
        vector<cv::Point> nucleusBoundary;
        cv::Rect boundingBox;
        cv::Rect boundingBoxWithNeighbors;
        cv::Mat phi; //Used to store the evolving LSF front
        vector<cv::Point> finalContour;

        double phiArea;
        double nucleusArea;
        bool phiConverged;


        float calcMaxRadius(); //Used for shape priors
        cv::Vec3b generateColor();

        cv::Point computeNucleusCenter();
        void generateBoundaryFromMask();
        void generateMaskFromBoundary();
        cv::Rect findBoundingBox();
        cv::Rect findBoundingBoxWithNeighbors();
        void initializePhi();
        cv::Mat getPhi(cv::Rect boundingBox);
        cv::Mat getPhi();
        void setPhi(cv::Mat phi);
        double getPhiArea();
        vector<cv::Point> getPhiContour();
        cv::Point calcGeometricCenter();
        cv::Mat calcShapePrior();


    };
}


#endif //CELL_H
