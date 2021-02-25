#ifndef SEGMENTER_CPP_CLUMP_H
#define SEGMENTER_CPP_CLUMP_H

#include "Cell.h"
#include "opencv2/opencv.hpp"
#include "Image.h"


using namespace std;

namespace segment {
    class Image; //forward declaration
    class Cell; //forward declaration
    class Clump {
    public:
        // attributes
        Image *image;
        cv::Mat clumpMat;
        vector<cv::Point> contour;
        vector<cv::Point> offsetContour;
        cv::Rect boundingRect;
        vector<vector<cv::Point>> nucleiBoundaries;
        vector<cv::Point> nucleiCenters;
        vector<vector<cv::Point>> cytoBoundaries;
        cv::Mat nucleiAssocs;
        vector<cv::Mat> nucleiMasks; //Used to avoid re-computing the nucleiMasks
        vector<Cell> cells;
        cv::Mat clumpPrior;
        cv::Mat edgeEnforcer;
        vector<vector<cv::Point>> finalCellContours;

        // member functions
        // Given that imgptr and contour are defined, compute the bounding rect
        cv::Rect computeBoundingRect(cv::Mat image);
        // Given the contour and bounding rect are defined, find the contour offset
        // by the bounding rect
        vector<cv::Point> computeOffsetContour();
        // Mask the clump from the original image, return the result
        cv::Mat extractFull(bool showBoundary=false);
        // mask the clump from the image, then return image cropped to show only the clump
        cv::Mat extract(bool showBoundary=false);
        // If nucleiBoundaries are defined, compute the center of each nuclei
        vector<cv::Point> computeCenters();
        // Allows for the reversal of computeOffsetContour, as used to generate nuclei_boundaries.png
        vector<vector<cv::Point>> undoOffsetContour();
        vector<cv::Point> undoBoundingRect(vector<cv::Point> tmpBoundaries);
        void createCells();
        void convertNucleiBoundariesToContours();
        void generateNucleiMasks();
        cv::Mat calcClumpPrior();
        vector<vector<cv::Point>> getFinalCellContours();
    };
}


#endif //SEGMENTER_CPP_CLUMP_H