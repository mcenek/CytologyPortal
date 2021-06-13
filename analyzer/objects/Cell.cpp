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

    cv::Rect Cell::findBoundingBoxWithNeighbors() {
        cv::Rect boundingBox = cv::boundingRect(this->cytoBoundary);
        for (int i = 0; i < this->neighbors.size(); i++) {
            Cell *neighbor = this->neighbors[i];
            cv::Rect neighborBoundingBox = cv::boundingRect(neighbor->cytoBoundary);
            boundingBox = boundingBox | neighborBoundingBox;
        }

        return boundingBox;
    }

    void Cell::initializePhi() {
        this->boundingBox = cv::boundingRect(this->cytoBoundary);
        this->boundingBoxWithNeighbors = this->findBoundingBoxWithNeighbors();

        vector<cv::Point> initialPhiContour;
        for (cv::Point &point : this->cytoBoundary) {
            cv::Point newPoint = cv::Point(point.x - this->boundingBox.x, point.y - this->boundingBox.y);
            initialPhiContour.push_back(newPoint);
        }

        this->phi = cv::Mat::zeros(this->boundingBox.height, this->boundingBox.width, CV_32FC1);
        cv::drawContours(this->phi, vector<vector<cv::Point>>{initialPhiContour}, 0, 1, CV_FILLED);
        this->phi = this->phi * -4 + 2;

        this->phiArea = this->getPhiArea();
        this->phiConverged = false;
    }

    cv::Mat Cell::getPhi(cv::Rect bb) {
        int top = this->boundingBox.y - bb.y;
        int bottom = bb.height - this->boundingBox.height - top;
        int left = this->boundingBox.x - bb.x;
        int right = bb.width - this->boundingBox.width - left;
        cv::Mat x;

        int padding = 10;
        top += padding;
        bottom += padding;
        left += padding;
        right += padding;

        cv::copyMakeBorder(this->phi, x, top, bottom, left, right, cv::BORDER_CONSTANT, 2);
        return x;
    }

    cv::Mat Cell::getPhi() {
        return getPhi(this->boundingBoxWithNeighbors);
    }

    void Cell::setPhi(cv::Mat phi) {
        int top = this->boundingBox.y - this->boundingBoxWithNeighbors.y;
        int left = this->boundingBox.x - this->boundingBoxWithNeighbors.x;

        int padding = 10;
        top += padding;
        left += padding;

        cv::Rect roi(left, top, this->boundingBox.width, this->boundingBox.height);
        phi = phi.clone();
        this->phi = phi(roi);
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
        //cv::Rect cropRect(10, 10, this->clump->boundingRect.width, this->clump->boundingRect.height);
        cv::Mat temp = this->phi.clone();
        //temp = temp(cropRect);
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
        vector<cv::Point> actualPhiContour;
        for (cv::Point &point : phiContour) {
            cv::Point newPoint = cv::Point(point.x + this->boundingBox.x, point.y + this->boundingBox.y);
            actualPhiContour.push_back(newPoint);
        }


        return actualPhiContour;
    }


    cv::Point Cell::calcGeometricCenter() {
        this->generateMaskFromBoundary();
        cv::Moments m = cv::moments(this->cytoMask, true);
        this->cytoMask.release();
        cv::Point p(m.m10 / m.m00, m.m01 / m.m00);
        this->geometricCenter = p;
        return p;
    }

    cv::Mat Cell::calcShapePrior() {
        this->initializePhi();
        this->calcGeometricCenter();
        cv::Point geometricCenter(this->geometricCenter.x - this->boundingBox.x, this->geometricCenter.y - this->boundingBox.y);

        cv::Mat shapePrior = this->phi.clone();

        shapePrior = -shapePrior;

        float maxRadius = this->calcMaxRadius();
        for (int i = 0; i < shapePrior.rows; i++) {
            for (int j = 0; j < shapePrior.cols; j++) { //Assumes a single channel matrix
                cv::Point point(j, i);
                float currentValue = shapePrior.at<float>(point);
                if (currentValue == 2) {
                    float dist = cv::norm(point - geometricCenter) / maxRadius;
                    shapePrior.at<float>(point) = (-2.0 / (1.0 + exp(-1 * 5 * dist)));
                }
            }
        }
        shapePrior += 2;

        return shapePrior;
    }

}