#include "opencv2/opencv.hpp"
#include "EvaluateSegmentation.h"
#include "../objects/Image.h"
#include "boost/filesystem.hpp"
#include "boost/foreach.hpp"

using namespace std;

namespace segment {

    double calcContoursArea(vector<vector<cv::Point>> contours) {
        double area = 0;
        for (vector<cv::Point> contour : contours) {
            area += cv::contourArea(contour);
        }
        return area;
    }

    double calcDice(cv::Mat estimated, cv::Mat groundTruth) {
        vector<vector<cv::Point>> estimatedContours;
        vector<vector<cv::Point>> groundTruthContours;

        cv::findContours(estimated, estimatedContours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
        cv::findContours(groundTruth, groundTruthContours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

        cv::Mat overlap;
        cv::bitwise_and(estimated, groundTruth, overlap);

        vector<vector<cv::Point>> overlapContours;
        cv::findContours(overlap, overlapContours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

        double overlapArea = cv::contourArea(overlapContours);
        double estimatedArea = cv::contourArea(estimatedContours);
        double groundTruthArea = cv::contourArea(groundTruthContours);

        double dice = (2.0 * overlapArea) / (estimatedArea + groundTruthArea);
        return dice;
    }

    double calcTotalDice(cv::Mat estimated, cv::Mat groundTruth) {
        estimated.convertTo(estimated, CV_8UC1);
        groundTruth.convertTo(groundTruth, CV_8UC1);


        double foregroundDice = calcDice(estimated, groundTruth);

        cv::bitwise_not(estimated, estimated);
        cv::bitwise_not(groundTruth, groundTruth);

        double backgroundDice = calcDice(estimated, groundTruth);

        double totalDice = (foregroundDice + backgroundDice) / 2.0;
        return totalDice;
    }

    vector<cv::Mat> generateMasks(int rows, int cols, vector<vector<cv::Point>> contours) {
        vector<cv::Mat> estimatedMasks;
        for (vector<cv::Point> contour : contours) {
            cv::Mat estimatedMask = cv::Mat::zeros(rows, cols, CV_8UC1);
            cv::drawContours(estimatedMask, vector<vector<cv::Point>>{contour}, 255, CV_FILLED);
            estimatedMasks.push_back(estimatedMask);
        }
        return estimatedMasks;
    }


    double evaluateSegmentation(Image *image) {
        vector<cv::Mat> estimatedMasks;
        for (int i = 0; i < image->clumps.size(); i++) {
            Clump *clump = &image->clumps[i];
            vector<vector<cv::Point>> cellContours = clump->finalCellContours;
            vector<cv::Mat> masks = generateMasks(image->mat.rows, image->mat.cols, cellContours);
            for (cv::Mat mask : masks) {
                estimatedMasks.push_back(mask);
            }

        }

        string groundTruthsLocation = "./images/Synthetic/Synthetic000.png";

        //cv::Mat groundTruth = cv::imread("./images/Synthetic/Synthetic000_.png");

        boost::filesystem::path targetDir(groundTruthsLocation);
        boost::filesystem::recursive_directory_iterator iter(targetDir), eod;
        BOOST_FOREACH(boost::filesystem::path const& i, make_pair(iter, eod)){
            if (is_regular_file(i)) {
                cout << i.string() << endl;
            }
        }
        return 0;
    }
}