#include <iostream>
#include <ctime>
#include <stdio.h>
#include <filesystem>
#include "opencv2/opencv.hpp"
#include "opencv2/features2d/features2d.hpp"
#include "objects/Clump.h"
#include "objects/Cell.h"
#include "objects/Image.h"
#include "functions/LSM.h"
#include "functions/EvaluateSegmentation.h"
#include "functions/SegmenterTools.h"
#include "functions/ClumpSegmentation.h"
#include "functions/InitialCellSegmentation.h"
#include "functions/NucleiDetection.h"
#include "functions/OverlappingCellSegmentation.h"
#include <chrono>

extern "C" {
#include "vl/quickshift.h"
}

using namespace std;

namespace segment {
    class Segmenter {
    public:
        // Quickshift params
        int kernelsize = 2;
        int maxdist = 4;
        // Canny params
        int threshold1 = 20;
        int threshold2 = 40;
        // GMM params
        int maxGmmIterations = 10;
        // GMM post processing params
        double minAreaThreshold = 1000.0;
        // MSER params
        int delta = 9, minArea = 500, maxArea = 600; //minArea was 120
        double maxVariation = 0.5, minDiversity = 0.25;
        // Cell segmentation params - TODO: reference these
        double mi = 0.2;
        double eta = 0.00000001;
        double ni = FLT_MIN;
        int lambda1 = 1;
        int lambda2 = 1;
        int eps = 1;
        double dt = 0.5; // timestep
        int steps = 2;

    private:
        // internal attributes
        bool debug = true; //TODO - Add this as a command line arg
        cv::Scalar pink; //TODO - Qualify pink here
        int allContours = -1;
        bool totalTimed = true;


    public:
        // Constructors
        Segmenter() { setCommonValues(); }

        Segmenter(int kernelsize, int maxdist) {
            setCommonValues();
            this->kernelsize = kernelsize;
            this->maxdist = maxdist;
        }

        Segmenter(int kernelsize, int maxdist, int thres1, int thres2, int maxGmmIterations, int minAreaThreshold,
                  int delta, int minArea, int maxArea, double maxVariation, double minDiversity) {
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
        }

        // Constructor helper
        void setCommonValues() {
            pink = cv::Scalar(1.0, 0, 1.0);
        }

        void displayResults(vector<Clump> *clumps) {
            for (unsigned int clumpIdx = 0; clumpIdx < clumps->size(); clumpIdx++) {
                Clump *clump = &(*clumps)[clumpIdx];
                for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
                    Cell *cellI = &clump->cells[cellIdxI];

                    cout << "Clump " << clumpIdx << ", Cell " << cellIdxI << endl;
                    cout << "\t" << "Nucleus Area: " << cellI->nucleusArea << endl;
                    cout << "\t" << "Phi Area: " << cellI->phiArea << endl;
                    cout << "\t" << "Nucleus/Phi Area Ratio: " << cellI->nucleusArea/cellI->phiArea << endl;

                    cv::imshow("Clump " + to_string(clumpIdx) + ", " + "Cell " + to_string(cellIdxI), cellI->phi);
                    cv::waitKey(0);

                }
            }
        }

        cv::Mat loadMatrix(const char *path) {
            cv::Mat mat;
            FILE *file;
            if ((file = fopen(path, "r"))) {
                fclose(file);
                cv::FileStorage fs(path, cv::FileStorage::READ);
                fs["mat"] >> mat;
                fs.release();
                if (debug) {
                    cout << "Loaded from file: " << path << endl;
                }
            }
            return mat;
        }

        void saveMatrix(const char *path, cv::Mat mat) {
            cv::FileStorage fs(path, cv::FileStorage::WRITE);
            fs << "mat" << mat;
            fs.release();
        }

        void runSegmentation(string fileName) {
            auto total = chrono::high_resolution_clock::now();

            double end;

            Image image = Image(fileName);

            cv::Mat outimg;


            auto startClumpSeg = chrono::high_resolution_clock::now();
            auto start = chrono::high_resolution_clock::now();
            if (debug) printf("Beginning quickshift...\n");

            cv::Mat postQuickShift = loadMatrix("../images/quickshifted_cyto.yml");
            if (postQuickShift.empty()) {
                postQuickShift = runQuickshift(image.mat, kernelsize, maxdist);
                saveMatrix("../images/quickshifted_cyto.yml", postQuickShift);
                cv::imwrite("../images/quickshifted_cyto.png", postQuickShift);
            }
            
            end = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-start).count() / 1000000.0;
            if (debug) printf("Quickshift complete, time: %f\n", end);

            start = chrono::high_resolution_clock::now();
            if (debug) printf("Beginning Edge Detection...\n");

            cv::Mat postEdgeDetection = runCanny(postQuickShift, threshold1, threshold2, true);
            cv::imwrite("../images/edgeDetectedEroded_cyto.png", postEdgeDetection);

