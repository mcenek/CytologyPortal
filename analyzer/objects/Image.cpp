#include "Image.h"
#include "Clump.h"

namespace segment {
    Image::Image(string path) {
        this->path = path;
        this->mat = readMatrix(path);
        this->padding = 30;

        int channels = this->mat.channels();
        int width = this->mat.cols;
        int height = this->mat.rows;


        printf("Image data: (rows) %i (cols) %i (channels) %i\n", height, width, channels);

    }

    cv::Mat Image::readMatrix(string path) {
        cv::Mat image = cv::imread(path);
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
            try {
                cout << "clump " << i << endl;
                for (int j = 0; j < clump->finalCellContours.size(); j++) {
                    if (clump->finalCellContours[j].empty()) {
                        continue;
                    }

                    cv::Scalar color = cv::Scalar(rng.uniform(0,255), rng.uniform(0, 255), rng.uniform(0, 255));
                    vector<vector<cv::Point>> contour = {clump->finalCellContours[j]};
                    cv::drawContours(img, contour, 0, color, 3);

                }

            } catch(int e) {}

        }
        cv::imshow("x", img);
        cv::waitKey(0);
        cv::imwrite("../images/cell_boundaries.png", img);



    }
}
