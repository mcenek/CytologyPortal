#ifndef NUCLEIDETECTION_H
#define NUCLEIDETECTION_H

#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"

using namespace std;

namespace segment {
    void generateNucleiMasks(Clump *clump);

    /*
      runMser takes an image and params and runs MSER algorithm on it, for nuclei detection
      Return:
      vector<vector<cv::Point> > = stable regions found
      Params:
      cv::Mat img = the image
      int delta = the # of iterations a region must remain stable
      int minArea = the minimum number of pixels for a viable region
      int maxArea = the maximum number of pixels for a viable region
      double maxVariation = the max amount of variation allowed in regions
      double minDiversity = the min diversity allowed in regions
    */
    vector<vector<cv::Point>> runMser(cv::Mat img, vector<cv::Point> contour, int delta,
                                      int minArea, int maxArea, double maxVariation,
                                      double minDiversity, bool debug = false);

    cv::Mat runNucleiDetection(cv::Mat image, vector<Clump>* clumps, int delta, int minArea, int maxArea, double maxVariation, double minDiversity, bool debug);
    cv::Mat postNucleiDetection(cv::Mat image, vector<Clump> *clumps);
}


#endif //NUCLEIDETECTION_H
