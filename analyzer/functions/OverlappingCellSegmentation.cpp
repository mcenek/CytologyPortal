#include "OverlappingCellSegmentation.h"
#include "../objects/ClumpsThread.h"
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
                        cellI->finalContour = cellI->getPhiContour();
                        cout << "converged" << endl;
                    }
                }

            }
            i++;
        }


        clump->edgeEnforcer.release();
        clump->clumpPrior.release();

        /*
        for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
            Cell *cell = &clump->cells[cellIdxI];
            cell->phi.release();
            cell->shapePrior.release();
            vector<cv::Point> finalContour = finalContours[cellIdxI];
            string fileStem = "clump" + to_string(clumpIdx) + "cell" + to_string(cellIdxI);
            boost::filesystem::path writePath = image->getWritePath(fileStem, ".txt");
            std::ofstream outFile(writePath.string());
            for (cv::Point point : finalContour) {
                outFile << point.x << " " << point.y << endl;
            }

        }
         */



    }


    void saveFinalCellBoundaries(json &finalCellBoundaries, Image *image, Clump *clump, int clumpIdx) {
        for (int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &(clump->cells[cellIdx]);
            json finalCellBoundary;
            for (cv::Point &point : cell->finalContour) {
                finalCellBoundary.push_back({point.x, point.y});
            }
            finalCellBoundaries[clumpIdx][cellIdx] = finalCellBoundary;
        }

        if (clumpIdx % 100 == 0) {
            image->writeJSON("finalCellBoundaries", finalCellBoundaries);
        }
    }

    void loadFinalCellBoundaries(json &finalCellBoundaries, Image *image, vector<Clump> *clumps) {
        finalCellBoundaries = image->loadJSON("finalCellBoundaries");
        for (int clumpIdx = 0; clumpIdx < finalCellBoundaries.size(); clumpIdx++) {
            Clump *clump = &(*clumps)[clumpIdx];
            json jsonClumpContours = finalCellBoundaries[clumpIdx];
            if (jsonClumpContours == nullptr) continue;
            int jsonNumberCells = finalCellBoundaries[clumpIdx].size();
            for (int cellIdx = 0; cellIdx < jsonNumberCells; cellIdx++) {
                json jsonCellContour = finalCellBoundaries[clumpIdx][cellIdx];
                // Will not load incomplete clumps
                if (jsonCellContour == nullptr) break;
                vector<cv::Point> contour;
                for (json &jsonPoint : jsonCellContour) {
                    int x = jsonPoint[0];
                    int y = jsonPoint[1];
                    cv::Point point = cv::Point(x, y);
                    contour.push_back(point);
                }
                Cell *cell = &clump->cells[cellIdx];
                cell->finalContour = contour;
                // Clump cyto boundaries are fully loaded
                if (cellIdx == jsonNumberCells - 1) {
                    clump->finalCellContoursLoaded = true;
                }
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

        json finalCellBoundaries;

        loadFinalCellBoundaries(finalCellBoundaries, image, clumps);


        for (unsigned int clumpIdx = 0; clumpIdx < clumps->size(); clumpIdx++) {
            Clump *clump = &(*clumps)[clumpIdx];
            clump->edgeEnforcer = drlse::calcEdgeEnforcer(padMatrix(clump->extract(), cv::Scalar(255, 255, 255)));
            clump->clumpPrior = padMatrix(clump->clumpPrior, cv::Scalar(255, 255, 255));
            for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
                Cell *cellI = &clump->cells[cellIdxI];
                cellI->phi = padMatrix(cellI->phi, 2);
            }

        }

        function<void(Clump *, int)> threadFunction = [&image](Clump *clump, int clumpIdx) {
            if (clump->finalCellContoursLoaded) {
                image->log("Loaded clump %u final cell boundaries from file\n", clumpIdx);
                return;
            }
            startOverlappingCellSegmentationThread(image, clump, clumpIdx);
        };

        function<void(Clump *, int)> threadDoneFunction = [&finalCellBoundaries, &image](Clump *clump, int clumpIdx) {
            if (!clump->finalCellContoursLoaded) {
                saveFinalCellBoundaries(finalCellBoundaries, image, clump, clumpIdx);
            }
        };

        int maxThreads = 8;
        ClumpsThread(maxThreads, clumps, threadFunction, threadDoneFunction);

        image->writeJSON("finalCellBoundaries", finalCellBoundaries);
    }

}