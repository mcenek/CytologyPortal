#include "InitialCellSegmentation.h"
#include "ClumpSegmentation.h"
#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"
#include "LSM.h"
#include "SegmenterTools.h"

namespace segment {
    const int allContours = -1;
    const cv::Scalar pink = cv::Scalar(1.0, 0, 1.0);

    void associateClumpBoundariesWithCell(Clump *clump, int c, bool debug) {
        clump->nucleiAssocs = cv::Mat::zeros(clump->clumpMat.rows, clump->clumpMat.cols, CV_64FC3);

        // typedef cv::Point3_<uint8_t> Pixel;
        // clump->nucleiAssocs.forEach<Pixel>([](Pixel &p, const int * position) -> void {
        //     // p.x = 1.0;
        //     // p.z = 1.0;
        // });

        // printf("Clump %i, # of cells: %lu\n", c, clump->cells.size());

        // debugging information
        int numPixelsOutside = 0, numPixelsAssigned = 0, numPixelsLost = 0;
        int pixelTotal = clump->clumpMat.rows * clump->clumpMat.cols;
        int pixelCount = 0;

        // for each pixel in the clump ...
        for (int row = 0; row < clump->clumpMat.rows; row++) {
            for (int col = 0; col < clump->clumpMat.cols; col++) {
                pixelCount++;
                // clump->nucleiAssocs.at<cv::Vec3d>(row, col) = cv::Vec3d(0, 0, 1.0);
                // if the pixel is outside of the clump, ignore it
                // TODO something is off here, seems like it should be row, col
                cv::Point pixel = cv::Point(col, row);
                if (cv::pointPolygonTest(clump->offsetContour, pixel, false) < 0) {
                    // clump->nucleiAssocs.at<cv::Vec3d>(col, row) = cv::Vec3d(0, 1.0, 0);
                    numPixelsOutside++; //TODO: Not currently used
                } else {
                    // find the distance to each nucleus
                    vector<pair<int, double>> nucleiDistances;
                    for (unsigned int n = 0; n < clump->cells.size(); n++) {
                        double distance = cv::norm(pixel - clump->cells[n].nucleusCenter);
                        nucleiDistances.push_back(pair<int, double>(n, distance));
                        //printf("pixel %i, %i, nucleus %i, distance %f\n", row, col, n, distance);
                    }
                    // order the nuclei indices by increasing distance
                    sort(nucleiDistances.begin(), nucleiDistances.end(),
                         [=](pair<int, double> &a, pair<int, double> &b) {
                             return a.second < b.second;
                         });

                    // then look at the closest and make sure it has a viable line
                    // while we don't have an answer, keep finding the closest and checking it
                    int nIndex = -1;
                    while (nIndex == -1 && !nucleiDistances.empty()) {
                        pair<int, double> closest = nucleiDistances[0];
                        nIndex = closest.first;
                        // printf("Checking pixel %i, %i against nucleus %i, distance %f\n", row, col, nIndex, closest.second);

                        // check points along the line between pixel and closest nuclei
                        //TODO: Use segTools testLineViability
                        for (float f = 0.1; f < 1.0; f += 0.1) {
                            float x = pixel.x * f + clump->cells[nIndex].nucleusCenter.x * (1 - f);
                            float y = pixel.y * f + clump->cells[nIndex].nucleusCenter.y * (1 - f);

                            // if the point is outside the clump, we haven't found our match
                            if (cv::pointPolygonTest(clump->offsetContour, cv::Point(x, y), false) < 0) {
                                nIndex = -1;
                                nucleiDistances.erase(nucleiDistances.begin());
                                break;
                                // printf("Point %f, %f falls outside the clump\n", x, y);
                            }
                        }
                    }
                    if (nIndex > -1) {
                        clump->nucleiAssocs.at<cv::Vec3d>(row, col) = clump->cells[nIndex].color;
                        // clump->nucleiAssocs.data[(row + col*clump->nucleiAssocs.rows)*3 + 0] = clump->cells[nIndex].color[0];
                        // clump->nucleiAssocs.data[(row + col*clump->nucleiAssocs.rows)*3 + 1] = clump->cells[nIndex].color[1];
                        // clump->nucleiAssocs.data[(row + col*clump->nucleiAssocs.rows)*3 + 2] = clump->cells[nIndex].color[2];
                        numPixelsAssigned++;
                    } else {
                        clump->nucleiAssocs.at<cv::Vec3d>(row, col) = cv::Vec3d(1., 0., 0.);
                        // clump->nucleiAssocs.data[(row + col*clump->nucleiAssocs.rows)*3 + 0] = 0;
                        // clump->nucleiAssocs.data[(row + col*clump->nucleiAssocs.rows)*3 + 1] = 0;
                        // clump->nucleiAssocs.data[(row + col*clump->nucleiAssocs.rows)*3 + 2] = 0;
                        numPixelsLost++;
                    }
                }
            }
        }
        if (debug) {
            printf("For clump: %i, # of cells: %lu, %i pixels total, %i pixels assessed, %i pixels inside clump, %i pixels outside, %i pixels lost\n",
                   c, clump->cells.size(), pixelTotal, pixelCount, numPixelsAssigned, numPixelsOutside,
                   numPixelsLost);
        }
    }
    bool testLineViability(cv::Point pixel, Clump *clump, Cell *cell, int idx) {
        for (float f = 0.1; f < 1.0; f += 0.1) {
            float x = pixel.x * f + cell->nucleusCenter.x * (1 - f);
            float y = pixel.y * f + cell->nucleusCenter.y * (1 - f);

            // if the point is outside the clump, we haven't found our match
            if (cv::pointPolygonTest(clump->offsetContour, cv::Point(x, y), false) < 0) {
                return false;
                // printf("Point %f, %f falls outside the clump\n", x, y);
            }
        }
        return true;
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

    //TODO: Fix this to work in all cases
    vector<cv::Point> interpolateLine(cv::Point start, cv::Point middle, cv::Point stop) {
        vector<cv::Point> tmpVec;
        int detail = 5; //Level of detail

        vector<float> distSM = getLineDistances(start, middle);
        vector<float> distMS = getLineDistances(middle, stop);
        vector<float> distSS = getLineDistances(start, stop);

        float percentSM = distSM[2] / (float) (distSM[2] + distMS[2]);
        int breakpoint = round(detail * percentSM);

        vector<float> deltaSM = {distSM[0] / breakpoint, distSM[1] / breakpoint};
        vector<float> deltaMS = {distMS[0] / (detail - breakpoint), distMS[1] / (detail - breakpoint)};
        vector<float> deltaSS = {distSS[0] / detail, distSS[1] / detail};

        float mu = 0.5;
        mu = (1 - cos(mu * M_PI)) / 2;
        for (int i = 0; i < detail; i++) {
            float xPos = start.x + (deltaSS[0] * i);
            float yPos = start.y + (deltaSS[1] * i);
            float xPos2, yPos2;

            if (i <= breakpoint) {
                xPos2 = start.x + (deltaSM[0] * i);
                yPos2 = start.y + (deltaSM[1] * i);
            } else {
                xPos2 = middle.x + (deltaMS[0] * i);
                yPos2 = middle.y + (deltaMS[1] * i);
            }

            float interpX = (xPos * (1 - mu) + xPos * mu);
            float interpY = (yPos * (1 - mu) + yPos2 * mu);
            tmpVec.push_back(cv::Point(interpX, interpY));
        }

        return tmpVec;
    }

    void runInitialCellSegmentation(vector<Clump> *clumps, int threshold1, int threshold2, bool debug) {

        // run a find contour on the nucleiBoundaries to get them as contours, not regions
        for (unsigned int c = 0; c < clumps->size(); c++) {
            Clump *clump = &(*clumps)[c];
            clump->createCells();
            // sanity check: view all the cell array sizes for each clump
            printf("Clump: %u, # of nuclei boundaries: %lu, # of cells: %lu\n",
                   c, clump->nucleiBoundaries.size(), clump->cells.size());
        }


        /*
        Initial approach (grossly inefficient...but simple)
        for each clump:
        for each pixel:
        associate with the closest nuclei with a direct line not leaving the clump:
        measure distance to each nuclei
        for closest one, check to make sure it has direct line not leaving clump
        if it does, repeat with 2nd closest until you find one that works
        */
        for (unsigned int c = 0; c < clumps->size(); c++) {
            Clump *clump = &(*clumps)[c];

            for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
                vector<cv::Point> contour;
                std::ifstream inFile("clump" + to_string(c) + "cell" + to_string(cellIdx) + ".txt");
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
                cout << "Loaded contours from clump " << c << endl;
                continue;
            }
            
            associateClumpBoundariesWithCell(clump, c, debug);

            //TODO: Collapse these for-loops. They're nice for debugging, but unnecessary and computationally awful
            //TODO: Clean up your pointers. They're everywhere


            //Linear interpolation (TEST)
            //We've used a really odd approach above to generate our cell initial masks
            //Though it would likely be best to restructure the code, I don't have the time right now
            //However, since interpolation is necessary for proper overlap representation, we need to calc. something
            //TODO: Refactor

            //Proposed Approach: Take the assigned pixels, divide them into isolated binary "cell" masks
            //                   Find overlap between binary cell masks. Treat the extremal overlap points to create a polygon
            //                   (resembling a triangle), contraining these two extremal overlaps and the centroid of the shared cell mask

            //Issue: The cell masks don't overlap...
            //Fix: Pad the current cell's mask by 1 on all sides..? Admittedly, this seems awful.





            for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
                Cell *cell = &clump->cells[cellIdx];





                cv::Vec3d cellColor = cell->color;

                cv::Mat temp = clump->nucleiAssocs.clone(); //Copy clump mask
                temp.forEach<cv::Vec3d>([cellColor](cv::Vec3d &pixel, const int *position) -> void {
                                            if (pixel == cellColor) {
                                                pixel = {1.0, 1.0, 1.0};
                                            } else pixel = {0., 0., 0.};
                                        }
                );

                temp.convertTo(cell->cytoMask, CV_8UC3); //Keep an eye on this.. Iffy conversion?

                //cout << cell->cytoMask << endl;

                //	    cv::imshow("Cytoplasm mask " + to_string(cellIdx), cell->cytoMask * 1.0);
                //cv::waitKey(0);
            };

            //TODO: Finish implementation of Pre-interpolation Contours
            for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
                Cell *cell = &clump->cells[cellIdx];

                cv::Mat temp = cell->cytoMask.clone();


                cv::cvtColor(temp, temp, CV_RGB2GRAY);
                //cv::Mat initialAssignments = runCanny(temp, threshold1, threshold2, true); //TODO: Resolve issues with canny detection... I'm getting good results without it..? But we don't have any holes..
                //TODO: test with holes

                vector<vector<cv::Point>> initialContours;


                cv::findContours(temp, initialContours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

                //Remove fragments
                vector<cv::Point> filteredContour;
                int maxArea = 0;
                for (vector<cv::Point> contour : initialContours) {
                    int area = cv::contourArea(contour);
                    if (area > maxArea) {
                        maxArea = area;
                        filteredContour = contour;
                    }
                }
                cell->cytoBoundary = filteredContour;

                cv::Mat initMask = cv::Mat::zeros(clump->clumpMat.rows, clump->clumpMat.cols, CV_8U);
                cv::drawContours(initMask, vector<vector<cv::Point >>{cell->cytoBoundary}, 0, cv::Scalar(255),
                                 -1);

                cell->initCytoBoundaryMask = initMask;
                initMask.convertTo(cell->initCytoBoundaryMask, CV_8U);

                //cell.boundary = initialCells; //Set pre-interpolation contours
                //	    cv::imshow("Pre-interpolation " + to_string(cellIdx), cell->initCytoBoundaryMask);
                //	    cv::waitKey(0);

            }

            //Find geometric center
            for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
                Cell *cell = &clump->cells[cellIdx];

                cv::Moments m = cv::moments(cell->initCytoBoundaryMask, true);
                cv::Point p(m.m10 / m.m00, m.m01 / m.m00);
                cell->geometricCenter = p;


            }



