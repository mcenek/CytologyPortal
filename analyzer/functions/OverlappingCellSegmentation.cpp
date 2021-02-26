#include "OverlappingCellSegmentation.h"
#include "opencv2/opencv.hpp"
#include "LSM.h"
#include "SegmenterTools.h"

namespace segment {
    cv::Mat calcAllBinaryEnergy(Cell *cellI, Clump *clump, cv::Mat dirac) {
        cv::Mat binaryEnergy = cv::Mat::zeros(cellI->phi.rows, cellI->phi.cols, CV_32FC1);
        for (unsigned int cellIdxJ = 0; cellIdxJ < clump->cells.size(); cellIdxJ++) {
            Cell *cellJ = &clump->cells[cellIdxJ];

            if (cellI == cellJ) {
                continue;
            }

            if (hasOverlap(cellI->phi, cellJ->phi)) {
                binaryEnergy += calcBinaryEnergy(cellJ->phi, clump->clumpPrior, clump->edgeEnforcer, dirac);

            }
        }
        return binaryEnergy;
    }

    cv::Mat calcBinaryEnergy(cv::Mat mat, cv::Mat clumpPrior, cv::Mat edgeEnforcer, cv::Mat dirac) {
        cv::Mat overAreaTerm;
        cv::Mat heaviside = calcHeaviside(mat);
        overAreaTerm = clumpPrior.mul(edgeEnforcer);
        overAreaTerm = overAreaTerm.mul(dirac);
        overAreaTerm = overAreaTerm.mul(heaviside);
        return overAreaTerm;
    }

    bool isConverged(Cell *cellI, int i) {
        double newPhiArea = cellI->getPhiArea();
        if (abs(cellI->phiArea - newPhiArea) < 50 || i > 2000) {
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


    /*
       Overlapping Segmentation (Distance Map)
       https://cs.adelaide.edu.au/~zhi/publications/paper_TIP_Jan04_2015_Finalised_two_columns.pdf
       http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.231.9150&rep=rep1&type=pdf
     */
    void runOverlappingSegmentation(vector<Clump> *clumps) {
        //Initialize set up parameters
        double dt = 5; //Time step
        double epsilon = 1.5; //Pixel spacing
        double mu = 0.04; //Contour length weighting parameter
        double kappa = 13;
        double chi = 3;

        for (unsigned int clumpIdx = 0; clumpIdx < clumps->size(); clumpIdx++) {
            Clump *clump = &(*clumps)[clumpIdx];
            clump->edgeEnforcer = calcEdgeEnforcer(padMatrix(clump->clumpMat, cv::Scalar(255, 255, 255)));
            clump->clumpPrior = padMatrix(clump->clumpPrior, cv::Scalar(255, 255, 255));
            for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
                Cell *cellI = &clump->cells[cellIdxI];
                cellI->phi = padMatrix(cellI->phi, 2);

            }

        }

        //Iterate over each clump
        for (unsigned int clumpIdx = 0; clumpIdx < clumps->size(); clumpIdx++) {
            Clump *clump = &(*clumps)[clumpIdx];
            int cellsConverged = 0;


            if (clump->finalCellContours.size() == clump->cells.size()) continue;

            //int i = 0;
            if (clump->cells.size() > 1) {
                //while (cellsConverged < clump->cells.size()) {
                for (int i = 0; i < 200; i++) {
                    for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
                        Cell *cellI = &clump->cells[cellIdxI];

                        if (cellI->phiConverged) {
                            continue;
                        }

                        updatePhi(cellI, clump, dt, epsilon, mu, kappa, chi);

                        cout << "LSF Iteration " << i << ": Clump " << clumpIdx << ", Cell " << cellIdxI << endl;

                        /*
                        if (i >= 50 && i % 50 == 0) {
                            if (isConverged(cellI, i)) {
                                cellsConverged++;
                                cout << "converged" << endl;


                            }
                        }
                         */
                    }
                    i++;
                }
            }

            vector<vector<cv::Point>> finalContours = clump->getFinalCellContours();

            for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
                vector<cv::Point> finalContour = finalContours[cellIdxI];
                std::ofstream outFile("../images/clump" + to_string(clumpIdx) + "cell" + to_string(cellIdxI) + ".txt");
                for (cv::Point point : finalContour) {
                    outFile << point.x << " " << point.y << endl;
                }
            }






        }

        //runPostOverlappingSegmentation(clumps);
    }

    void runPostOverlappingSegmentation(vector<Clump> *clumps) {

    }

    void updatePhi(Cell *cellI, Clump *clump, double dt, double epsilon, double mu, double kappa, double chi) {
        //displayMatrix("phi", cellI->phi);
        //cellI->phi = neumannBoundCond(cellI->phi);
        vector <cv::Mat> gradient = calcGradient(cellI->phi);
        cv::Mat gradientX = getGradientX(gradient);
        cv::Mat gradientY = getGradientY(gradient);
        cv::Mat magnitude = calcMagnitude(gradientX, gradientY);
        cv::Mat regularizer = calcSignedDistanceReg(cellI->phi);
        cv::Mat dirac = calcDiracDelta(cellI->phi, epsilon);
        cv::Mat gac = calcGeodesicTerm(dirac, gradient, clump->edgeEnforcer, clump->clumpPrior);
        cv::Mat binaryEnergy = calcAllBinaryEnergy(cellI, clump, dirac);

        cellI->phi += dt * (
                mu * regularizer +
                kappa * gac +
                chi * binaryEnergy
        );
    }
}