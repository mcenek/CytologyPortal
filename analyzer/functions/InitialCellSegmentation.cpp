#include "InitialCellSegmentation.h"
#include "SegmenterTools.h"
#include "../objects/ClumpsThread.h"
#include <thread>
#include <future>

namespace segment {
    /*
     * findNucleiDistances finds the distances from a point to each cell in the clump
     * returns a vector with pairs of cells and their respective distances
     */
    vector<pair<Cell*, double>> findNucleiDistances(cv::Point point, Clump *clump) {
        // find the distance to each nucleus
        vector<pair<Cell*, double>> nucleiDistances;
        for (unsigned int i = 0; i < clump->cells.size(); i++) {
            Cell *cell = &clump->cells[i];
            double distance = getDistance(point, cell->nucleusCenter);
            nucleiDistances.push_back(pair<Cell*, double>(cell, distance));
        }
        return nucleiDistances;
    }

    /*
     * findClosestCell returns a pair of the closet cell and its respective distance
     * to the point specified
     */
    pair<Cell*, float> findClosestCell(cv::Point point, Clump *clump) {
        Cell *closestCell = nullptr;
        float minDistance = FLT_MAX;
        for (unsigned int i = 0; i < clump->cells.size(); i++) {
            Cell *cell = &clump->cells[i];
            float distance = getDistance(point, cell->nucleusCenter);
            if (distance < minDistance && distance != 0) {
                closestCell = cell;
                minDistance = distance;
            }
        }
        pair<Cell*, float> closestCellDistance(closestCell, minDistance);
        return closestCellDistance;
    }

    /*
     * sortNucleiDistances sorts a vector of pairs of Cells and their
     * respective distances by distances
     */
    void sortNucleiDistances(vector<pair<Cell*, double>> *nucleiDistances) {
        // order the nuclei indices by increasing distance
        sort(nucleiDistances->begin(), nucleiDistances->end(),
             [=](pair<Cell*, double> &a, pair<Cell*, double> &b) {
                 return a.second < b.second;
             }
        );
    }

    /*
     * findAssociatedCell finds the associated cell of a pixel in the clump
     * This is done by finding the closest cell to the pixel and ensuring that
     * a straight line can be drawn from the pixel to the cell without intersecting
     * clump boundaries
     */
    Cell* findAssociatedCell(cv::Point point, Clump *clump) {
        vector<pair<Cell*, double>> nucleiDistances = findNucleiDistances(point, clump);
        sortNucleiDistances(&nucleiDistances);
        for (const pair<Cell*, double> &nucleiDistance : nucleiDistances) {
            Cell *closestCell = nucleiDistance.first;
            bool validAssociation = testLineViability(point, clump, closestCell);
            if (validAssociation) {
                return closestCell;
            }
        }
        return nullptr;
    }

    /*
     * insideClump returns true when a pixel is inside the clump and false otherwise
     * The function uses the actual image, the clump contour, and gmm predictions to determine this.
     */
    bool insideClump(Image *image, Clump *clump, cv::Point point) {
        cv::Point pointImage(point.x + clump->boundingRect.x, point.y + clump->boundingRect.y);
        bool insideImage = pointImage.x >= 0 && pointImage.x < image->gmmPredictions.cols &&
                pointImage.y >= 0 && pointImage.y < image->gmmPredictions.rows;
        return insideImage && image->gmmPredictions.at<uchar>(pointImage) > 0 &&
            cv::pointPolygonTest(clump->offsetContour, point, false) >= 0;
    }