            end = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-start).count() / 1000000.0;
            if (debug) printf("Edge Detection complete, time: %f\n", end);

            start = chrono::high_resolution_clock::now();
            if (debug) printf("Beginning CCA and building convex hulls...\n");

            // find contours
            vector<vector<cv::Point>> contours;

            cv::findContours(postEdgeDetection, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

            image.mat.copyTo(outimg);
            outimg.convertTo(outimg, CV_64FC3);
            cv::drawContours(outimg, contours, allContours, pink);



            cv::imwrite("../images/contours_cyto.png", outimg);


            // find convex hulls
            vector<vector<cv::Point>> hulls = runFindConvexHulls(contours);

            image.mat.copyTo(outimg);
            outimg.convertTo(outimg, CV_64FC3);
            cv::drawContours(outimg, hulls, allContours, pink);

            cv::imwrite("../images/hulls_cyto.png", outimg);

            end = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-start).count() / 1000000.0;
            if (debug) printf("Finished with CCA and convex hulls, time: %f\n", end);

            start = chrono::high_resolution_clock::now();
            if (debug) printf("Beginning Gaussian Mixture Modeling...\n");

            cv::Mat gmmPredictions = loadMatrix("../images/gmmPredictions.yml");
            if (gmmPredictions.empty()) {
                gmmPredictions = runGmm(image.mat, hulls, maxGmmIterations);
                saveMatrix("../images/gmmPredictions.yml", gmmPredictions);

                cv::Mat temp;
                cv::threshold(gmmPredictions, temp, 0, 256, CV_THRESH_BINARY);
                cv::imwrite("../images/gmmPredictions.png", temp);
            }



            end = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-start).count() / 1000000.0;
            if (debug) printf("Finished with Gaussian Mixture Modeling, time: %f\n", end);

            start = chrono::high_resolution_clock::now();
            if (debug) printf("Beginning GMM Output post processing...\n");

            vector<vector<cv::Point>> clumpBoundaries = findFinalClumpBoundaries(gmmPredictions, minAreaThreshold);

            outimg = drawColoredContours(image.mat, &clumpBoundaries);
            cv::imwrite("../images/clump_boundaries.png", outimg);
            //cv::imshow("Clump Segmentation", outimg);
            //cv::waitKey(0);

            // extract each clump from the original image
            image.createClumps(clumpBoundaries);

            end = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-start).count() / 1000000.0;
            unsigned int numClumps = clumpBoundaries.size();
            if (debug) printf("Finished GMM post processing, clumps found:%i, time: %f\n", numClumps, end);

            double endClumpSeg = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-startClumpSeg).count() / 1000000.0;
            if (debug) printf("Finished clump segmentation, time: %f\n", endClumpSeg);

            start = chrono::high_resolution_clock::now();
            if (debug) printf("Beginning MSER nuclei detection...\n");

            cv::Mat nucleiImg = runNucleiDetection(image.mat, &image.clumps, delta, minArea, maxArea, maxVariation, minDiversity, debug);

            //Save a snapshot of nuclei across all clumps
            cv::imwrite("../images/nuclei_boundaries.png", nucleiImg);

            end = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-start).count() / 1000000.0;
            if (debug) printf("Finished MSER nuclei detection, time: %f\n", end);

            start = chrono::high_resolution_clock::now();
            if (debug) printf("Beginning initial cell segmentation...\n");


            cv::Mat initialCells = runInitialCellSegmentation(&image, threshold1, threshold2, debug);
            //cv::imwrite("../images/initial_cell_boundaries.png", initialCells);
            //cv::imshow("Initial Cell Segmentation", initialCells);
            //cv::waitKey(0);

            end = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-start).count() / 1000000.0;
            if (debug) printf("Finished initial cell segmentation, time: %f\n", end);

            // print the nuclei sizes
            fstream nucleiSizes("nucleiSizes.txt");
            for (unsigned int c = 0; c < image.clumps.size(); c++) {
                Clump *clump = &image.clumps[c];
                for (vector<cv::Point> nuclei : clump->nucleiBoundaries) {
                    nucleiSizes << cv::contourArea(nuclei) << endl;
                }
            }
            nucleiSizes.close();


            start = chrono::high_resolution_clock::now();
            if (debug) printf("Beginning segmentation by level set functions...\n");

            runOverlappingSegmentation(&image.clumps);

            end = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-start).count() / 1000000.0;
            if (debug) printf("Finished level set cell segmentation, time: %f\n", end);
            //displayResults(&image.clumps);


            // clean up
            end = std::chrono::duration_cast<std::chrono::microseconds>(chrono::high_resolution_clock::now()-total).count() / 1000000.0;

            if (debug || totalTimed) printf("Segmentation finished, total time: %f\n", end);

            double dice = evaluateSegmentation(&image);
            cout << dice << endl;

            image.showFinalResults();


        }
    };


}

