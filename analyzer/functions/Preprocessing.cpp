#include <thread>
#include <future>
#include <chrono>
#include "../objects/Image.h"
#include "../objects/SubImage.h"
#include "ClumpSegmentation.h"

using namespace std;

namespace segment {
    /*
     * startPreprocessingThread is the main function that finds a mask of the clumps of an image
     * This function takes in a subimage of the image.
     * Preprocessing runs several algorithms:
     * 1) Quickshift
     * 2) Canny Edge Detection
     * 3) Compute Convex Hulls
     * 4) Gaussian Mixture Modeling
     */
    SubImage startProcessingThread(Image *image, SubImage subImage, int kernelsize, int maxDist, int threshold1, int threshold2, int maxGmmIterations) {
        bool debug = true;
        auto start = chrono::high_resolution_clock::now();
        double end;

        if (debug) image->log("Beginning quickshift...\n");

        cv::Mat postQuickShift = runQuickshift(&(subImage.mat), kernelsize, maxDist);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image->log("Quickshift complete, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image->log("Beginning Edge Detection...\n");

        cv::Mat postEdgeDetection = runCanny(postQuickShift, threshold1, threshold2, true);
        //image.writeImage("edgeDetectedEroded_cyto.png", postEdgeDetection);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image->log("Edge Detection complete, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image->log("Beginning CCA and building convex hulls...\n");

        // find contours
        vector <vector<cv::Point>> contours;

        cv::findContours(postEdgeDetection, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

        // find convex hulls
        vector <vector<cv::Point>> hulls = runFindConvexHulls(contours);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image->log("Finished with CCA and convex hulls, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image->log("Beginning Gaussian Mixture Modeling...\n");

        cv::Mat gmmPredictions = runGmm(&(subImage.mat), hulls, maxGmmIterations);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image->log("Finished with Gaussian Mixture Modeling, time: %f\n", end);

        subImage.mat = gmmPredictions;

        return subImage;
    }

    /*
     * runPreprocessing is the main function that finds a mask of the clumps of the image
     * This function spawns multiple threads for each subimage that finds the mask of the clumps of the image.
     */
    cv::Mat runPreprocessing(Image *image, int kernelsize, int maxdist, int threshold1, int threshold2, int maxGmmIterations) {
        //Number of horizontal and vertical subimages
        //Total number of subimages is subMatNumX * subMatNumY
        const int subMatNumX = 1;
        const int subMatNumY = 1;
        const int numThreads = 16;

        cv::Mat *mat = &image->mat;
        //Percentage of overlap padding between subimages
        double paddingWidth = 0.30;
        double paddingHeight = 0.30;
        vector<SubImage> subImages = splitMat(mat, subMatNumX, subMatNumY, paddingWidth, paddingHeight);
        cv::Mat returnMatrixArray[subMatNumX][subMatNumY];

        auto start = chrono::high_resolution_clock::now();

        // Below is similar to ClumpThread, but runs on a list of SubImages instead of Clumps
        vector<shared_future<SubImage>> allThreads;

        do {
            // Fill thread queue
            while (allThreads.size() < numThreads && subImages.size() > 0) {
                SubImage subImage = subImages.front();
                subImages.erase(subImages.begin());
                shared_future<SubImage> thread_object = async(&startProcessingThread, image, subImage, kernelsize, maxdist, threshold1, threshold2, maxGmmIterations);
                allThreads.push_back(thread_object);
            }

            // Wait for a thread to finish
            auto timeout = std::chrono::milliseconds(10);
            for (int i = 0; i < allThreads.size(); i++) {
                shared_future<SubImage> thread = allThreads[i];
                if (thread.valid() && thread.wait_for(timeout) == future_status::ready) {
                    SubImage subImage = thread.get();
                    allThreads.erase(allThreads.begin() + i);
                    cv::Mat cropped = subImage.undoPadding();
                    returnMatrixArray[subImage.i][subImage.j] = cropped;
                    break;
                }
            }

        } while(allThreads.size() > 0 || subImages.size() > 0);

        // Stich the subimages back together
        cv::Mat verticalMatrices[subMatNumX];
        for (int i = 0; i < subMatNumX; i++) {
            cv::Mat verticalMatrix;
            cv::vconcat(returnMatrixArray[i], subMatNumY, verticalMatrix);
            verticalMatrices[i] = verticalMatrix;
        }
        cv::Mat fullMat;
        cv::hconcat(verticalMatrices, subMatNumX, fullMat);

        double end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        printf("Preprocessing time: %f", end);

        cv::Mat temp;
        cv::threshold(fullMat, temp, 0, 256, CV_THRESH_BINARY);
        image->writeImage("gmmPredictions.png", temp);

        return fullMat;
    }




}
