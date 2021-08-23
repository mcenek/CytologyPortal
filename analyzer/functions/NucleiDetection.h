#ifndef NUCLEIDETECTION_H
#define NUCLEIDETECTION_H

#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"
#include "../thirdparty/nlohmann/json.hpp"

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
    vector<vector<cv::Point>> runMser(cv::Mat *img, vector<cv::Point> contour, int delta,
                                      int minArea, int maxArea, double maxVariation,
                                      double minDiversity, bool debug = false);

    void startNucleiDetectionThread(Clump *clump, int i, Image *image, int delta, int minArea, int maxArea, double maxVariation, double minDiversity,
                                    double minCircularity, bool debug);

    void saveNucleiBoundaries(json &nucleiBoundaries, Image *image, Clump *clump, int clumpIdx);

    void loadNucleiBoundaries(json &nucleiBoundaries, Image *image, vector<Clump> *clumps);

    void runNucleiDetection(Image *image, int delta, int minArea, int maxArea, double maxVariation, double minDiversity, double minCircularity, bool debug);
    cv::Mat runNucleiDetectionandMask(Image *image, int delta, int minArea, int maxArea, double maxVariation, double minDiversity, double minCircularity, bool debug, cv::Scalar intensity, bool test, cv::Mat testMat);
}


#endif //NUCLEIDETECTION_H
