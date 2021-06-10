#include "Cell.h"
#include "opencv2/opencv.hpp"
#include "../functions/SegmenterTools.h"

double getRandomDouble(double x, double y) {
    double random = ((double) rand()) / (double) RAND_MAX;
    double diff = y - x;
    double r = random * diff;
    return x + r;
}

namespace segment {
    float Cell::calcMaxRadius() {
        float maxDist = 0;
        for (cv::Point p : this->cytoBoundary) {
            float dist = cv::norm(p - this->geometricCenter);
            if (dist > maxDist) maxDist = dist;
        }
        return maxDist;
    }

    cv::Vec3b Cell::generateColor() {
        this->color = cv::Vec3d(getRandomDouble(0.2, 0.8), getRandomDouble(0.2, 0.8),
                                getRandomDouble(0.2, 0.8));
        return this->color;
    }

    // if the nucleusBoundary is defined, compute the center of the nucleus
    cv::Point Cell::computeNucleusCenter() {
        if (this->nucleusBoundary.empty())
            cerr << "nucleusBoundary must be defined and present before Cell::computeNucleusCenter() can be run." << "\n";
        double sumX = 0.0, sumY = 0.0;
        int count = 0;
        for (cv::Point p : nucleusBoundary) {
            sumX += p.x;
            sumY += p.y;
            count++;
        }
        this->nucleusCenter = cv::Point(sumX / count, sumY / count);

        return this->nucleusCenter;
    }

    void Cell::generateBoundaryFromMask() {
        vector<vector<cv::Point>> contours;
        cv::findContours(this->cytoMask, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
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
            this->cytoBoundary = maxContour;
        }
    }

    void Cell::generateMaskFromBoundary() {
        this->cytoMask = cv::Mat::zeros(this->clump->boundingRect.height, this->clump->boundingRect.width, CV_8U);
        cv::drawContours(this->cytoMask, vector<vector<cv::Point>>{this->cytoBoundary}, 0, 255, CV_FILLED);
    }

    cv::Rect Cell::findBoundingBox() {
        cv::Rect boundingBox = cv::boundingRect(this->cytoBoundary);
        for (int i = 0; i < this->neighbors.size(); i++) {
            Cell *neighbor = this->neighbors[i];
            cv::Rect neighborBoundingBox = cv::boundingRect(neighbor->cytoBoundary);
            boundingBox = boundingBox | neighborBoundingBox;
        }
        this->boundingBox = boundingBox;
        return boundingBox;
    }

    void Cell::initializePhi() {
        this->generateMaskFromBoundary();

        this->calcGeometricCenter();
        this->cytoMask.convertTo(this->phi, CV_32FC1, 1.0/255);
        this->cytoMask.release();


        for (int i = 0; i < this->phi.rows; i++) {
            float *row = this->phi.ptr<float>(i);
            for (int j = 0; j < this->phi.cols; j++) {
                float value = row[j];
                if (value != 0) row[j] = -2;
                else row[j] = 2;
            }
        }

        this->phiArea = this->getPhiArea();
        this->phiConverged = false;
    }


    double Cell::getPhiArea() {
        cv::Mat temp;
        cv::threshold(this->phi, temp, 0, 1, cv::THRESH_BINARY_INV);
        temp.convertTo(temp, CV_8UC1, 255);
        vector<vector<cv::Point>> contours;
        cv::findContours(temp, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
        double area = 0;
        //Looking for the biggest contour
        for (vector<cv::Point> contour : contours) {
            double contourArea = cv::contourArea(contour);
            area = max(contourArea, area);
        }
        return area;
    }

    vector<cv::Point> Cell::getPhiContour() {
        cv::Rect cropRect(10, 10, this->clump->boundingRect.width, this->clump->boundingRect.height);
        cv::Mat temp = this->phi.clone();
        temp = temp(cropRect);
        cv::threshold(temp, temp, 0, 1, cv::THRESH_BINARY_INV);
        temp.convertTo(temp, CV_8UC1, 255);
        vector<vector<cv::Point>> contours;
        cv::findContours(temp, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
        double area = 0;
        vector<cv::Point> phiContour;
        //Looking for the biggest contour
        for (vector<cv::Point> contour : contours) {
            double contourArea = cv::contourArea(contour);
            if (contourArea > area) {
                area = contourArea;
                phiContour = contour;
            }
        }

        return phiContour;
    }


    cv::Point Cell::calcGeometricCenter() {
        cv::Moments m = cv::moments(this->cytoMask, true);
        cv::Point p(m.m10 / m.m00, m.m01 / m.m00);
        this->geometricCenter = p;
        return p;
    }

    cv::Mat Cell::calcShapePrior() {
        cv::Mat shapePrior;
        this->phi.convertTo(shapePrior, CV_8UC1, 255);
        shapePrior = 1 - shapePrior;

        vector<vector<cv::Point>> contours;

        cv::findContours(shapePrior, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

        shapePrior = cv::Mat::ones(this->phi.rows, this->phi.cols, CV_32FC1);
        for (int i = 0; i < shapePrior.rows; i++) {
            for (int j = 0; j < shapePrior.cols; j++) { //Assumes a single channel matrix
                if (cv::pointPolygonTest(contours[0], cv::Point(j, i), false) >= 0) {
                    float dist = cv::norm(cv::Point(j, i) - this->geometricCenter) / (this->calcMaxRadius());
                    shapePrior.at<float>(i, j) = (-2.0 / (1.0 + exp(-1 * 5 * dist)) + 2.0);
                } else if (cv::pointPolygonTest(clump->offsetContour, cv::Point(j, i), false) > 0)
                    shapePrior.at<float>(i, j) = 0.0;
            }
        }
        return shapePrior;
    }

}