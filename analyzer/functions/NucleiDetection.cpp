#include "NucleiDetection.h"
#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"
#include "SegmenterTools.h"

namespace segment {
    cv::Scalar pink = cv::Scalar(1.0, 0, 1.0);
    int allContours = -1;

    void removeClumpsWithoutNuclei(vector<Clump> *clumps) {
        // remove clumps that don't have any nuclei
        int tempindex = 0;
        unsigned int numchecked = 0;
        unsigned int originalsize = clumps->size();
        while (clumps->size() > 0 && numchecked < originalsize) {
            if ((*clumps)[tempindex].nucleiBoundaries.empty()) {
                clumps->erase(clumps->begin() + tempindex);
                tempindex--;
            }
            tempindex++;
            numchecked++;
        }
    }

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
                                      double minDiversity, bool debug) {
        //Create a MSER instance
        cv::Ptr<cv::MSER> mser = cv::MSER::create(delta, minArea, maxArea,
                                                  maxVariation, minDiversity);

        //Convert the src image to grayscale
        cv::Mat tmp;
        img.convertTo(tmp, CV_8U);
        cv::cvtColor(tmp, tmp, CV_BGR2GRAY); //TODO - Read the image in as gray, remove this step

        //Return variables
        vector<vector<cv::Point> > regions;
        vector<cv::Rect> mser_bbox;

        mser->detectRegions(tmp, regions, mser_bbox);

        // filter out regions that are outside the clump boundary
        // this is a bit of a hack, but there doesn't seem to be an easy way to make
        // cv::mser run on only a certain region within an image
        int i = 0;
        unsigned int numchecked = 0;
        unsigned int originalsize = regions.size();
        while (regions.size() > 0 && numchecked < originalsize) {
            for (cv::Point p : regions[i]) {
                if (cv::pointPolygonTest(contour, p, false) < 0) {
                    regions.erase(regions.begin() + i);
                    i = i - 1;
                    break;
                }
            }
            i = i + 1;
            numchecked++;
        }

        //TODO - "Regions found" is currently a redundant print
        /*if (debug) {
      printf("Regions found: %lu\n", regions.size());

      for (unsigned int i = 0; i < regions.size(); i++) {
        //Display region
        cv::Mat regionImg = cv::Mat::zeros(img.rows, img.cols, CV_8UC1);
        cv::drawContours(regionImg, regions, i, cv::Scalar(255), 1);
        cv::imshow("Region " + to_string(i), regionImg);
        cv::waitKey(0);

        //Report region area
        cout << cv::contourArea(regionImg) << endl;
      }
      }*/

        return regions;
    }

    cv::Mat runNucleiDetection(cv::Mat image, vector<Clump> *clumps, int delta, int minArea, int maxArea, double maxVariation, double minDiversity, bool debug) {
        for (unsigned int i = 0; i < clumps->size(); i++) {
            cv::Mat clump = (*clumps)[i].extract();

            cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(5);
            cv::cvtColor(clump, clump, CV_RGB2GRAY);
            //clahe->apply(clump, clump);
            cv::cvtColor(clump, clump, CV_GRAY2RGB);
            (*clumps)[i].nucleiBoundaries = runMser(clump, (*clumps)[i].computeOffsetContour(),
                                                 delta, minArea, maxArea, maxVariation,
                                                 minDiversity, debug);
            printf("Clump %u, nuclei boundaries found: %lu\n", i, (*clumps)[i].nucleiBoundaries.size());
        }

        // write all the found clumps with nuclei
        cv::Mat nucleiImg = postNucleiDetection(clumps);
        image.convertTo(nucleiImg, CV_64FC3);

        return nucleiImg;
    }

    cv::Mat postNucleiDetection(vector<Clump> *clumps) {
        removeClumpsWithoutNuclei(clumps);
        cv::Mat nucleiImg;
        for (unsigned int i = 0; i < clumps->size(); i++) {
            Clump *clump = &(*clumps)[i];
            cv::Mat clumpImg = clump->extract();
            cv::drawContours(nucleiImg, (*clumps)[i].undoOffsetContour(), allContours, pink, 2);
            clump->convertNucleiBoundariesToContours();
        }
        return nucleiImg;
    }
}