            //Calculate extremal contour extensions for each cell
            for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
                Cell *cell = &clump->cells[cellIdx];



                cv::Mat initMask = cv::Mat::zeros(clump->clumpMat.rows, clump->clumpMat.cols, CV_8U);
                cv::drawContours(initMask, vector<vector<cv::Point >>{cell->cytoBoundary}, 0, cv::Scalar(255),
                                 2);
                cell->cytoMask = cell->initCytoBoundaryMask;
                //TODO: If we keep this approach shared edges could be memoized
                for (unsigned int compIdx = 0; compIdx < clump->cells.size(); compIdx++) {
                    if (cellIdx == compIdx) continue;

                    Cell *comparatorCell = &clump->cells[compIdx];
                    cv::Mat sharedEdge = initMask & comparatorCell->initCytoBoundaryMask;

                    if (cv::countNonZero(sharedEdge) > 0) {
                        //		cv::imshow("Shared Edge " + to_string(cellIdx) + ":" + to_string(compIdx), sharedEdge);
                        //		cv::waitKey(0);

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
                                testLineViability(startPt, clump, comparatorCell, compIdx)) {
                                start = startRef;
                                startPoint = startPt;
                                //cout << startPt << endl;
                            }

                            if (end == -1 && testLineViability(endPt, clump, comparatorCell, compIdx)) {
                                end = endRef;
                                endPoint = endPt;
                                //cout << endPt << endl;
                            }
                        }