    /*
     * associateClumpBoundariesWithCell creates a map of pixels and their associated cell
     * Optimizations include:
     *     - associating the entire clump with a cell if there is only one cell in the clump
     *     - associating pixels near a cell's nuclei with the cell
     */
    void associateClumpBoundariesWithCell(Image *image, Clump *clump, int c, bool debug) {
        //Associate the entire clump with a cell if there is only one cell in the clump
        if (clump->cells.size() == 1) {
            Cell *cell = &clump->cells[0];
            cell->cytoMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
            cv::drawContours(cell->cytoMask, vector<vector<cv::Point>>{clump->offsetContour}, -1, 255, CV_FILLED);
            cell->generateBoundaryFromMask();
            cell->originalCytoBoundary = cell->cytoBoundary;
            cell->cytoMask.release();
            return;
        }

        //Create map
        vector<vector<Cell *>> associatedCells(clump->boundingRect.height, vector<Cell *>(clump->boundingRect.width));

        //Move vector to a Clump instance variable
        clump->associatedCells = associatedCells;
        associatedCells.clear();
        associatedCells.shrink_to_fit();

        //Associating pixels near a cell's nuclei with the cell
        //Near is defined as any pixel within a circle of radius less than half the distance
        //to the nearest nucleus
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            cv::Point nucleusCenter = cell->nucleusCenter;

            //Find the nearest nucleus and its distance
            pair<Cell *, float> nearestCellDistance = findClosestCell(nucleusCenter, clump);
            Cell *nearestCell = nearestCellDistance.first;
            float distance = nearestCellDistance.second;
            if (nearestCell == nullptr) continue;

            //Radius of circle will be half that distance
            float radius = distance / 2.0;

            //Iterate over pixels in that circle
            for (int j = -radius; j < radius; j++) {
                float i1 = -sqrt(pow(radius, 2) - pow(j, 2));
                float i2 = -i1;
                for (int i = i1; i < i2; i++) {
                    float x = i + nucleusCenter.x;
                    float y = j + nucleusCenter.y;
                    cv::Point point = cv::Point(x, y);
                    //Ensure that these pixels also are inside the clump and that we can draw
                    //a line from the pixel to the candidate associated cell's nucleus without
                    //intersecting the clump boundary
                    if (insideClump(image, clump, point) && testLineViability(point, clump, cell)) {
                        cell->cytoAssocs.push_back(point);
                        clump->associatedCells[y][x] = cell;
                    }
                }
            }
        }

