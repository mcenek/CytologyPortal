#include "Image.h"
#include "Clump.h"
#include "../functions/SegmenterTools.h"
#include "boost/filesystem.hpp"

using namespace std;

namespace segment {
    Image::Image(string path) {
        boost::filesystem::path tmp(path);
        this->path = tmp;
        this->padding = 1;
        this->mat = readMatrix();


        int channels = this->mat.channels();
        int width = this->mat.cols;
        int height = this->mat.rows;


        printf("Image data: (rows) %i (cols) %i (channels) %i\n", height, width, channels);

    }

    cv::Mat Image::readMatrix() {
        cv::Mat image = cv::imread(this->path.string());

        if (image.empty()) {
            cerr << "Could not read image at: " << path << endl;
        }

        return image;
    }

    void Image::createClumps(vector<vector<cv::Point>> clumpBoundaries) {
        this->clumps = vector<Clump>();
        for (unsigned int i = 0; i < clumpBoundaries.size(); i++) {
            Clump clump;
            clump.image = this;
            clump.contour = vector<cv::Point>(clumpBoundaries[i]);
            clump.computeBoundingRect(this->matPadded);
            clumps.push_back(clump);

            //TODO - Remove
            // char buffer[200];
            // sprintf(buffer, "../images/clumps/clump_%i.png", i);
            // clump.extract().convertTo(outimg, CV_8UC3);
            // cv::imwrite(buffer, outimg);
        }
    }

    void Image::showFinalResults() {
        cv::RNG rng(12345);
        cv::Mat img = this->mat.clone();
        for (int i = 0; i < this->clumps.size(); i++) {
            Clump *clump = &this->clumps[i];
            drawColoredContours(&img, &clump->finalCellContours, &rng);
        }
        cv::imwrite("../images/cell_boundaries.png", img);
        cv::imshow("Overlapping Cell Segmentation", img);
        cv::waitKey(0);
    }
}
