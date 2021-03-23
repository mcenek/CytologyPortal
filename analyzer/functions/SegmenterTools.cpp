#include "opencv2/opencv.hpp"
#include "SegmenterTools.h"

#include <math.h>

using namespace std;

namespace segment {
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
            cv::drawContours(*img, contour, 0, color, 3);
        }
        return img;
    }

}