        //Finding the other pixels' nuclei associations
        for (int row = 0; row < clump->boundingRect.height; row++) {
            for (int col = 0; col < clump->boundingRect.width; col++) {
                cv::Point point = cv::Point(col, row);
                if (clump->associatedCells[row][col] != nullptr) continue;

                if (insideClump(image, clump, point)) {
                    Cell *associatedCell = findAssociatedCell(point, clump);
                    if (associatedCell == nullptr) continue;
                    associatedCell->cytoAssocs.push_back(point);
                    clump->associatedCells[row][col] = associatedCell;
                }
            }
        }
    }


    /*
     * associationsToBoundaries converts pixel associations in vector form to contours
     * We plot these associations on a mask and then run a contour finding algorithm on the mask
     * to get the boundaries
     */
    void associationsToBoundaries(Clump *clump) {
        if (clump->cells.size() > 1) {
            for (int i = 0; i < clump->cells.size(); i++) {
                Cell *cell = &clump->cells[i];

                //Create the mask from the vector of associations
                cell->cytoMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
                for (const cv::Point &association : cell->cytoAssocs) {
                    cell->cytoMask.at<unsigned char>(association.y, association.x) = 255;
                }

                //Release the vector from memory, no longer needed
                cell->cytoAssocs.clear();
                cell->cytoAssocs.shrink_to_fit();

                //Convert the mask to contour
                cell->generateBoundaryFromMask();

                //Save the original boundary for overlapping cell interpolation
                cell->originalCytoBoundary = cell->cytoBoundary;

                //Release the mask from memory, to conserve memory
                cell->cytoMask.release();
            }
        }
    }

    /*
     * findNeighbors finds cells that are neighbors with each cell in the specified clump
     * We iterate over each cell's contour and look at pixels above, to the left and to the top left
     * and check if those pixels are associated with another cell.
     * If so we add it to the cell's list of neighbors
     */
    void findNeighbors(Clump *clump) {
        //A clump with one cell has no neighbors
        if (clump->cells.size() == 1) return;
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            //Iterate over cell contour
            for (const cv::Point &point : cell->cytoBoundary) {
                int row = point.y;
                int col = point.x;
                //Ensure row is a positive value
                if (row > 0) {
                    Cell *cellAbove = clump->associatedCells[row - 1][col];
                    //Ensure that the pixel is associated with a cell and its not the original cell
                    if (cellAbove != nullptr && cell != cellAbove) {
                        if (find(cell->neighbors.begin(), cell->neighbors.end(), cellAbove) == cell->neighbors.end()) {
                            cell->neighbors.push_back(cellAbove);
                            cellAbove->neighbors.push_back(cell);
                        }

                    }
                }
                //Ensure col is a positive value
                if (col > 0) {
                    Cell *cellLeft = clump->associatedCells[row][col - 1];
                    //Ensure that the pixel is associated with a cell and its not the original cell
                    if (cellLeft != nullptr && cell != cellLeft) {
                        if (find(cell->neighbors.begin(), cell->neighbors.end(), cellLeft) == cell->neighbors.end()) {
                            cell->neighbors.push_back(cellLeft);
                            cellLeft->neighbors.push_back(cell);
                        }

                    }
                }
                //Ensure row and col are positive values
                if (row > 0 && col > 0) {
                    Cell *cellAbove = clump->associatedCells[row - 1][col];
                    Cell *cellLeft = clump->associatedCells[row][col - 1];
                    Cell *cellTopLeft = clump->associatedCells[row - 1][col - 1];
                    //Check that cellTopLeft is different than cellAbove and cellLeft since we would
                    //have checked those already
                    if (cellTopLeft == cellAbove || cellTopLeft == cellLeft) continue;
                    //Ensure that the pixel is associated with a cell and its not the original cell
                    if (cellTopLeft != nullptr && cell != cellTopLeft) {
                        if (find(cell->neighbors.begin(), cell->neighbors.end(), cellTopLeft) == cell->neighbors.end()) {
                            cell->neighbors.push_back(cellTopLeft);
                            cellTopLeft->neighbors.push_back(cell);
                        }

                    }
                }
            }
        }
        //Remove the association map from memory, no longer needed
        clump->associatedCells.clear();
        clump->associatedCells.shrink_to_fit();
    }

    /*
     * findDistanceToNearestNucleus returns the distance to the closest nucleus
     */
    float findDistanceToNearestNucleus(Clump *clump, Cell *cell) {
        float minDistance = FLT_MAX;
        for (int i = 0; i < clump->cells.size(); i++) {
            Cell *comparatorCell = &clump->cells[i];
            if (cell == comparatorCell) continue;
            float distance = getDistance(cell->nucleusCenter, comparatorCell->nucleusCenter);
            minDistance = min(minDistance, distance);
        }
        return minDistance;
    }

    /*
     * testLineViability ensures that we can draw a straight line from the pixel to cell's nucleus
     * without intersecting clump boundaries.
     */
    bool testLineViability(cv::Point pixel, Clump *clump, Cell *cell) {
        for (float f = 0.1; f < 1.0; f += 0.1) {
            float x = pixel.x * f + cell->nucleusCenter.x * (1 - f);
            float y = pixel.y * f + cell->nucleusCenter.y * (1 - f);

            // if the point is outside the clump, we haven't found our match
            if (cv::pointPolygonTest(clump->offsetContour, cv::Point(x, y), false) < 0) {
                return false;
            }
        }
        return true;
    }

    /*
     * getDistance returns the distance between two points
     */
    float getDistance(cv::Point &a, cv::Point &b) {
        cv::Point d = a - b;
        return cv::sqrt(d.x*d.x + d.y*d.y);
    }

    /*
     * getMidpoint returns the midpoint between two points
     */
    cv::Point getMidpoint(const cv::Point &a, const cv::Point &b) {
        cv::Point ret;
        ret.x = (a.x + b.x) / 2;
        ret.y = (a.y + b.y) / 2;
        return ret;
    }


    /*
     * getSharedEdge returns a binary AND mask of a cell and a comparator cell's mask
     * Since boundary information is stored as contours in memory, contours are first converted to masks
     */
    cv::Mat getSharedEdge(Clump *clump, Cell *cell, Cell *comparatorCell) {
        vector <cv::Point> cytoContour = cell->originalCytoBoundary;
        vector <cv::Point> comparatorCytoContour = comparatorCell->originalCytoBoundary;

        cv::Mat cytoBoundaryMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
        cv::Mat comparatorCytoBoundaryMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);

        if (cytoContour.size() > 0) {
            cv::drawContours(cytoBoundaryMask, vector<vector<cv::Point>>{cytoContour}, 0, 255, 2);
        }

        if (comparatorCytoContour.size() > 0) {
            cv::drawContours(comparatorCytoBoundaryMask, vector<vector<cv::Point>>{comparatorCytoContour}, 0, 255, 2);
        }

        cv::Mat sharedEdge;
        cv::bitwise_and(cytoBoundaryMask, comparatorCytoBoundaryMask, sharedEdge);
        cytoBoundaryMask.release();
        comparatorCytoBoundaryMask.release();
        return sharedEdge;
    }

    /*
     * getSharedEdgeContour returns a sorted vector of points that are on the boundary of the shared edge
     * Used for finding the points for interpolation
     */
    vector<cv::Point> getSharedEdgeContour(cv::Mat sharedEdge) {
        vector<cv::Point> sharedEdgeContour;
        if (cv::countNonZero(sharedEdge) > 0) {
            //Find contour of shared edge
            vector<vector<cv::Point>> sharedEdgeContours;
            cv::findContours(sharedEdge, sharedEdgeContours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
            sharedEdge.release();

            //Sort line as contours are not implicitly ordered
            struct contoursCmpY {
                bool operator()(const cv::Point &a, const cv::Point &b) const {
                    if (a.y == b.y)
                        return a.x < b.x;
                    return a.y < b.y;
                }
            } contoursCmpY_;

            std::sort(sharedEdgeContours[0].begin(), sharedEdgeContours[0].end(), contoursCmpY_);
            sharedEdgeContour = sharedEdgeContours[0];
        }
        return sharedEdgeContour;

    }

    /*
     * findInterpolationPoints finds the start and end point for interpolation of overlapping area
     * from the shared edge contour.
     * If the start or end points cannot be found, then an empty vector is returned.
     */
    vector<cv::Point> findInterpolationPoints(vector<cv::Point> sharedEdgeContour,
                                              Clump *clump, Cell *comparatorCell) {
        vector<cv::Point> interpolationPoints;

        if (!sharedEdgeContour.empty()) {
            //Iterate over the edge to find a viable point to extrapolate contour
            int start = -1, end = -1;
            cv::Point startPoint;
            cv::Point endPoint;

            unsigned int endRef = sharedEdgeContour.size();
            if (sharedEdgeContour.size() >= 2) {
                endRef -= 2;
            }

            for (unsigned int startRef = 0; startRef < endRef; startRef++, endRef--) {
                cv::Point startPt = sharedEdgeContour[startRef];
                cv::Point endPt = sharedEdgeContour[endRef];

                if (start == -1 &&
                    testLineViability(startPt, clump, comparatorCell)) {
                    start = startRef;
                    startPoint = startPt;
                }

                if (end == -1 && testLineViability(endPt, clump, comparatorCell)) {
                    end = endRef;
                    endPoint = endPt;
                }
            }
            if (start != -1 && end != -1) {
                interpolationPoints.push_back(startPoint);
                interpolationPoints.push_back(endPoint);
            }
        }

        return interpolationPoints;
    }

    /*
     * interpolateOverlappingArea extends the initial cell association boundaries to include extra
     * overlapping area with each of the cell's neighbors.
     * This is necessary because in the overlapping cell segmentation, this boundary shrinks to fit
     * the actual cell boundaries.
     */
    void interpolateOverlappingArea(Clump *clump, Cell *cell, Cell *comparatorCell) {
        //Find start and end points for interpolation.
        //These points signify the 'border' between a cell and its neighbor if we were to draw a line between them
        cv::Mat sharedEdge = getSharedEdge(clump, cell, comparatorCell);
        vector<cv::Point> sharedEdgeContour = getSharedEdgeContour(sharedEdge);
        vector<cv::Point> interpolationPoints = findInterpolationPoints(sharedEdgeContour, clump, comparatorCell);

        if (interpolationPoints.empty()) return;

        //Draw ellipse with major axis distance between interpolated points
        //and minor axis is major axis / 2
        //where the major axis lies along a line between the interpolated points
        cv::Mat overlappingContour = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
        float angle = angleBetween(interpolationPoints[0], interpolationPoints[1]);
        cv::Point midpoint = getMidpoint(interpolationPoints[0], interpolationPoints[1]);
        double width = cv::norm(interpolationPoints[0] - interpolationPoints[1]);
        double height = width / 2;
        cv::RotatedRect rotatedRect = cv::RotatedRect(midpoint, cv::Size2f(width, height), angle);
        cv::ellipse(overlappingContour, rotatedRect, cv::Scalar(255), -1);

        //Boundaries are stored as contours in memory, so we calculate the mask
        comparatorCell->generateMaskFromBoundary();
        cv::bitwise_and(overlappingContour, comparatorCell->cytoMask, overlappingContour);
        //Release the mask since we don't need it anymore
        comparatorCell->cytoMask.release();

        //Add the overlapping contour to the cell's mask
        cell->generateMaskFromBoundary();
        cell->cytoMask += overlappingContour;
        overlappingContour.release();
        // Update the cytoBoundary with the changes made to the cytoMask
        cell->generateBoundaryFromMask();
        cell->cytoMask.release();
    }

    /*
     * startInitialCellSegmentationThread is the main function that finds the initial cell boundaries
     * for each cell in the specified clump
     * 1) We associate each pixel in the clump with a cell
     * 2) Then we convert these associations to contours
     * 3) We find neighboring cells
     * 4) Interpolate overlapping area of neighboring cells.
     *    This is a liberal guess of the overlapping region of cells in which
     *    the overlapping cell segmentation will shrink to fit the actual boundaries.
     */
    void startInitialCellSegmentationThread(Image *image, Clump *clump, int clumpIdx, bool debug) {
        image->log("Beginning initial cell segmentation for clump %d, width: %d, height: %d\n", clumpIdx, clump->boundingRect.width, clump->boundingRect.height);

        associateClumpBoundariesWithCell(image, clump, clumpIdx, debug);
        image->log("Clump %d, done associate clump boundaries with cells\n", clumpIdx);
        associationsToBoundaries(clump);
        findNeighbors(clump);

        //Interpolation of overlapping neighbors
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            for (unsigned int compIdx = 0; compIdx < cell->neighbors.size(); compIdx++) {
                Cell *neighborCell = cell->neighbors[compIdx];
                if (cell == neighborCell) continue;
                image->log("Clump %d, Cell %d/%zu, Neighbors: %d/%zu\n", clumpIdx, cellIdx, clump->cells.size() - 1,
                           compIdx, cell->neighbors.size() - 1);
                interpolateOverlappingArea(clump, cell, neighborCell);
            }
        }
        image->log("Finishing initial cell segmentation for clump %d\n", clumpIdx);
    }

    /*
     * saveCellNeighbors saves each cell's neighbors in the form of cell indexes for all clumps
     * to a JSON file
     */
    void saveCellNeighbors(json &cellNeighbors, Image *image, Clump *clump, int clumpIdx) {
        map<Cell *, int> cellToIdx;

        //Find indexes for each cell
        for (int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &(clump->cells[cellIdx]);
            cellToIdx[cell] = cellIdx;
        }

        //Save neighbor information to JSON object in memory
        for (int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &(clump->cells[cellIdx]);
            json neighbors;
            for (Cell *neighbor : cell->neighbors) {
                int neighborIdx = cellToIdx[neighbor];
                neighbors.push_back(neighborIdx);
            }
            cellNeighbors[clumpIdx][cellIdx] = neighbors;
        }

        //Write to JSON file every 100 clumps or when the clump is big
        if (clumpIdx % 100 == 0 || clump->cells.size() > 5000) {
            image->writeJSON("cellNeighbors", cellNeighbors);
        }
    }

    void loadCellNeighbors(json &cellNeighbors, Image *image, vector<Clump> *clumps) {
        cellNeighbors = image->loadJSON("cellNeighbors");
        for (int clumpIdx = 0; clumpIdx < cellNeighbors.size(); clumpIdx++) {
            Clump *clump = &(*clumps)[clumpIdx];
            json jsonClumpCellNeighbors = cellNeighbors[clumpIdx];
            if (jsonClumpCellNeighbors == nullptr) continue;
            int jsonNumberCells = cellNeighbors[clumpIdx].size();
            for (int cellIdx = 0; cellIdx < jsonNumberCells; cellIdx++) {
                Cell *cell = &clump->cells[cellIdx];
                json jsonCellNeighbors = cellNeighbors[clumpIdx][cellIdx];
                if (jsonCellNeighbors == nullptr) break;
                for (int neighborIdx : jsonCellNeighbors) {
                    Cell *neighbor = &clump->cells[neighborIdx];
                    cell->neighbors.push_back(neighbor);
                }
            }

        }
    }

    void saveInitialCellBoundaries(json &initialCellBoundaries, Image *image, Clump *clump, int clumpIdx) {
        for (int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &(clump->cells[cellIdx]);
            json initialCellBoundary;
            for (cv::Point &point : cell->cytoBoundary) {
                initialCellBoundary.push_back({point.x, point.y});
            }
            initialCellBoundaries[clumpIdx][cellIdx] = initialCellBoundary;
        }

        if (clumpIdx % 100 == 0 || clump->cells.size() > 5000) {
            image->writeJSON("initialCellBoundaries", initialCellBoundaries);
        }
    }

    void loadInitialCellBoundaries(json &initialCellBoundaries, Image *image, vector<Clump> *clumps) {
        initialCellBoundaries = image->loadJSON("initialCellBoundaries");
        for (int clumpIdx = 0; clumpIdx < initialCellBoundaries.size(); clumpIdx++) {
            Clump *clump = &(*clumps)[clumpIdx];
            json jsonClumpContours = initialCellBoundaries[clumpIdx];
            if (jsonClumpContours == nullptr) continue;
            int jsonNumberCells = initialCellBoundaries[clumpIdx].size();
            for (int cellIdx = 0; cellIdx < jsonNumberCells; cellIdx++) {
                json jsonCellContour = initialCellBoundaries[clumpIdx][cellIdx];
                // Will not load incomplete clumps
                if (jsonCellContour == nullptr) break;
                bool validContour = true;
                vector<cv::Point> contour;
                for (json &jsonPoint : jsonCellContour) {
                    if (jsonPoint.size() != 2) {
                        validContour = false;
                        break;
                    }
                    int x = jsonPoint[0];
                    int y = jsonPoint[1];
                    cv::Point point = cv::Point(x, y);
                    contour.push_back(point);
                }
                if (!validContour) {
                    break;
                }
                Cell *cell = &clump->cells[cellIdx];
                cell->cytoBoundary = contour;
                // Clump cyto boundaries are fully loaded
                if (cellIdx == jsonNumberCells - 1) {
                    clump->initCytoBoundariesLoaded = true;
                }
            }

        }
    }

    cv::Mat runInitialCellSegmentation(Image *image, int threshold1, int threshold2, bool debug) {
        vector<Clump> *clumps = &image->clumps;
        // run a find contour on the nucleiBoundaries to get them as contours, not regions
        for (unsigned int c = 0; c < clumps->size(); c++) {
            Clump *clump = &(*clumps)[c];
            clump->createCells();
        }

        cv::Mat outimg = image->mat.clone();
        cv::RNG rng(12345);

        json initialCellBoundaries;
        json cellNeighbors;

        loadInitialCellBoundaries(initialCellBoundaries, image, clumps);
        loadCellNeighbors(cellNeighbors, image, clumps);

        function<void(Clump *, int)> threadFunction = [&image, &debug, &rng, &outimg](Clump *clump, int clumpIdx) {
            if (clump->initCytoBoundariesLoaded) {
                image->log("Loaded clump %u initial cell boundaries from file\n", clumpIdx);
            } else {
                startInitialCellSegmentationThread(image, clump, clumpIdx, debug);
            }
            for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
                Cell *cell = &clump->cells[cellIdx];

                cell->originalCytoBoundary.clear();
                cell->originalCytoBoundary.shrink_to_fit();

                vector<cv::Point> contour = clump->undoBoundingRect(cell->cytoBoundary);
                if (contour.size() == 0) {
                    // Delete cell if there are no more boundaries
                    for (unsigned int neighborIdx = 0; neighborIdx < cell->neighbors.size(); neighborIdx++) {
                        Cell *neighborCell = cell->neighbors[neighborIdx];
                        //Remove cell from its neighbors
                        std::remove(neighborCell->neighbors.begin(), neighborCell->neighbors.end(), cell);
                    }
                    // Remove cell from the cell list
                    clump->cells.erase(clump->cells.begin() + cellIdx);
                    cellIdx--;
                    continue;
                }
                cv::Scalar color = cv::Scalar(rng.uniform(0,255), rng.uniform(0, 255), rng.uniform(0, 255));
                cv::drawContours(outimg, vector<vector<cv::Point>>{contour}, 0, color, 3);
            }
        };

        function<void(Clump *, int)> threadDoneFunction = [&initialCellBoundaries, &cellNeighbors, &image](Clump *clump, int clumpIdx) {
            if (!clump->initCytoBoundariesLoaded) {
                saveInitialCellBoundaries(initialCellBoundaries, image, clump, clumpIdx);
                saveCellNeighbors(cellNeighbors, image, clump, clumpIdx);
            }
        };

        int maxThreads = 16;
        ClumpsThread(maxThreads, clumps, threadFunction, threadDoneFunction);

        image->writeJSON("initialCellBoundaries", initialCellBoundaries);
        image->writeJSON("cellNeighbors", cellNeighbors);

        return outimg;
    }
}