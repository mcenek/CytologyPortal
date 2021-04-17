#include "OverlappingCellSegmentation.h"
#include "opencv2/opencv.hpp"
#include "DRLSE.h"
#include "SegmenterTools.h"
#include "boost/filesystem.hpp"
#include <thread>
#include <future>

namespace segment {

    bool isConverged(Cell *cellI) {
        double newPhiArea = cellI->getPhiArea();
        if (abs(cellI->phiArea - newPhiArea) < 50) {
            cellI->phiConverged = true;

        }
        cellI->phiArea = newPhiArea;
        return cellI->phiConverged;
    }

    bool clumpHasSingleCell(Clump *clump) {
        if (clump->cells.size() == 1) {
            Cell *cellI = &clump->cells[0];
            cellI->phiConverged = true;
            return true;
        }
        return false;
    }

    cv::Mat padMatrix(cv::Mat mat, cv::Scalar value) {
        int padding = 10;
        cv::Mat paddedMatrix;
        cv::copyMakeBorder(mat, paddedMatrix, padding, padding, padding, padding, cv::BORDER_CONSTANT, value);
        return paddedMatrix;
    }

    void startOverlappingCellSegmentationThread(Image *image, Clump *clump, int clumpIdx) {
        //Initialize set up parameters
        double dt = 5; //Time step
        double epsilon = 1.5; //Pixel spacing
        double mu = 0.04; //Contour length weighting parameter
        double kappa = 13;
        double chi = 3;

        int cellsConverged = 0;

        if (clumpHasSingleCell(clump)) cellsConverged = clump->cells.size();
        if (clump->finalCellContours.size() == clump->cells.size()) return;

        int i = 0;
        while (cellsConverged < clump->cells.size() && i < 1000) {
            for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
                Cell *cellI = &clump->cells[cellIdxI];

                if (cellI->phiConverged) {
                    continue;
                }

                drlse::updatePhi(cellI, clump, dt, epsilon, mu, kappa, chi);

                cout << "LSF Iteration " << i << ": Clump " << clumpIdx << ", Cell " << cellIdxI << endl;

                if (i != 0 && i % 50 == 0) {
                    if (isConverged(cellI)) {
                        cellsConverged++;
                        cout << "converged" << endl;
                    }
                }

            }
            i++;
        }
        vector<vector<cv::Point>> finalContours = clump->getFinalCellContours();

        for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
            vector<cv::Point> finalContour = finalContours[cellIdxI];
            string fileStem = "clump" + to_string(clumpIdx) + "cell" + to_string(cellIdxI);
            boost::filesystem::path writePath = image->getWritePath(fileStem, ".txt");
            std::ofstream outFile(writePath.string());
            for (cv::Point point : finalContour) {
                outFile << point.x << " " << point.y << endl;
            }
        }
    }

    /*
       Overlapping Segmentation (Distance Map)
       https://cs.adelaide.edu.au/~zhi/publications/paper_TIP_Jan04_2015_Finalised_two_columns.pdf
       http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.231.9150&rep=rep1&type=pdf
     */
    void runOverlappingSegmentation(Image *image) {
        vector<Clump> *clumps = &image->clumps;


        for (unsigned int clumpIdx = 0; clumpIdx < clumps->size(); clumpIdx++) {
            Clump *clump = &(*clumps)[clumpIdx];
            clump->edgeEnforcer = drlse::calcEdgeEnforcer(padMatrix(clump->clumpMat, cv::Scalar(255, 255, 255)));
            clump->clumpPrior = padMatrix(clump->clumpPrior, cv::Scalar(255, 255, 255));
            for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
                Cell *cellI = &clump->cells[cellIdxI];
                cellI->phi = padMatrix(cellI->phi, 2);
            }

        }

        vector<shared_future<void>> allThreads;
        vector<Clump>::iterator clumpIterator = clumps->begin();
        int numThreads = 16;
        do {
            // Fill thread queue
            while (allThreads.size() < numThreads && clumpIterator < clumps->end()) {
                Clump *clump = &(*clumpIterator);
                int clumpIdx = clumpIterator - clumps->begin();
                clumpIterator++;
                shared_future<void> thread_object = async(&startOverlappingCellSegmentationThread, image, clump, clumpIdx);
                allThreads.push_back(thread_object);
            }

            // Wait for a thread to finish
            auto timeout = std::chrono::milliseconds(10);
            for (int i = 0; i < allThreads.size(); i++) {
                shared_future<void> thread = allThreads[i];
                if (thread.valid() && thread.wait_for(timeout) == future_status::ready) {
                    thread.get();
                    allThreads.erase(allThreads.begin() + i);
                    break;
                }
            }

        } while(allThreads.size() > 0 || clumpIterator < clumps->end());

    }

}