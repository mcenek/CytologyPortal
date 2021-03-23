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

        double overlapArea = calcContoursArea(overlapContours);
        double estimatedArea = calcContoursArea(estimatedContours);
        double groundTruthArea = calcContoursArea(groundTruthContours);

        double dice = (2.0 * overlapArea) / (estimatedArea + groundTruthArea);
        return dice;
    }

    double calcTotalDice(cv::Mat estimated, cv::Mat groundTruth) {
        estimated = estimated.clone();
        groundTruth = groundTruth.clone();

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
        vector<cv::Mat> masks;
        for (vector<cv::Point> contour : contours) {
            cv::Mat mask = cv::Mat::zeros(rows, cols, CV_8UC1);
            cv::drawContours(mask, vector<vector<cv::Point>>{contour}, 0, 255, CV_FILLED);
            masks.push_back(mask);
        }
        return masks;
    }

    int* findMaxDiceLocation(vector<vector<double>> allDice) {
        double maxDice = 0;
        int *location = new int[2];

        for (int i = 0; i < allDice.size(); i++) {
            for (int j = 0; j < allDice[i].size(); j++) {
                double dice = allDice[i][j];
                if (dice > maxDice) {
                    maxDice = dice;
                    location[0] = i;
                    location[1] = j;

                }
            }
        }
        return location;
    }

    double evaluateSegmentation(Image *image) {
        vector<cv::Mat> estimatedMasks;
        vector<cv::Mat> groundTruthMasks;


        for (int i = 0; i < image->clumps.size(); i++) {
            Clump *clump = &image->clumps[i];
            vector<vector<cv::Point>> cellContours = clump->finalCellContours;
            vector<cv::Mat> masks = generateMasks(image->mat.rows, image->mat.cols, cellContours);
            for (cv::Mat mask : masks) {
                estimatedMasks.push_back(mask);
            }

        }

        boost::filesystem::path gtLocation(image->path.parent_path());
        gtLocation /= image->path.stem();
        gtLocation += "_GT";
        gtLocation /= image->path.stem();
        gtLocation += "_CytoGT";

        boost::filesystem::directory_iterator iter(gtLocation), eod;
        BOOST_FOREACH(boost::filesystem::path const& file, make_pair(iter, eod)){
            if (is_regular_file(file)) {
                if (file.has_extension()) {
                    if (file.extension().string() == ".png") {
                        cout << "Reading ground truth: " << file.string() << endl;
                        groundTruthMasks.push_back(cv::imread(file.string(), cv::IMREAD_GRAYSCALE));
                    }
                }
            }
        }

        vector<vector<double>> allDice;
        int associations[groundTruthMasks.size()];

        for (int i = 0; i < groundTruthMasks.size(); i++) {
            cv::Mat *groundTruthMask = &groundTruthMasks[i];

            allDice.push_back(vector<double>());
            for (int j = 0; j < estimatedMasks.size(); j++) {
                cv::Mat *estimatedMask = &estimatedMasks[j];
                double dice = calcTotalDice(*estimatedMask, *groundTruthMask);
                allDice[i].push_back(dice);
            }
            associations[i] = -1;
        }

        vector<double> finalDice;

        for (int k = 0; k < groundTruthMasks.size() && k < estimatedMasks.size(); k++) {

            int *location = findMaxDiceLocation(allDice);
            int groundTruth = location[0];
            int estimated = location[1];
            associations[groundTruth] = estimated;
            finalDice.push_back(allDice[groundTruth][estimated]);

            for (int j = 0; j < allDice[groundTruth].size(); j++) {
                allDice[groundTruth][j] = 0;
            }
        }

        double sumDice;
        for (double dice : finalDice) {
            sumDice += dice;
        }

        return sumDice / finalDice.size();
    }
}