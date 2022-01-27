#include "Clump.h"
#include <iterator>
#include "opencv2/opencv.hpp"

using namespace std;

namespace segment {

    /*
     * padBoundingBox returns a bounding box with padding
     */
    cv::Rect padBoundingBox(cv::Mat mat, cv::Rect boundingBox, int padding) {
        cv::Rect returnRect = cv::Rect(boundingBox.x - padding, boundingBox.y - padding, boundingBox.width + (padding * 2), boundingBox.height + (padding * 2));
        if (returnRect.x < 0)returnRect.x = 0;
        if (returnRect.y < 0)returnRect.y = 0;
        //if (returnRect.x+ returnRect.width >= mat.cols)returnRect.width = mat.cols-returnRect.x;
        //if (returnRect.y+returnRect.height >= mat.rows)returnRect.height = mat.rows-returnRect.y;
        return returnRect;
    }

    /*
     * computeBoundingRect compiutes the bounding rectangle of a clump
     */
    cv::Rect Clump::computeBoundingRect(cv::Mat img) {
        if(this->contour.empty())
            std::cerr << "Contour must be defined before Clump::computeBoundingRect() can be run." << '\n';
        this->boundingRect = cv::boundingRect(this->contour);

        return this->boundingRect;
    }

    /*
     * computeOffsetContour computes the contour of a clump relative to (0, 0)
     */
    vector<cv::Point> Clump::computeOffsetContour() {
        for(unsigned int i=0; i<this->contour.size(); i++)
            this->offsetContour.push_back(cv::Point(this->contour[i].x - this->boundingRect.x, this->contour[i].y - this->boundingRect.y));
        return this->offsetContour;
    }

    /*
     * undoOffsetContour undoes the bounding rectangle of nuclei so that they are relative to the image
     */
    vector<vector<cv::Point>> Clump::undoOffsetContour() {
        vector<vector<cv::Point>> tmpBoundaries = this->nucleiBoundaries;

        for(unsigned int i = 0; i < tmpBoundaries.size(); i++) {
            tmpBoundaries[i] = this->undoBoundingRect(tmpBoundaries[i]);
        }
        return tmpBoundaries;

    }

    /*
     * undoBoundingRect undoes the bounding rectangle of a contour so that they are relative to the image
     */
    vector<cv::Point> Clump::undoBoundingRect(vector<cv::Point> boundary) {

        for(unsigned int j = 0; j < boundary.size(); j++) {
            boundary[j] = cv::Point(boundary[j].x + this->boundingRect.x, boundary[j].y + this->boundingRect.y);
        }

        return boundary;
    }

    /*
     * extractFull returns a masked clump such that anything outside the clump is 1.0
     * and anything inside the clump is the image of the clump
     */
    cv::Mat Clump::extractFull(bool showBoundary)
    {
        cv::Mat img = this->image->mat;

        // create clump mask
        cv::Mat mask = cv::Mat::zeros(img.rows, img.cols, CV_8U);
        cv::drawContours(mask, vector<vector<cv::Point> >(1, this->contour), 0, cv::Scalar(1.0), CV_FILLED);
        cv::Mat clumpFull = cv::Mat::zeros(img.rows, img.cols, CV_64F);
        img.copyTo(clumpFull, mask);

        if (showBoundary)
            cv::drawContours(clumpFull, vector<vector<cv::Point> >(1, this->contour), 0, cv::Scalar(1.0, 0., 1.0));

        // invert the mask and then invert the black pixels in the extracted image
        cv::bitwise_not(clumpFull, clumpFull, mask);
        cv::bitwise_not(clumpFull, clumpFull);

        return clumpFull;
    }

    /*
     * extract returns a cropped version of the clump and masks it such that anything outside the clump is 1.0
     * and anything inside the clump is the image of the clump and optionally adds a boundary to the clump.
     */
    cv::Mat Clump::extract(bool showBoundary)
    {
        cv::Mat img = this->extractFull(showBoundary);

        cv::Mat clump = cv::Mat(img, this->boundingRect);
        if (showBoundary)
            cv::drawContours(clump, vector<vector<cv::Point> >(1, this->contour), 0, cv::Scalar(1.0, 0.0, 1.0));


        //this->mat = clump;
        return clump;
    }

    /*
     * computeNucleusCenters computes the center of each nuclei
     */
    vector<cv::Point> Clump::computeNucleusCenters()
    {
        if(this->nucleiBoundaries.empty())
            cerr << "nucleiBoundaries must be defined and present before Clump::computeNucleusCenters() can be run." << "\n";
        this->nucleiCenters.clear();
        for(vector<cv::Point> nucleus : nucleiBoundaries)
        {
            double sumX = 0.0, sumY = 0.0;
            int count = 0;
            for(cv::Point p : nucleus)
            {
                sumX += p.x;
                sumY += p.y;
                count++;
            }
            this->nucleiCenters.push_back(cv::Point(sumX/count, sumY/count));
        }

        return this->nucleiCenters;
    }

