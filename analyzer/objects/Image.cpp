#include "Image.h"
#include "../functions/SegmenterTools.h"

using namespace std;
using json = nlohmann::json;

namespace segment {
    Image::Image(string path) {
        boost::filesystem::path tmp(path);
        this->path = tmp;
        this->padding = 1;
        this->mat = readImage();

        this->totalNuclei = 0;
        int channels = this->mat.channels();
        int width = this->mat.cols;
        int height = this->mat.rows;

        clearLog();
        log("Loaded %s\n", path.c_str());
        log("Image data: (rows) %i (cols) %i (channels) %i\n", height, width, channels);

    }

    cv::Mat Image::readImage() {
        cv::Mat image = cv::imread(this->path.string());

        if (image.empty()) {
            cerr << "Could not read image at: " << path << endl;
        }

        return image;
    }

    boost::filesystem::path Image::getWriteDirectory() {
        boost::filesystem::path path("../images");
        path /= this->path.filename();
        boost::filesystem::create_directories(path);
        return path;
    }

    boost::filesystem::path Image::getWritePath(string name, string defaultExt) {
        boost::filesystem::path writeDirectory = getWriteDirectory();
        boost::filesystem::path writePath(writeDirectory / name);
        if (!writePath.has_extension()) {
            writePath += defaultExt;
        }
        return writePath;
    }

    void Image::writeImage(string name, cv::Mat mat) {
        boost::filesystem::path writePath = getWritePath(name, ".png");
        boost::filesystem::create_directories(writePath.parent_path());
        cv::imwrite(writePath.string(), mat);
    }

    cv::Mat Image::loadMatrix(string name) {
        boost::filesystem::path loadPath = getWritePath(name, ".yml");
        cv::Mat mat;

        if (is_regular_file(loadPath)) {
            cv::FileStorage fs(loadPath.string(), cv::FileStorage::READ);
            fs["mat"] >> mat;
            fs.release();
            cout << "Loaded from file: " << loadPath.string() << endl;
        }
        return mat;
    }

    void Image::writeMatrix(string name, cv::Mat mat) {
        boost::filesystem::path writePath = getWritePath(name, ".yml");
        boost::filesystem::create_directories(writePath.parent_path());
        cv::FileStorage fs(writePath.string(), cv::FileStorage::WRITE);
        fs << "mat" << mat;
        fs.release();
    }

    json Image::loadJSON(string name) {
        boost::filesystem::path writePath = getWritePath(name, ".json");
        ifstream ifs(writePath.string());
        json j;
        try {
            ifs >> j;
        } catch(json::parse_error& ex) {

        }
        return j;
    }


    void Image::writeJSON(string name, json &j) {
        boost::filesystem::path writePath = getWritePath(name, ".json");
        boost::filesystem::create_directories(writePath.parent_path());
        ofstream ofs(writePath.string());
        ofs << /*setw(4) <<*/ j << endl;
    }

    boost::filesystem::path Image::getLogPath() {
        return getWritePath("log", ".txt");
    }

    void Image::log(const char * format, ...) {
        va_list args;
        va_start(args, format);
        auto ret = vprintf(format, args);
        va_end(args);

        va_start(args, format);
        boost::filesystem::path logPath = getLogPath();
        FILE *file = fopen(logPath.string().c_str(), "a");
        vfprintf(file, format, args);
        fclose(file);
        va_end(args);
    }

    void Image::clearLog() {
        boost::filesystem::path logPath = getLogPath();
        FILE *file = fopen(logPath.string().c_str(), "w");
        fclose(file);
    }

    void Image::createClumps(vector<vector<cv::Point>> clumpBoundaries) {
        this->clumps = vector<Clump>();
        for (unsigned int i = 0; i < clumpBoundaries.size(); i++) {
            Clump clump;
            clump.image = this;
            clump.contour = vector<cv::Point>(clumpBoundaries[i]);
            clump.computeBoundingRect(this->matPadded);
            clump.computeOffsetContour();
            clumps.push_back(clump);

            //TODO - Remove
            // char buffer[200];
            // simage->log(buffer, "../images/clumps/clump_%i.png", i);
            // clump.extract().convertTo(outimg, CV_8UC3);
            // cv::imwrite(buffer, outimg);
        }
    }

    cv::Mat Image::getNucleiBoundaries() {
        cv::Mat img = this->mat.clone();
        for (int i = 0; i < this->clumps.size(); i++) {
            Clump *clump = &this->clumps[i];
            cv::drawContours(img, clump->undoOffsetContour(), -1, 0, 1);
        }
        return img;
    }

    cv::Mat Image::getFinalResult() {
        cv::RNG rng(12345);
        cv::Mat img = this->mat.clone();
        for (int clumpIdx = 0; clumpIdx < this->clumps.size(); clumpIdx++) {
            Clump *clump = &this->clumps[clumpIdx];
            vector<vector<cv::Point>> finalContours = clump->getFinalCellContours();
            drawColoredContours(&img, &finalContours, &rng);
        }
        return img;
    }
}
