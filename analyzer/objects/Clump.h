#ifndef CLUMP_H
#define CLUMP_H

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
        cv::Mat mat;
        vector<cv::Point> contour;
        vector<cv::Point> offsetContour;
        cv::Rect boundingRect;
        vector<vector<cv::Point>> nucleiBoundaries;
        bool nucleiBoundariesLoaded = false;
        vector<cv::Point> nucleiCenters;
        vector<vector<cv::Point>> cytoBoundaries;
        bool initCytoBoundariesLoaded = false;
        cv::Mat nucleiAssocs;
        vector<cv::Mat> nucleiMasks; //Used to avoid re-computing the nucleiMasks
        vector<Cell> cells;
        cv::Mat clumpPrior;
        cv::Mat edgeEnforcer;
        bool finalCellContoursLoaded = false;

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
        vector<cv::Point> computeNucleusCenters();
        // Allows for the reversal of computeOffsetContour, as used to generate nuclei_boundaries.png
        vector<vector<cv::Point>> undoOffsetContour();
        vector<cv::Point> undoBoundingRect(vector<cv::Point> tmpBoundaries);
        void createCells();
        void convertNucleiBoundariesToContours();
        void filterNuclei(double threshold = 0.5);
        //void generateNucleiMasks();
        cv::Mat calcClumpPrior();
        vector<vector<cv::Point>> getFinalCellContours();
    };
}


#endif //CLUMP_H
