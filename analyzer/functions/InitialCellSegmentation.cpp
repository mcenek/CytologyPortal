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
        while (!nucleiDistances.empty()) {
            Cell *closestCell = nucleiDistances[0].first;
            bool validAssociation = testLineViability(point, clump, closestCell);
            if (validAssociation) {
                return closestCell;
            } else {
                nucleiDistances.erase(nucleiDistances.begin());
            }
        }
        return nullptr;
    }

    void associateClumpBoundariesWithCell(Clump *clump, int c, bool debug) {
        if (clump->cells.size() == 1) {
            Cell *cell = &clump->cells[0];
            cell->cytoMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
            cv::drawContours(cell->cytoMask, vector<vector<cv::Point>>{clump->offsetContour}, -1, 255, CV_FILLED);
            return;
        }

        for (int row = 0; row < clump->boundingRect.height; row++) {
            for (int col = 0; col < clump->boundingRect.width; col++) {
                cv::Point point = cv::Point(col, row);
                if (cv::pointPolygonTest(clump->offsetContour, point, false) >= 0) {
                    Cell *associatedCell = findAssociatedCell(point, clump);
                    if (associatedCell == nullptr) continue;

                    if (associatedCell->cytoMask.empty()) {
                        associatedCell->cytoMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
                    }
                    associatedCell->cytoMask.at<unsigned char>(row, col) = 255;
                }
            }
        }


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

    void generateBoundaryFromMask(Cell *cell) {
        vector<vector<cv::Point>> contours;
        cv::findContours(cell->cytoMask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
        if (!contours.empty()) {
            vector <cv::Point> maxContour;
            int maxArea = 0;
            for (vector <cv::Point> contour : contours) {
                int area = cv::contourArea(contour);
                if (area > maxArea) {
                    maxArea = area;
                    maxContour = contour;
                }
            }
            cell->cytoBoundary = maxContour;
        }
    }

    void generateMaskFromBoundary(Cell *cell) {
        cell->cytoMask = cv::Mat::zeros(cell->clump->boundingRect.height, cell->clump->boundingRect.width, CV_8U);
        cv::drawContours(cell->cytoMask, vector<vector<cv::Point>>{cell->cytoBoundary}, 0, 255, CV_FILLED);
    }

    void getContoursFromMask(Clump *clump) {
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            // Finds the biggest contour for the cell, if it finds multiple
            generateBoundaryFromMask(cell);

            // Update the mask so that only the biggest contour is there
            generateMaskFromBoundary(cell);
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

    void calculateGeometricCenters(Clump *clump) {
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            cv::Moments m = cv::moments(cell->cytoMask, true);
            cv::Point p(m.m10 / m.m00, m.m01 / m.m00);
            cell->geometricCenter = p;
        }
    }

    void startInitialCellSegmentationThread(Image *image, Clump *clump, int clumpIdx, bool debug) {
        image->log("Beginning initial cell segmentation for clump %d\n", clumpIdx);

        associateClumpBoundariesWithCell(clump, clumpIdx, debug);
        getContoursFromMask(clump);
        calculateGeometricCenters(clump);

        //Calculate extremal contour extensions for each cell
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];


            //TODO: If we keep this approach shared edges could be memoized
            for (unsigned int compIdx = 0; compIdx < clump->cells.size(); compIdx++) {
                if (cellIdx == compIdx) continue;

                Cell *comparatorCell = &clump->cells[compIdx];


                cv::Mat sharedEdge;

                cv::Mat cytoBoundaryMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);
                cv::Mat comparatorCytoBoundaryMask = cv::Mat::zeros(clump->boundingRect.height, clump->boundingRect.width, CV_8U);

                vector<cv::Point> cytoContour = clump->cells[cellIdx].cytoBoundary;
                cv::drawContours(cytoBoundaryMask, vector<vector<cv::Point>>{cytoContour}, 0, 255, 2);

                vector<cv::Point> comparatorCytoContour = clump->cells[compIdx].cytoBoundary;
                cv::drawContours(comparatorCytoBoundaryMask, vector<vector<cv::Point>>{comparatorCytoContour}, 0, 255, 2);

                cv::bitwise_and(cytoBoundaryMask, comparatorCytoBoundaryMask, sharedEdge);


                if (cv::countNonZero(sharedEdge) > 0) {
                    vector<vector<cv::Point>> sharedEdgeVector;
                    cv::findContours(sharedEdge, sharedEdgeVector, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

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
                        cv::Mat overlappingContour = cv::Mat::zeros(cell->cytoMask.rows,cell->cytoMask.cols, CV_8U);
                        float angle = angleBetween(startPoint, endPoint);
                        cv::Point midpoint = getMidpoint(startPoint, endPoint);
                        double width = cv::norm(startPoint-endPoint);
                        double height = width / 2;
                        cv::RotatedRect rotatedRect = cv::RotatedRect(midpoint, cv::Size2f(width,height), angle);
                        cv::ellipse(overlappingContour, rotatedRect, cv::Scalar(255), -1);
                        cv::bitwise_and(overlappingContour, comparatorCell->cytoMask, overlappingContour);
                        cell->cytoMask += overlappingContour;
                    }
                }
            }
            // Update the cytoBoundary with the changes made to the cytoMask
            generateBoundaryFromMask(cell);
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

        if (clumpIdx % 100 == 0) {
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
                vector<cv::Point> contour;
                for (json &jsonPoint : jsonCellContour) {
                    int x = jsonPoint[0];
                    int y = jsonPoint[1];
                    cv::Point point = cv::Point(x, y);
                    contour.push_back(point);
                }
                Cell *cell = &clump->cells[cellIdx];
                cell->cytoBoundary = contour;
                generateMaskFromBoundary(cell);
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
                vector<cv::Point> contour = clump->undoBoundingRect(cell->cytoBoundary);
                cv::Scalar color = cv::Scalar(rng.uniform(0,255), rng.uniform(0, 255), rng.uniform(0, 255));
                cv::drawContours(outimg, vector<vector<cv::Point>>{contour}, 0, color, 3);
            }
            clump->calcClumpPrior();
        };

        function<void(Clump *, int)> threadDoneFunction = [&initialCellBoundaries, &image](Clump *clump, int clumpIdx) {
            if (!clump->initCytoBoundariesLoaded) {
                saveInitialCellBoundaries(initialCellBoundaries, image, clump, clumpIdx);
            }
        };

        int maxThreads = 8;
        ClumpsThread(maxThreads, clumps, threadFunction, threadDoneFunction);

        image->writeJSON("initialCellBoundaries", initialCellBoundaries);

        return outimg;
    }
}