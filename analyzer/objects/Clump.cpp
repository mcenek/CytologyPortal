#include <iterator>
#include "opencv2/opencv.hpp"
#include "Clump.h"

using namespace std;

namespace segment {


    cv::Rect padBoundingBox(cv::Mat mat, cv::Rect boundingBox, int padding) {
        cv::Rect returnRect = cv::Rect(boundingBox.x - padding, boundingBox.y - padding, boundingBox.width + (padding * 2), boundingBox.height + (padding * 2));
        if (returnRect.x < 0)returnRect.x = 0;
        if (returnRect.y < 0)returnRect.y = 0;
        //if (returnRect.x+ returnRect.width >= mat.cols)returnRect.width = mat.cols-returnRect.x;
        //if (returnRect.y+returnRect.height >= mat.rows)returnRect.height = mat.rows-returnRect.y;
        return returnRect;
    }

    // Given that contour is defined, compute the bounding rect
    cv::Rect Clump::computeBoundingRect(cv::Mat img)
    {
        if(this->contour.empty())
            std::cerr << "Contour must be defined before Clump::computeBoundingRect() can be run." << '\n';
        this->boundingRect = cv::boundingRect(this->contour);

        return this->boundingRect;
    }

    vector<cv::Point> Clump::computeOffsetContour()
    {
        if(this->boundingRect.empty())
            std::cerr << "boundingRect must be defined before Clump::computeOffsetContour() can be run." << '\n';
        for(unsigned int i=0; i<this->contour.size(); i++)
            this->offsetContour.push_back(cv::Point(this->contour[i].x - this->boundingRect.x, this->contour[i].y - this->boundingRect.y));
        return this->offsetContour;
    }

    vector<vector<cv::Point>> Clump::undoOffsetContour() {
        vector<vector<cv::Point>> tmpBoundaries = this->nucleiBoundaries;

        for(unsigned int i = 0; i < tmpBoundaries.size(); i++) {
            tmpBoundaries[i] = this->undoBoundingRect( tmpBoundaries[i]);
        }
        return tmpBoundaries;

    }

    vector<cv::Point> Clump::undoBoundingRect(vector<cv::Point> boundary) {

        for(unsigned int j = 0; j < boundary.size(); j++) {
            boundary[j] = cv::Point(boundary[j].x + this->boundingRect.x, boundary[j].y + this->boundingRect.y);
        }

        return boundary;
    }

    // Mask the clump from the original image, return the result
    cv::Mat Clump::extractFull(bool showBoundary)
    {
        cv::Mat img = this->image->mat;

        // create clump mask
        cv::Mat mask = cv::Mat::zeros(img.rows, img.cols, CV_8U);

        cv::drawContours(mask, vector<vector<cv::Point> >(1, this->contour), 0, cv::Scalar(1.0), CV_FILLED);

        cv::Mat clumpFull = cv::Mat::zeros(img.rows, img.cols, CV_64F);

        img.copyTo(clumpFull, mask);


        //TODO - Remove
        /*cout << "Clump Mask" << endl;
          cv::imshow("Clump Mask", mask);
          cv::waitKey(0);*/

        if (showBoundary)
            cv::drawContours(clumpFull, vector<vector<cv::Point> >(1, this->contour), 0, cv::Scalar(1.0, 0., 1.0));

        // invert the mask and then invert the black pixels in the extracted image
        cv::bitwise_not(clumpFull, clumpFull, mask);
        cv::bitwise_not(clumpFull, clumpFull);

        //TODO - Remove
        /*cout << "Inverted Clump Mask" << endl;
          cv::imshow("Clump Mask", clumpFull);
          cv::waitKey(0);*/


        return clumpFull;
    }

