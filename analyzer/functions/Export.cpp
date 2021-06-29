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

    void exportResults(Image *image) {
        json results;

        json nucleiBoundaries = image->loadJSON("nucleiBoundaries");
        results["nucleiBoundaries"] = removeClumpArrays(nucleiBoundaries);

        json finalCellBoundaries = image->loadJSON("finalCellBoundaries");
        results["cytoBoundaries"] = removeClumpArrays(finalCellBoundaries);

        json nucleiCytoRatios = image->loadJSON("nucleiCytoRatios");
        results["nucleiCytoRatios"] = removeClumpArrays(nucleiCytoRatios);

        boost::filesystem::remove_all(image->getWriteDirectory() / "exports" / "thumbnails");

        int i = 0;
        for (int clumpIdx = 0; clumpIdx < image->clumps.size(); clumpIdx++) {
            Clump *clump = &image->clumps[clumpIdx];
            cv::Mat clumpMat = image->mat.clone();
            clumpMat = clumpMat(clump->boundingRect);
            for (int cellIdx = 0; cellIdx < clump->cells.size(); cellIdx++) {
                Cell *cell = &clump->cells[cellIdx];
                cv::Mat thumbnail = clumpMat.clone();
                cv::drawContours(thumbnail, vector<vector<cv::Point>>{cell->finalContour}, 0, cv::Scalar(255, 0, 0), 2);
                cv::drawContours(thumbnail, vector<vector<cv::Point>>{cell->nucleusBoundary}, 0, cv::Scalar(0, 0, 255), 2);

                cv::Rect cellBounding = cv::boundingRect(cell->cytoBoundary);
                thumbnail = thumbnail(cellBounding);
                string fileName = to_string(i);
                fileName += "_" + to_string((int) round(cell->phiArea));
                fileName += "_" + to_string((int) round(cell->nucleusArea));
                cout << fileName << endl;
                image->writeImage("exports/thumbnails/" + fileName, thumbnail);
                i++;

            }

        }


        image->writeJSON("exports/export.json", results);
    }
}