#include "InitialCellSegmentation.h"
#include "SegmenterTools.h"
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

    void getContoursFromMask(Clump *clump) {
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            vector<vector<cv::Point>> contours;

            cv::findContours(cell->cytoMask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
            if (!contours.empty()) {
                vector<cv::Point> maxContour;
                int maxArea = 0;
                for (vector<cv::Point> contour : contours) {
                    int area = cv::contourArea(contour);
                    if (area > maxArea) {
                        maxArea = area;
                        maxContour = contour;
                    }
                }
                cell->cytoBoundary = maxContour;

                cell->cytoMask = cv::Mat::zeros(cell->cytoMask.rows, cell->cytoMask.cols, CV_8U);

                cv::drawContours(cell->cytoMask, vector<vector<cv::Point>>{maxContour}, 0, 255, CV_FILLED);

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

    void calculateGeometricCenters(Clump *clump) {
        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            cv::Moments m = cv::moments(cell->cytoMask, true);
            cv::Point p(m.m10 / m.m00, m.m01 / m.m00);
            cell->geometricCenter = p;
        }
    }

    void startInitialCellSegmentationThread(Image *image, Clump *clump, int clumpIdx, cv::RNG *rng, cv::Mat outimg, bool debug) {

        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            vector<cv::Point> contour;
            string fileStem = "clump" + to_string(clumpIdx) + "cell" + to_string(cellIdx);
            boost::filesystem::path loadPath = image->getWritePath(fileStem, ".txt");
            std::ifstream inFile(loadPath.string());
            if (!inFile.fail()) {
                int x, y;
                while (inFile >> x >> y) {
                    contour.push_back(cv::Point(x, y));

                }
                clump->finalCellContours.push_back(contour);
            }
        }
        if (clump->finalCellContours.size() != clump->cells.size()) {
            clump->finalCellContours.clear();
        } else {
            image->log("Loaded final contours from clump %d\n", clumpIdx);
            return;
        }

        image->log("Beginning initial cell segmentation for clump %d\n", clumpIdx);

        associateClumpBoundariesWithCell(clump, clumpIdx, debug);
        //generateOtherCellBoundaries(clump);

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

        }

        for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
            Cell *cell = &clump->cells[cellIdx];
            vector<vector<cv::Point>> contours;
            cv::findContours(cell->cytoMask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
            if (!contours.empty()) {
                contours[0] = clump->undoBoundingRect(contours[0]);
                cv::Scalar color = cv::Scalar(rng->uniform(0,255), rng->uniform(0, 255), rng->uniform(0, 255));
                cv::drawContours(outimg, vector<vector<cv::Point>>{contours[0]}, 0, color, 3);
            }
        }

        clump->calcClumpPrior();

    }


    cv::Mat runInitialCellSegmentation(Image *image, int threshold1, int threshold2, bool debug) {
        vector<Clump> *clumps = &image->clumps;
        // run a find contour on the nucleiBoundaries to get them as contours, not regions
        for (unsigned int c = 0; c < clumps->size(); c++) {
            Clump *clump = &(*clumps)[c];
            clump->createCells();
            // sanity check: view all the cell array sizes for each clump
            //image->log("Clump: %u, # of nuclei boundaries: %lu, # of cells: %lu\n",
            //       c, clump->nucleiBoundaries.size(), clump->cells.size());
        }


        cv::Mat outimg = image->mat.clone();
        cv::RNG rng(12345);


        vector<shared_future<void>> allThreads;
        vector<Clump>::iterator clumpIterator = clumps->begin();
        int numThreads = 8;
        do {
            // Fill thread queue
            while (allThreads.size() < numThreads && clumpIterator < clumps->end()) {
                Clump *clump = &(*clumpIterator);
                int clumpIdx = clumpIterator - clumps->begin();
                clumpIterator++;
                shared_future<void> thread_object = async(&startInitialCellSegmentationThread, image, clump, clumpIdx, &rng, outimg, debug);
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

        return outimg;
    }
}