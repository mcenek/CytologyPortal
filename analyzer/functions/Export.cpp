#include "Export.h"


using json = nlohmann::json;

namespace segment {
    json removeClumpArrays(json &boundaries) {
        int count = 0;
        json newBoundaries;
        for (json &clump : boundaries) {
            for (json &boundary: clump) {
                newBoundaries.push_back(boundary);
                count++;
            }
        }
        cout << "cells: " << count << endl;
        return newBoundaries;

    }

    json contourToJson(vector<cv::Point> contour) {
        json converted;
        for (cv::Point &point : contour) {
            converted.push_back({point.x, point.y});
        }
        return converted;
    }

    void exportResults(Image *image) {
        json results;

        boost::filesystem::remove_all(image->getWriteDirectory() / "thumbnails");

        json nucleiBoundaries;
        json finalCellBoundaries;
        json nucleiCytoRatios;
        json thumbnails;

        int i = 0;
        for (int clumpIdx = 0; clumpIdx < image->clumps.size(); clumpIdx++) {
            Clump *clump = &image->clumps[clumpIdx];
            cv::Mat clumpMat = image->mat.clone();
            clumpMat = clumpMat(clump->boundingRect);
            for (int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
                Cell *cell = &clump->cells[cellIdx];
                cv::Mat thumbnail = clumpMat.clone();
                if (cell->finalContour.empty() || cell->nucleusBoundary.empty()) {
                    continue;
                }

                cv::drawContours(thumbnail, vector<vector<cv::Point>>{cell->finalContour}, 0, cv::Scalar(255, 0, 0), 2);
                cv::drawContours(thumbnail, vector<vector<cv::Point>>{cell->nucleusBoundary}, 0, cv::Scalar(0, 0, 255), 2);

                thumbnail = thumbnail(cell->boundingBoxWithNeighbors);
                string fileName = to_string(i);
                fileName += "_" + to_string((int) round(cell->phiArea));
                fileName += "_" + to_string((int) round(cell->nucleusArea));

                image->writeImage("thumbnails/" + fileName + ".png", thumbnail);

                nucleiBoundaries.push_back(contourToJson(cell->nucleusBoundary));
                finalCellBoundaries.push_back(contourToJson(cell->finalContour));
                nucleiCytoRatios.push_back(cell->nucleusArea / cell->phiArea);
                thumbnails.push_back(fileName + ".png");
                i++;

            }

        }

        //results["nucleiBoundaries"] = nucleiBoundaries;
        //results["cytoBoundaries"] = finalCellBoundaries;
        results["nucleiCytoRatios"] = nucleiCytoRatios;
        results["thumbnails"] = thumbnails;

        image->writeJSON("export.json", results);
    }
}