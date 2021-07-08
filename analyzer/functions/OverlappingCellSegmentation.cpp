#include "OverlappingCellSegmentation.h"
#include "../objects/ClumpsThread.h"
#include "DRLSE.h"

namespace segment {

    /*
     * isConverged returns true if a cell's phi has converged and false otherwise
     */
    bool isConverged(Cell *cellI) {
        double newPhiArea = cellI->getPhiArea();
        // If less than 50 pixels have changed, then it is considered converged
        if (abs(cellI->phiArea - newPhiArea) < 50) {
            cellI->phiConverged = true;
        }
        cellI->phiArea = newPhiArea;
        return cellI->phiConverged;
    }

    /*
     * clumpHasSingleCell returns true if there is only one cell in the clump.
     * If so, then it is marked as converged.
     */
    bool clumpHasSingleCell(Clump *clump) {
        if (clump->cells.size() == 1) {
            Cell *cellI = &clump->cells[0];
            cellI->finalContour = cellI->getPhiContour();
            // Cell is converged since its boundary is the clump boundary
            cellI->phiConverged = true;
            return true;
        }
        return false;
    }

    /*
     * padMatrix adds a padding of 10 to all sides of the specified matrix with the specified value / intensity
     */
    cv::Mat padMatrix(cv::Mat mat, cv::Scalar value) {
        int padding = 10;
        cv::Mat paddedMatrix;
        cv::copyMakeBorder(mat, paddedMatrix, padding, padding, padding, padding, cv::BORDER_CONSTANT, value);
        return paddedMatrix;
    }

    /*
     * startOverlappingCellSegmentationThread is the main function that finds the final cell boundaries on a clump
     * We run the Distance Regulated Level Set Evolution (DRLSE) Algortithm.
     * We check every 50 loops or iterations for convergence
     */
    void startOverlappingCellSegmentationThread(Image *image, Clump *clump, int clumpIdx) {
        //Initialize set up parameters
        double dt = 5; //Time step
        double epsilon = 1.5; //Pixel spacing
        double mu = 0.04; //Contour length weighting parameter
        double kappa = 13;
        double chi = 3;

        int cellsConverged = 0;

        // Mark as converged if clump only has one cell
        if (clumpHasSingleCell(clump)) cellsConverged = clump->cells.size();

        int i = 0;
        while (cellsConverged < clump->cells.size()) {
            for (unsigned int cellIdxI = 0; cellIdxI < clump->cells.size(); cellIdxI++) {
                Cell *cellI = &clump->cells[cellIdxI];

                if (cellI->phiConverged) {
                    continue;
                }

                // Update phi per DRLSE
                drlse::updatePhi(cellI, clump, dt, epsilon, mu, kappa, chi);

                cout << "LSF Iteration " << i << ": Clump " << clumpIdx << ", Cell " << cellIdxI << endl;

                // Check every 50 iterations if the cell has converged
                if (i != 0 && i % 50 == 0) {
                    // Clump has converged or iterations have exceeded 1000
                    if (isConverged(cellI) || i >= 1000) {
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
    }


    /*
     * saveFinalCellBoundaries saves each cell's final boundaries as contours to a JSON file
     */
    void saveFinalCellBoundaries(json &finalCellBoundaries, json &nucleiCytoRatios, Image *image, Clump *clump, int clumpIdx) {
        for (int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &(clump->cells[cellIdx]);
            json finalCellBoundary;
            for (cv::Point &point : cell->finalContour) {
                finalCellBoundary.push_back({point.x, point.y});
            }
            finalCellBoundaries[clumpIdx][cellIdx] = finalCellBoundary;
            nucleiCytoRatios[clumpIdx][cellIdx] = cell->nucleusArea / cell->phiArea;
        }

        //Write to JSON file every 100 clumps or when the clump is big
        if (clumpIdx % 100 == 0 || clump->cells.size() > 3000) {
            image->writeJSON("finalCellBoundaries", finalCellBoundaries);
            image->writeJSON("nucleiCytoRatios", nucleiCytoRatios);
        }
    }

    /*
     * loadInitialCellBoundaries loads final cell boundaries from a JSON file into memory
     */
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
                cell->phiArea = cv::contourArea(contour);
                // Clump cyto boundaries are fully loaded
                if (cellIdx == jsonNumberCells - 1) {
                    clump->finalCellContoursLoaded = true;
                }
            }

        }
    }
    
    /*
     * runOverlappingSegmentation is the main function that finds the final cell boundaries for the image
     * This function spawns multiple threads for each clump that finds the final cell boundaries.
     */
    void runOverlappingSegmentation(Image *image) {
        vector<Clump> *clumps = &image->clumps;

        json finalCellBoundaries;
        json nucleiCytoRatios;

        // Load final cell boundaries from finalCellBoundaries.json if it exists
        loadFinalCellBoundaries(finalCellBoundaries, image, clumps);

        function<void(Clump *, int)> threadFunction = [&image](Clump *clump, int clumpIdx) {
            // Do not run the level set algorithm if the final contours have been loaded from file
            if (clump->finalCellContoursLoaded) {
                image->log("Loaded clump %u final cell boundaries from file\n", clumpIdx);
                return;
            }

            image->log("Calculating clump %u's edge enforcer and clump prior\n", clumpIdx);
            // Pad the edge enforcer, clump prior and the cells' phi so that the level set algorithm
            // will not distort any cells that happen to be at the boundary of the image
            clump->edgeEnforcer = drlse::calcEdgeEnforcer(padMatrix(clump->extract(), cv::Scalar(255, 255, 255)));
            clump->clumpPrior = padMatrix(clump->calcClumpPrior(), cv::Scalar(255, 255, 255));

            // Run the level set algorithm
            startOverlappingCellSegmentationThread(image, clump, clumpIdx);
        };

        function<void(Clump *, int)> threadDoneFunction = [&finalCellBoundaries, &nucleiCytoRatios, &image](Clump *clump, int clumpIdx) {
            if (!clump->finalCellContoursLoaded) {
                saveFinalCellBoundaries(finalCellBoundaries, nucleiCytoRatios, image, clump, clumpIdx);
            }
        };

        int maxThreads = 16;
        // Spawns threads that run the thread function for each clump
        ClumpsThread(maxThreads, clumps, threadFunction, threadDoneFunction);

        image->writeJSON("finalCellBoundaries", finalCellBoundaries);
    }

}