    // Mask the clump from the original image, then return a mat cropped to show
    // only this clump
    cv::Mat Clump::extract(bool showBoundary)
    {
        cv::Mat img = this->extractFull(showBoundary);

        if (this->boundingRect.empty())
            std::cerr << "boundingRect must be defined before Clump::extract() can be run." << '\n';

        cv::Mat clump = cv::Mat(img, this->boundingRect);
        if (showBoundary)
            cv::drawContours(clump, vector<vector<cv::Point> >(1, this->contour), 0, cv::Scalar(1.0, 0.0, 1.0));


        this->clumpMat = clump;
        return this->clumpMat;
    }

    // If nucleiBoundaries are defined, compute the center of each nuclei
    vector<cv::Point> Clump::computeCenters()
    {
        if(this->nucleiBoundaries.empty())
            cerr << "nucleiBoundaries must be defined and present before Clump::computeCenters() can be run." << "\n";
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

    void Clump::createCells() {
        if(this->nucleiBoundaries.empty())
            cerr << "nucleiBoundaries must be defined and present before Clump::createCells can be run." << "\n";
        for (unsigned int n = 0; n < this->nucleiBoundaries.size(); n++) {
            vector<cv::Point> *boundary = &this->nucleiBoundaries[n];
            Cell cell;
            cell.clump = this;
            cell.nucleusBoundary = vector<cv::Point>(*boundary);
            cell.computeCenter();
            cell.generateColor(); //TODO: Not strictly guarenteed to be unique.. Could lead to segmentation errors as currently implemented

            this->cells.push_back(cell);
        }
        this->generateNucleiMasks();
    }

    void Clump::convertNucleiBoundariesToContours() {
        if(this->nucleiBoundaries.empty())
            cerr << "nucleiBoundaries must be defined and present before Clump::convertNucleiBoundariesToContours can be run." << "\n";
        cv::Mat regionMask = cv::Mat::zeros(this->clumpMat.rows, this->clumpMat.cols, CV_8U);
        cv::drawContours(regionMask, this->nucleiBoundaries, -1, cv::Scalar(1.0));
        cv::findContours(regionMask, this->nucleiBoundaries, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
    }

    void Clump::generateNucleiMasks() {
        if(this->nucleiBoundaries.empty())
            cerr << "nucleiBoundaries must be defined and present before Clump::generateNucleiMasks can be run." << "\n";
        for (unsigned int cellIdx = 0; cellIdx < this->cells.size(); cellIdx++) {
            Cell *cell = &this->cells[cellIdx];

            cv::Mat nucleusMask = cv::Mat::zeros(this->clumpMat.rows, this->clumpMat.cols, CV_8U);
            cv::drawContours(nucleusMask, this->nucleiBoundaries, cellIdx, cv::Scalar(255), CV_FILLED);

            cell->nucleusMask = nucleusMask;
            cell->nucleusArea = cv::contourArea(this->nucleiBoundaries[cellIdx]);

            //cv::imshow("Nucleus mask " + to_string(cellIdx), nucleusMask * 1.0);
            //cv::waitKey(0);
        }
    }

    cv::Mat Clump::calcClumpPrior() {
        for (unsigned int cellIdx = 0; cellIdx < this->cells.size(); cellIdx++) {
            Cell *cell = &this->cells[cellIdx];
            if (cell->phi.empty()) cell->initializePhi();
            cell->calcShapePrior();
            if (this->clumpPrior.empty()) this->clumpPrior = cell->shapePrior;
            else cv::max(this->clumpPrior, cell->shapePrior, this->clumpPrior);

            //cv::imshow("Shape prior", cell->shapePrior);
            //cv::waitKey(0);
        }
        //cv::imshow("Clump prior", this->clumpPrior);
        //cv::waitKey(0);
        return this->clumpPrior;
    }

    vector<vector<cv::Point>> Clump::getFinalCellContours() {
        for (unsigned int cellIdx = 0; cellIdx < this->cells.size(); cellIdx++) {
            Cell *cell = &this->cells[cellIdx];

            cell->finalContour = this->undoBoundingRect(cell->getPhiContour());
            this->finalCellContours.push_back(cell->finalContour);
        }

        return this->finalCellContours;
    }

}