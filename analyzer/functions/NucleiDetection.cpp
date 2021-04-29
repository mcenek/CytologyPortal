#include "NucleiDetection.h"
#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"
#include "SegmenterTools.h"
#include <future>
#include <thread>
#include "../thirdparty/nlohmann/json.hpp"

using json = nlohmann::json;

namespace segment {

    void removeClumpsWithoutNuclei(vector<Clump> *clumps) {
        // remove clumps that don't have any nuclei
        int tempindex = 0;
        unsigned int numchecked = 0;
        unsigned int originalsize = clumps->size();

        while (clumps->size() > 0 && numchecked < originalsize) {
            Clump *clump = &(*clumps)[tempindex];
            if (clump->nucleiBoundaries.empty()) {
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
    vector<vector<cv::Point>> runMser(cv::Mat *img, vector<cv::Point> contour, int delta,
                                      int minArea, int maxArea, double maxVariation,
                                      double minDiversity, bool debug) {
        //Create a MSER instance
        cv::Ptr<cv::MSER> mser = cv::MSER::create(delta, minArea, maxArea,
                                                  maxVariation, minDiversity);

        //Convert the src image to grayscale
        cv::Mat tmp;
        img->convertTo(tmp, CV_8U);
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
      image->log("Regions found: %lu\n", regions.size());

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

    void startNucleiDetectionThread(Clump *clump, int i, Image *image, int delta, int minArea, int maxArea, double maxVariation, double minDiversity,
                                    double minCircularity, bool debug) {
        if (clump->nucleiBoundariesLoaded) {
            image->log("Loaded clump %u nuclei from file\n", i);
            return;
        }
        cv::Mat clumpMat = clump->extract();
        vector<vector<cv::Point>> nuclei = runMser(&clumpMat, clump->offsetContour,
                                                   delta, minArea, maxArea, maxVariation,
                                                   minDiversity, debug);
        clump->nucleiBoundaries = nuclei;
        if (clump->nucleiBoundaries.size() > 0) {
            clump->convertNucleiBoundariesToContours();
            clump->filterNuclei(minCircularity);
        }

        image->log("Clump %u, nuclei found: %lu\n", i, clump->nucleiBoundaries.size());
    }

    void saveNucleiBoundaries(json &nucleiBoundaries, Image *image, Clump *clump, int clumpIdx) {
        nucleiBoundaries[clumpIdx] = json::array();
        for (vector<cv::Point> &contour : clump->nucleiBoundaries) {
            json nucleiBoundary;
            for (cv::Point &point : contour) {
                nucleiBoundary.push_back({point.x, point.y});
            }
            nucleiBoundaries[clumpIdx].push_back(nucleiBoundary);
        }
        if (clumpIdx % 100 == 0) {
            image->writeJSON("nucleiBoundaries", nucleiBoundaries);
        }
    }

    void loadNucleiBoundaries(json &nucleiBoundaries, Image *image, vector<Clump> *clumps) {
        nucleiBoundaries = image->loadJSON("nucleiBoundaries");
        for (int clumpIdx = 0; clumpIdx < nucleiBoundaries.size(); clumpIdx++) {
            json jsonContours = nucleiBoundaries[clumpIdx];
            if (jsonContours == nullptr) continue;
            vector<vector<cv::Point>> contours;
            for (json &jsonContour : jsonContours) {
                vector<cv::Point> contour;
                for (json &jsonPoint : jsonContour) {
                    int x = jsonPoint[0];
                    int y = jsonPoint[1];
                    cv::Point point = cv::Point(x, y);
                    contour.push_back(point);
                }
                contours.push_back(contour);
            }
            Clump *clump = &(*clumps)[clumpIdx];
            clump->nucleiBoundaries = contours;
            clump->nucleiBoundariesLoaded = true;
        }

    }

    void runNucleiDetection(Image *image, int delta, int minArea, int maxArea, double maxVariation, double minDiversity,
                            double minCircularity, bool debug) {
        vector<Clump> *clumps = &image->clumps;
        vector<shared_future<void>> allThreads;
        vector<int> threadClumpId;

        json nucleiBoundaries;

        loadNucleiBoundaries(nucleiBoundaries, image, clumps);


        vector<Clump>::iterator clumpIterator = clumps->begin();
        int numThreads = 4;
        do {
            // Fill thread queue
            while (allThreads.size() < numThreads && clumpIterator < clumps->end()) {
                Clump *clump = &(*clumpIterator);
                int clumpIdx = clumpIterator - clumps->begin();
                clumpIterator++;
                shared_future<void> thread_object = async(startNucleiDetectionThread, clump, clumpIdx, image, delta, minArea, maxArea, maxVariation, minDiversity, minCircularity, debug);
                allThreads.push_back(thread_object);
                threadClumpId.push_back(clumpIdx);
            }

            // Wait for a thread to finish
            auto timeout = std::chrono::milliseconds(10);
            for (int i = 0; i < allThreads.size(); i++) {
                shared_future<void> thread = allThreads[i];
                int clumpIdx = threadClumpId[i];
                Clump *clump = &(*clumps)[clumpIdx];
                if (thread.valid() && thread.wait_for(timeout) == future_status::ready) {
                    thread.get();
                    if (!clump->nucleiBoundariesLoaded) {
                        saveNucleiBoundaries(nucleiBoundaries, image, clump, clumpIdx);
                    }
                    allThreads.erase(allThreads.begin() + i);
                    threadClumpId.erase(threadClumpId.begin() + i);
                    break;
                }
            }

        } while(allThreads.size() > 0 || clumpIterator < clumps->end());

        image->writeJSON("nucleiBoundaries", nucleiBoundaries);

        removeClumpsWithoutNuclei(clumps);

    }


}