                        //Verify viable extrapolation was found
                        if (start != -1 && end != -1) {
                            cv::Mat temp = cv::Mat::zeros(cell->initCytoBoundaryMask.rows,cell->initCytoBoundaryMask.cols, CV_8U);
                            float angle = angleBetween(startPoint, endPoint);
                            cv::Point midpoint = getMidpoint(startPoint, endPoint);
                            double width = cv::norm(startPoint-endPoint);
                            double height = width / 2;
                            cv::RotatedRect rotatedRect = cv::RotatedRect(midpoint, cv::Size2f(width,height), angle);
                            cv::ellipse(temp, rotatedRect, cv::Scalar(255), -1);
                            cv::bitwise_and(temp, comparatorCell->initCytoBoundaryMask, temp);


                            cell->cytoMask = cell->initCytoBoundaryMask + temp;


                            //cv::findContours(mask, cell->interpolatedContour, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

                            //displayMatrix()
                            //		  cv::imshow("Extrapolation " + to_string(cellIdx) + ":" + to_string(compIdx), mask);
                            //		  cv::waitKey(0);
                        }
                    }
                }
            }
            /*
            //TODO: Finish interpolation ****
            //Interpolate extremal contours for each cell
            for (unsigned int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
                Cell *cell = &clump->cells[cellIdx];

                //cv::Mat cellCoords;
                //cell->interpolatedContour.convertTo(cellCoords, CV_8UC1, 1.0);
                //cv::findNonZero(cellCoords, cellCoords);

                for (unsigned int compIdx = 0; compIdx < clump->cells.size(); compIdx++) {
                    if (cellIdx == compIdx) continue;

                    Cell *comparatorCell = &clump->cells[compIdx];

                    cv::Mat mask = cv::Mat::zeros(clump->clumpMat.rows, clump->clumpMat.cols, CV_64FC1);
                    cv::Mat out = cv::Mat::zeros(clump->clumpMat.rows, clump->clumpMat.cols, CV_64FC1);

                    cv::Mat coords;
                    comparatorCell->interpolatedContour.convertTo(coords, CV_8UC1, 1.0);
                    //cv::findNonZero(coords, coords);
                    coords.convertTo(coords, CV_16SC2);

                    cv::Mat empty = cv::Mat();

                    cout << coords.type() << " " << (coords.type() == CV_16SC2) << endl;
                    cout << empty.empty() << endl;
                    cv::remap(cell->interpolatedContour, out, coords, empty, INTER_CUBIC);



                    //cv::drawContours(cell->initCytoBoundaryMask, vector<vector<cv::Point>> {comparatorCell->interpolatedContour}, 0, cv::Scalar(1.0), 1, FILLED);

                    //cv::imshow("c", out);
                    //cv::waitKey(0);

                    //cv::imshow("Interp. " + to_string(cellIdx) + ":" + to_string(compIdx), cell->initCytoBoundaryMask);
                    //cv::waitKey(0);
                }
            }
*/
            clump->calcClumpPrior();

            // print the potential assignment
            char buffer[200];
            cv::Mat temp = clump->nucleiAssocs.clone();
            temp.convertTo(temp, CV_8UC3);
            sprintf(buffer, "../images/clumps/final_clump_%i_raw.png", c);
            cv::imwrite(buffer, temp);

            //TODO: Remove duplicated code
            cv::Mat initialAssignments = runCanny(temp, threshold1, threshold2);
            vector<vector<cv::Point>> initialCells;
            cv::findContours(initialAssignments, initialCells, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

            cv::Mat outimg;
            clump->clumpMat.copyTo(outimg);
            outimg.convertTo(outimg, CV_64FC3);

            cv::drawContours(outimg, clump->nucleiBoundaries, allContours, pink, 2);
            cv::drawContours(outimg, initialCells, allContours, cv::Scalar(1.0, 0, 0), 2);

            sprintf(buffer, "../images/clumps/final_clump_%i_boundaries_1.png", c);
            cv::imwrite(buffer, outimg);
        }
    }
}