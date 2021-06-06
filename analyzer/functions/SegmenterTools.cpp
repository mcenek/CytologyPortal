#include "SegmenterTools.h"
#include <cmath>

using namespace std;

namespace segment {

    //Returns a divergence of the given matrix
    cv::Mat calcDivergence(cv::Mat x, cv::Mat y) {
        vector <cv::Mat> gradientX = calcGradient(x);
        vector <cv::Mat> gradientY = calcGradient(y);
        cv::Mat gradientXX = getGradientX(gradientX);
        cv::Mat gradientYY = getGradientY(gradientY);
        return gradientXX + gradientYY;
    }

    vector <cv::Mat> calcGradient(cv::Mat image) {
        int rows = image.rows;
        int cols = image.cols;

        cv::Mat gradientX = cv::Mat(rows, cols, CV_32FC1);
        cv::Mat gradientY = cv::Mat(rows, cols, CV_32FC1);

        float dx = 0.0;
        float dy = 0.0;

        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                if (j == 0) {
                    dx = image.at<float>(i, j + 1) - image.at<float>(i, j);
                } else if (j == cols - 1) {
                    dx = image.at<float>(i, j) - image.at<float>(i, j - 1);
                } else {
                    dx = 0.5 * (image.at<float>(i, j + 1) - image.at<float>(i, j - 1));
                }

                if (i == 0) {
                    dy = image.at<float>(i + 1, j) - image.at<float>(i, j);
                } else if (i == rows - 1) {
                    dy = image.at<float>(i, j) - image.at<float>(i - 1, j);
                } else {
                    dy = 0.5 * (image.at<float>(i + 1, j) - image.at<float>(i - 1, j));
                }

                gradientX.at<float>(i, j) = dx;
                gradientY.at<float>(i, j) = dy;
            }
        }

        return {gradientX, gradientY};
    }

    //Returns the magnitude for the given matricies
    cv::Mat calcGradientMagnitude(cv::Mat img) {
        vector <cv::Mat> gradient = calcGradient(img);
        return calcMagnitude(getGradientX(gradient), getGradientY(gradient));
    }

    //Returns the magnitude for the given matricies
    cv::Mat calcMagnitude(cv::Mat x, cv::Mat y) {
        cv::Mat magnitude;
        cv::magnitude(x, y, magnitude);
        return magnitude;
    }

    cv::Mat getGradientX(vector <cv::Mat> gradient) {
        return gradient[0];
    }

    cv::Mat getGradientY(vector <cv::Mat> gradient) {
        return gradient[1];
    }

    bool hasOverlap(cv::Mat mat1, cv::Mat mat2) {
        cv::Mat thresholdMat1, thresholdMat2;
        cv::threshold(mat1, thresholdMat1, 0, 1, CV_THRESH_BINARY_INV);
        cv::threshold(mat2, thresholdMat2, 0, 1, CV_THRESH_BINARY_INV);
        cv::Mat overlapMask;
        cv::bitwise_and(thresholdMat1, thresholdMat2, overlapMask);
        return cv::countNonZero(overlapMask) > 0;
    }

    void displayMatrix(string name, cv::Mat mat) {
        cv::imshow(name, mat);
        printMatrix(name, mat);
        cv::waitKey(0);
    }

    void displayMatrixList(string prefix, vector<cv::Mat> matrices) {
        for (size_t i = 0; i < matrices.size(); i++) {
            string label = prefix + "_" + to_string(i);
            displayMatrix(label, matrices[i]);
        }
    }


    cv::Mat drawContour(cv::Mat img, vector<cv::Point> contour) {
        vector<vector<cv::Point>> contours = {contour};
        cv::drawContours(img, contours, 0, (255,255,255), 2);
        return img;
    }

    void printMatrix(string name, cv::Mat mat) {
        cout << name << endl;
        for (int i = 0; i < mat.rows; i++) {
            for (int j = 0; j < mat.cols; j++) {
                cout << setw(10) << mat.at<float>(i, j);
            }
            cout << endl;
        }
        cout << endl;
    }

    float angleBetween(const cv::Point v1, const cv::Point v2){
        float dy = v1.y - v2.y;
        float dx = v1.x - v2.x;
        float theta = atan(dy/dx);
        theta *= 180.0/M_PI;
        return theta;
    }

    cv::Mat drawColoredContours(cv::Mat img, vector<vector<cv::Point>> *contours) {
        img = img.clone();
        cv::RNG rng(12345);
        return *drawColoredContours(&img, contours, &rng);
    }

    cv::Mat* drawColoredContours(cv::Mat *img, vector<vector<cv::Point>> *contours, cv::RNG *rng) {
        for (int j = 0; j < contours->size(); j++) {
            if ((*contours)[j].empty()) {
                continue;
            }
            cv::Scalar color = cv::Scalar(rng->uniform(0,255), rng->uniform(0, 255), rng->uniform(0, 255));
            vector<vector<cv::Point>> contour = {(*contours)[j]};
            cv::drawContours(*img, contour, 0, color, 2);
        }
        return img;
    }

}