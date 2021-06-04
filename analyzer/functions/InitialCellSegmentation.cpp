#include "InitialCellSegmentation.h"
#include "SegmenterTools.h"
#include "../objects/ClumpsThread.h"
#include <thread>
#include <future>

namespace segment {
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

    void sortNucleiDistances(vector<pair<Cell*, double>> *nucleiDistances) {
        // order the nuclei indices by increasing distance
        sort(nucleiDistances->begin(), nucleiDistances->end(),
             [=](pair<Cell*, double> &a, pair<Cell*, double> &b) {
                 return a.second < b.second;
             }
        );
    }

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


    void associateClumpBoundariesWithCell(Clump *clump, int c, bool debug) {
        if (clump->cells.size() == 1) {
            Cell *cell = &clump->cells[0];
            cell->cytoMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
            cv::drawContours(cell->cytoMask, vector<vector<cv::Point>>{clump->offsetContour}, -1, 255, CV_FILLED);
            cell->generateBoundaryFromMask();
            cell->originalCytoBoundary = cell->cytoBoundary;
            cell->cytoMask.release();
            return;
        }

        vector<vector<Cell *>> associatedCells(clump->boundingRect.height, vector<Cell *>(clump->boundingRect.width));
        clump->associatedCells = associatedCells;
        associatedCells.clear();
        associatedCells.shrink_to_fit();

        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            cv::Point nucleusCenter = cell->nucleusCenter;
            pair<Cell *, float> nearestCellDistance = findClosestCell(nucleusCenter, clump);
            Cell *nearestCell = nearestCellDistance.first;
            float distance = nearestCellDistance.second;
            if (nearestCell == nullptr) continue;

            float radius = distance / 2.0;

            for (int j = -radius; j < radius; j++) {
                float i1 = -sqrt(pow(radius, 2) - pow(j, 2));
                float i2 = -i1;
                for (int i = i1; i < i2; i++) {
                    float x = i + nucleusCenter.x;
                    float y = j + nucleusCenter.y;
                    cv::Point point = cv::Point(x, y);
                    bool insideClump = cv::pointPolygonTest(clump->offsetContour, point, false) >= 0;
                    bool validAssociation = testLineViability(point, clump, cell);
                    if (insideClump && validAssociation) {
                        cell->cytoAssocs.push_back(point);
                        clump->associatedCells[y][x] = cell;
                    }
                }
            }
        }


