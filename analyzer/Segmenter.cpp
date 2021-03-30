#include <iostream>
#include <ctime>
#include <stdio.h>
#include <filesystem>
#include <chrono>
#include "Segmenter.h"
#include "opencv2/opencv.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "objects/Clump.h"
#include "objects/Cell.h"
#include "objects/Image.h"
#include "functions/DRLSE.h"
#include "functions/EvaluateSegmentation.h"
#include "functions/SegmenterTools.h"
#include "functions/ClumpSegmentation.h"
#include "functions/InitialCellSegmentation.h"
#include "functions/NucleiDetection.h"
#include "functions/OverlappingCellSegmentation.h"

extern "C" {
#include "vl/quickshift.h"
}

using namespace std;

namespace segment {
    // Constructor
    Segmenter::Segmenter(int kernelsize, int maxdist, int thres1, int thres2, int maxGmmIterations,
                         int minAreaThreshold, int delta, int minArea, int maxArea, double maxVariation,
                         double minDiversity, double minCircularity) {
        setCommonValues();
        this->kernelsize = kernelsize;
        this->maxdist = maxdist;
        this->maxGmmIterations = maxGmmIterations;
        this->threshold1 = thres1;
        this->threshold2 = thres2;
        this->minAreaThreshold = minAreaThreshold;
        this->delta = delta;
        this->minArea = minArea;
        this->maxArea = maxArea;
        this->maxVariation = maxVariation;
        this->minDiversity = minDiversity;
        this->minCircularity = minCircularity;
    }

    // Constructor helper
    void Segmenter::setCommonValues() {
        pink = cv::Scalar(1.0, 0, 1.0);
    }


    void Segmenter::runSegmentation(string fileName) {
        debug = true;
        auto total = chrono::high_resolution_clock::now();

        double end;

        Image image = Image(fileName);

        cv::Mat outimg;


        auto startClumpSeg = chrono::high_resolution_clock::now();
        auto start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning quickshift...\n");

        cv::Mat postQuickShift = image.loadMatrix("quickshifted_cyto.yml");
        if (postQuickShift.empty()) {
            postQuickShift = runQuickshift(&image, kernelsize, maxdist);
            image.writeMatrix("quickshifted_cyto.yml", postQuickShift);
            image.writeImage("quickshifted_cyto.png", postQuickShift);
        }

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Quickshift complete, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning Edge Detection...\n");

        cv::Mat postEdgeDetection = runCanny(postQuickShift, threshold1, threshold2, true);
        image.writeImage("edgeDetectedEroded_cyto.png", postEdgeDetection);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Edge Detection complete, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning CCA and building convex hulls...\n");

        // find contours
        vector <vector<cv::Point>> contours;

        cv::findContours(postEdgeDetection, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

        image.mat.copyTo(outimg);
        outimg.convertTo(outimg, CV_64FC3);
        cv::drawContours(outimg, contours, allContours, pink);


        image.writeImage("contours_cyto.png", outimg);


        // find convex hulls
        vector <vector<cv::Point>> hulls = runFindConvexHulls(contours);

        image.mat.copyTo(outimg);
        outimg.convertTo(outimg, CV_64FC3);
        cv::drawContours(outimg, hulls, allContours, pink);

        image.writeImage("hulls_cyto.png", outimg);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished with CCA and convex hulls, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning Gaussian Mixture Modeling...\n");

        cv::Mat gmmPredictions = image.loadMatrix("gmmPredictions.yml");
        if (gmmPredictions.empty()) {
            gmmPredictions = runGmm(image.mat, hulls, maxGmmIterations);
            image.writeMatrix("gmmPredictions.yml", gmmPredictions);

            cv::Mat temp;
            cv::threshold(gmmPredictions, temp, 0, 256, CV_THRESH_BINARY);
            image.writeImage("gmmPredictions.png", temp);
        }


        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished with Gaussian Mixture Modeling, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning GMM Output post processing...\n");

        vector <vector<cv::Point>> clumpBoundaries = findFinalClumpBoundaries(gmmPredictions, minAreaThreshold);


        outimg = drawColoredContours(image.mat, &clumpBoundaries);
        image.writeImage("clump_boundaries.png", outimg);
        if (debug) {
            //cv::imshow("Clump Segmentation", outimg);
            //cv::waitKey(0);
        }

        // extract each clump from the original image
        image.createClumps(clumpBoundaries);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        unsigned int numClumps = clumpBoundaries.size();
        if (debug) image.log("Finished GMM post processing, clumps found:%i, time: %f\n", numClumps, end);

        double endClumpSeg = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - startClumpSeg).count() / 1000000.0;
        if (debug) image.log("Finished clump segmentation, time: %f\n", endClumpSeg);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning MSER nuclei detection...\n");

        cv::Mat nucleiImg = runNucleiDetection(&image, delta, minArea, maxArea, maxVariation, minDiversity, minCircularity, debug);

        //Save a snapshot of nuclei across all clumps
        image.writeImage("nuclei_boundaries.png", nucleiImg);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished MSER nuclei detection, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning initial cell segmentation...\n");

        cv::Mat initialCells = runInitialCellSegmentation(&image, threshold1, threshold2, debug);
        image.writeImage("initial_cell_boundaries.png", initialCells);
        if (debug) {
            //cv::imshow("Initial Cell Segmentation", initialCells);
            //cv::waitKey(0);
        }

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished initial cell segmentation, time: %f\n", end);

        // print the nuclei sizes
        fstream nucleiSizes("nucleiSizes.txt");
        for (unsigned int c = 0; c < image.clumps.size(); c++) {
            Clump *clump = &image.clumps[c];
            for (vector <cv::Point> nuclei : clump->nucleiBoundaries) {
                nucleiSizes << cv::contourArea(nuclei) << endl;
            }
        }
        nucleiSizes.close();


        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning segmentation by level set functions...\n");

        runOverlappingSegmentation(&image);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished level set cell segmentation, time: %f\n", end);
        //displayResults(&image.clumps);


        // clean up
        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - total).count() / 1000000.0;

        if (debug || totalTimed) image.log("Segmentation finished, total time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning segmentation evaluation...\n");
        double dice = evaluateSegmentation(&image);
        image.log("Dice coefficient: %f\n", dice);
        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished segmentation evaluation, time: %f\n", end);

        outimg = image.getFinalResult();
        image.writeImage("cell_boundaries.png", outimg);
        if (debug) {
            //cv::imshow("Overlapping Cell Segmentation", outimg);
            //cv::waitKey(0);
        }
    }


}

