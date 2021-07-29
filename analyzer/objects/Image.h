#ifndef IMAGE_H
#define IMAGE_H

#include "opencv2/opencv.hpp"
#include "Clump.h"
#include "boost/filesystem.hpp"
#include "../thirdparty/nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

namespace segment {
    class Clump; //forward declaration
    class Image {
    public:
        cv::Mat mat;

        cv::Mat matPadded;
        int padding;

        boost::filesystem::path path;
        vector<Clump> clumps;

        Image(string path);
        Image(cv::Mat mat);

        cv::Mat padMatrix();
        cv::Mat readImage();
        boost::filesystem::path getWriteDirectory();
        boost::filesystem::path getWritePath(string name, string defaultExt);
        void writeImage(string name, cv::Mat mat);
        cv::Mat loadMatrix(string name);
        void writeMatrix(string name, cv::Mat mat);
        json loadJSON(string name);
        void writeJSON(string name, json &j);
        boost::filesystem::path getLogPath();
        void log(const char * format, ...);
        void clearLog();
        void createClumps(vector<vector<cv::Point>> clumpBoundaries);
        cv::Mat getNucleiBoundaries();
        cv::Mat getFinalResult();
        
    };
}

#endif //IMAGE_H