        for (int row = 0; row < clump->boundingRect.height; row++) {
            for (int col = 0; col < clump->boundingRect.width; col++) {
                cv::Point point = cv::Point(col, row);
                if (clump->associatedCells[row][col] != nullptr) continue;

                // Check if inside clump
                if (cv::pointPolygonTest(clump->offsetContour, point, false) >= 0) {
                    Cell *associatedCell = findAssociatedCell(point, clump);
                    if (associatedCell == nullptr) continue;
                    associatedCell->cytoAssocs.push_back(point);
                    clump->associatedCells[row][col] = associatedCell;
                }
            }
        }
    }


    void associationsToBoundaries(Clump *clump) {
        if (clump->cells.size() > 1) {
            for (int i = 0; i < clump->cells.size(); i++) {
                Cell *cell = &clump->cells[i];
                cell->cytoMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
                for (const cv::Point &association : cell->cytoAssocs) {
                    cell->cytoMask.at<unsigned char>(association.y, association.x) = 255;
                }
                cell->cytoAssocs.clear();
                cell->cytoAssocs.shrink_to_fit();

                cell->generateBoundaryFromMask();
                cell->originalCytoBoundary = cell->cytoBoundary;

                cell->cytoMask.release();
            }
        }
    }

    void findNeighbors(Clump *clump) {
        if (clump->cells.size() == 1) return;
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            for (const cv::Point point : cell->cytoBoundary) {
                int row = point.y;
                int col = point.x;
                if (row > 0) {
                    Cell *cellAbove = clump->associatedCells[row - 1][col];
                    if (cellAbove != nullptr && cell != cellAbove) {
                        if (find(cell->neighbors.begin(), cell->neighbors.end(), cellAbove) == cell->neighbors.end()) {
                            cell->neighbors.push_back(cellAbove);
                            cellAbove->neighbors.push_back(cell);
                        }

                    }
                }
                if (col > 0) {
                    Cell *cellLeft = clump->associatedCells[row][col - 1];
                    if (cellLeft != nullptr && cell != cellLeft) {
                        if (find(cell->neighbors.begin(), cell->neighbors.end(), cellLeft) == cell->neighbors.end()) {
                            cell->neighbors.push_back(cellLeft);
                            cellLeft->neighbors.push_back(cell);
                        }

                    }
                }
                if (row > 0 && col > 0) {
                    Cell *cellAbove = clump->associatedCells[row - 1][col];
                    Cell *cellLeft = clump->associatedCells[row][col - 1];
                    Cell *cellTopLeft = clump->associatedCells[row - 1][col - 1];
                    if (cellTopLeft == cellAbove || cellTopLeft == cellLeft) continue;
                    if (cellTopLeft != nullptr && cell != cellTopLeft) {
                        if (find(cell->neighbors.begin(), cell->neighbors.end(), cellTopLeft) == cell->neighbors.end()) {
                            cell->neighbors.push_back(cellTopLeft);
                            cellTopLeft->neighbors.push_back(cell);
                        }

                    }
                }
            }
        }

        clump->associatedCells.clear();
        clump->associatedCells.shrink_to_fit();

    }


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

    void generateOtherCellBoundaries(Clump *clump) {
        for (int i = 0; i < clump->cells.size(); i++) {
            Cell *cell = &clump->cells[i];
            if (cell->cytoMask.empty()) {
                float distanceToNearestNucleus = findDistanceToNearestNucleus(clump, cell);

                cv::Mat clumpMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);

                cv::drawContours(clumpMask, vector<vector<cv::Point>>{clump->offsetContour}, -1, 255, CV_FILLED);

                cv::Mat contourMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
                cv::circle(contourMask, cell->nucleusCenter, distanceToNearestNucleus, 255, CV_FILLED);

                cv::bitwise_and(clumpMask, contourMask, contourMask);

                cell->cytoMask = contourMask;
            }
        }
    }

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

    float getDistance(cv::Point a, cv::Point b) {
        cv::Point d = a - b;
        return cv::sqrt(d.x*d.x + d.y*d.y);
    }

    vector<float> getLineDistances(cv::Point start, cv::Point stop) {
        float xDist = stop.x - start.x;
        float yDist = stop.y - start.y;
        float totalLen = sqrt(pow(xDist, 2) + pow(yDist, 2));
        return {xDist, yDist, totalLen};
    }

    cv::Point getMidpoint(const cv::Point& a, const cv::Point& b) {
        cv::Point ret;
        ret.x = (a.x + b.x) / 2;
        ret.y = (a.y + b.y) / 2;
        return ret;
    }

    bool boundingBoxIntersects(cv::Rect A, cv::Rect B) {
        bool xIntersects = abs((A.x + A.width / 2) - (B.x + B.width / 2)) <= (A.width + B.width) / 2;
        bool yIntersects = abs((A.y + A.height / 2) - (B.y + B.height / 2)) <= (A.height + B.height) / 2;
        return xIntersects && yIntersects;
    }

    void startInitialCellSegmentationThread(Image *image, Clump *clump, int clumpIdx, bool debug) {
        image->log("Beginning initial cell segmentation for clump %d, width: %d, height: %d\n", clumpIdx, clump->boundingRect.width, clump->boundingRect.height);


        associateClumpBoundariesWithCell(clump, clumpIdx, debug);
        image->log("Clump %d, done associate clump boundaries with cells\n", clumpIdx);
        associationsToBoundaries(clump);
        findNeighbors(clump);

        //Calculate extremal contour extensions for each cell
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];

            for (unsigned int compIdx = 0; compIdx < cell->neighbors.size(); compIdx++) {
                Cell *comparatorCell = cell->neighbors[compIdx];
                if (cell == comparatorCell) continue;

                image->log("Clump %d, Cell %d/%zu, Neighbors: %d/%zu\n", clumpIdx, cellIdx, clump->cells.size() - 1, compIdx, cell->neighbors.size() - 1);

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

                if (cv::countNonZero(sharedEdge) > 0) {
                    vector<vector<cv::Point>> sharedEdgeVector;
                    cv::findContours(sharedEdge, sharedEdgeVector, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
                    sharedEdge.release();

                    //Sort line as contours are not implicitly ordered
                    struct contoursCmpY {
                        bool operator()(const cv::Point &a, const cv::Point &b) const {
                            if (a.y == b.y)
                                return a.x < b.x;
                            return a.y < b.y;
                        }
                    } contoursCmpY_;

                    std::sort(sharedEdgeVector[0].begin(), sharedEdgeVector[0].end(), contoursCmpY_);

                    //Iterate over the edge to find a viable point to extrapolate contour
                    //TODO: I anticipate some issues with oddly concave shapes.. We're either going to need to be very clever with this approach or refactor
                    int start = -1, end = -1;
                    cv::Point startPoint;
                    cv::Point endPoint;
                    unsigned int endRef = sharedEdgeVector[0].size();
                    if (sharedEdgeVector[0].size() >= 2) {
                        endRef -= 2;
                    }

                    for (unsigned int startRef = 0; startRef < endRef; startRef++, endRef--) {
                        cv::Point startPt = sharedEdgeVector[0][startRef];
                        cv::Point endPt = sharedEdgeVector[0][endRef];

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
                        cv::Mat overlappingContour = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
                        float angle = angleBetween(startPoint, endPoint);
                        cv::Point midpoint = getMidpoint(startPoint, endPoint);
                        double width = cv::norm(startPoint-endPoint);
                        double height = width / 2;
                        cv::RotatedRect rotatedRect = cv::RotatedRect(midpoint, cv::Size2f(width,height), angle);
                        cv::ellipse(overlappingContour, rotatedRect, cv::Scalar(255), -1);
                        //cyto mask is using up all the memory, recalculate cyto mask from boundary instead each time
                        comparatorCell->generateMaskFromBoundary();

                        cv::bitwise_and(overlappingContour, comparatorCell->cytoMask, overlappingContour);
                        comparatorCell->cytoMask.release();

                        cell->generateMaskFromBoundary();
                        cell->cytoMask += overlappingContour;
                        overlappingContour.release();
                        // Update the cytoBoundary with the changes made to the cytoMask
                        cell->generateBoundaryFromMask();
                        cell->cytoMask.release();
                        image->log("Clump %d, cell %d is a neighbor with cell %d, added ellipse\n", clumpIdx, cellIdx, compIdx);
                    }
                }

            }
        }
        image->log("Finishing initial cell segmentation for clump %d\n", clumpIdx);
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

        loadInitialCellBoundaries(initialCellBoundaries, image, clumps);

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

        function<void(Clump *, int)> threadDoneFunction = [&initialCellBoundaries, &image](Clump *clump, int clumpIdx) {
            if (!clump->initCytoBoundariesLoaded) {
                saveInitialCellBoundaries(initialCellBoundaries, image, clump, clumpIdx);
            }
        };

        int maxThreads = 16;
        ClumpsThread(maxThreads, clumps, threadFunction, threadDoneFunction);

        image->writeJSON("initialCellBoundaries", initialCellBoundaries);

        return outimg;
    }
}