    /*
     * createCells creates Cell objects for each nucleus detected in the clump.
     */
    void Clump::createCells() {
        if(this->nucleiBoundaries.empty())
            cerr << "nucleiBoundaries must be defined and present before Clump::createCells can be run." << "\n";
        for (unsigned int n = 0; n < this->nucleiBoundaries.size(); n++) {
            vector<cv::Point> *boundary = &this->nucleiBoundaries[n];
            Cell cell;
            cell.clump = this;
            cell.nucleusBoundary = vector<cv::Point>(*boundary);
            cell.nucleusArea = cv::contourArea(cell.nucleusBoundary);
            cell.computeNucleusCenter();
            this->cells.push_back(cell);
        }
    }

    /*
     * convertNucleiBoundariesToContours takes a mask of nuclei boundaries and converts them to contours
     */
    vector<vector<cv::Point>> Clump::convertNucleiBoundariesToContours(vector<vector<cv::Point>> nucleiBoundaries) {
        vector<vector<cv::Point>> nucleiContours;
        cv::Mat regionMask = cv::Mat::zeros(this->boundingRect.height, this->boundingRect.width, CV_8U);
        cv::drawContours(regionMask, nucleiBoundaries, -1, cv::Scalar(1.0));
        cv::findContours(regionMask, nucleiContours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
        return nucleiContours;
    }

    /*
     * filterNuclei filters nuclei above a specified circularity
     */
    vector<vector<cv::Point>> Clump::filterNuclei(vector<vector<cv::Point>> nucleiBoundaries, double minCircularity) {
        vector<vector<cv::Point>> filteredNuclei;
        for (int i = 0; i < nucleiBoundaries.size(); i++) {
            vector<cv::Point> *nucleus = &nucleiBoundaries[i];
            double area = cv::contourArea(*nucleus);
            double perimeter = cv::arcLength(*nucleus, true);
            double circularity = 4 * M_PI * area / pow(perimeter, 2.0);
            if (circularity >= minCircularity) {
                filteredNuclei.push_back(*nucleus);
            }
        }
        return filteredNuclei;
    }

    /*
    void Clump::generateNucleiMasks() {
        if(this->nucleiBoundaries.empty())
            cerr << "nucleiBoundaries must be defined and present before Clump::generateNucleiMasks can be run." << "\n";
        for (unsigned int cellIdx = 0; cellIdx < this->cells.size(); cellIdx++) {
            Cell *cell = &this->cells[cellIdx];

            cv::Mat nucleusMask = cv::Mat::zeros(this->boundingRect.height, this->boundingRect.width, CV_8U);
            cv::drawContours(nucleusMask, this->nucleiBoundaries, cellIdx, cv::Scalar(255), CV_FILLED);

            cell->nucleusMask = nucleusMask;
            cell->nucleusArea = cv::contourArea(this->nucleiBoundaries[cellIdx]);

            //cv::imshow("Nucleus mask " + to_string(cellIdx), nucleusMask * 1.0);
            //cv::waitKey(0);
        }
    }
     */

    /*
     * calcClumpPrior calculates a clump prior of a clump by finding the maximum of each cell's shape prior
     * and masking the clump
     */
    cv::Mat Clump::calcClumpPrior() {
        this->clumpPrior = cv::Mat::ones(this->boundingRect.height, this->boundingRect.width, CV_32FC1);
        //Create a mask of the clump
        cv::drawContours(this->clumpPrior, vector<vector<cv::Point>>{this->offsetContour}, 0, 0, CV_FILLED);
        for (unsigned int cellIdx = 0; cellIdx < this->cells.size(); cellIdx++) {
            Cell *cell = &this->cells[cellIdx];
            //The shape prior is relative to the cell's bounding box, so we have to make it relative to the clump
            cv::Mat shapePrior = cell->calcShapePrior();
            for (int i = 0; i < shapePrior.rows; i++) {
                for (int j = 0; j < shapePrior.cols; j++) {
                    //Make shape prior relative to the clump
                    cv::Point shapePriorPoint(j, i);
                    cv::Point clumpPriorPoint(cell->boundingBox.x + j, cell->boundingBox.y + i);
                    float shapePriorValue = shapePrior.at<float>(shapePriorPoint);
                    float clumpPriorValue = this->clumpPrior.at<float>(clumpPriorPoint);
                    //Set clump prior to maximum of the shape priors
                    if (shapePriorValue > clumpPriorValue) {
                        this->clumpPrior.at<float>(clumpPriorPoint) = shapePriorValue;
                    }
                }
            }
            //cv::imshow("Shape prior", cell->shapePrior);
            //cv::waitKey(0);
        }
        //cv::imshow("Clump prior", this->clumpPrior);
        //cv::waitKey(0);
        return this->clumpPrior;
    }

    /*
     * getFinalCellContours returns a list of final cell contours
     */
    vector<vector<cv::Point>> Clump::getFinalCellContours() {
        vector<vector<cv::Point>> finalContours;
        for (unsigned int cellIdx = 0; cellIdx < this->cells.size(); cellIdx++) {
            Cell *cell = &this->cells[cellIdx];
            finalContours.push_back(this->undoBoundingRect(cell->finalContour));
        }
        return finalContours;
    }

}