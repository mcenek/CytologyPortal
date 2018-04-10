#include <iostream>
#include <ctime>
#include <stdio.h>
#include "opencv2/opencv.hpp"
#include "VLFeatWrapper.cpp"
extern "C" {
    #include "vl/quickshift.h"
}

using namespace std;

namespace segment
{
    class Segmenter
    {
        public:

        void runSegmentation(int kernelsize, int maxdist) {
            clock_t total = clock();
            clock_t start;
            double end;

            cv::Mat image = cv::imread("../images/cyto.tif");
            cv::imshow("pre", image);

            // meta-data about the image
            int channels = image.channels();
            int width = image.cols;
            int height = image.rows;

            // debug data
            printf("Image data: (rows) %i (cols) %i (channels) %i\n", height, width, channels);


            // quickshift
            start = clock();
            printf("Beginning quickshift...\n");
            // convert to a format that VL_Feat can use
            cv::Mat tempMat;
            image.convertTo(tempMat, CV_64FC3, 1/255.0);
            double* cvimg = (double*) tempMat.data;
            double* vlimg = (double*) calloc(channels*width*height, sizeof(double));

            // create VLFeatWrapper object
            segment::VLFeatWrapper vlf_wrapper = segment::VLFeatWrapper(width, height, channels);
            vlf_wrapper.verifyVLFeat();

            // apply quickshift from VLFeat
            vlf_wrapper.convertOPENCV_VLFEAT(cvimg, vlimg);
            int superpixelcount = vlf_wrapper.quickshift(vlimg, kernelsize, maxdist);
            vlf_wrapper.convertVLFEAT_OPENCV(vlimg, cvimg);

            cv::Mat postQuickShift = cv::Mat(height, width, CV_64FC3, cvimg);
            cv::imshow("quickshifted", postQuickShift);
            cv::Mat outimg;
            postQuickShift.convertTo(outimg, CV_8U, 255);
            imwrite("../images/quickshifted_cyto.png", outimg);

            end = (clock() - start) / CLOCKS_PER_SEC;
            printf("Quickshift complete, super pixels found: %i, time:%f\n", superpixelcount, end);


            // apply Canny Edge Detection
            start = clock();
            printf("Beginning Edge Detection...\n");
            cv::Mat postEdgeDetection;
            postEdgeDetection.create(postQuickShift.size(), postQuickShift.type());
            postEdgeDetection = cv::Scalar::all(0);
            cv::Mat blurred;
            cv::blur(postQuickShift, blurred, cv::Size(3,3));
            blurred.convertTo(blurred, CV_8UC3, 255);
            cv::Canny(blurred, postEdgeDetection, 20, 40);

            cv::imshow("edges", postEdgeDetection);
            postEdgeDetection.convertTo(outimg, CV_8U, 255);
            cv::imwrite("../images/edgeDetected_cyto.png", outimg);

            cv::dilate(postEdgeDetection, postEdgeDetection, cv::Mat(), cv::Point(-1, -1), 2);
            cv::erode(postEdgeDetection, postEdgeDetection, cv::Mat(), cv::Point(-1, -1), 2);

            cv::imshow("post dilate/erode", postEdgeDetection);
            postEdgeDetection.convertTo(outimg, CV_8U, 255);
            cv::imwrite("../images/edgeDetectedEroded_cyto.png", outimg);

            end = (clock() - start) / CLOCKS_PER_SEC;
            printf("Edge Detection complete, time: %f\n", end);

            // format conversion
            postEdgeDetection.convertTo(postEdgeDetection, CV_8UC3, 255);

            start = clock();
            printf("Beginning CCA and building convex hulls...\n");

            // find contours
            vector<vector<cv::Point> > contours;
            cv::findContours(postEdgeDetection, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

            image.convertTo(outimg, CV_8UC3);
            cv::drawContours(outimg, contours, -1, cv::Scalar(255, 0, 255), 1.5);
            cv::imshow("contours", outimg);
            cv::imwrite("../images/contours_cyto.png", outimg);

            // find convex hulls
            vector<vector<cv::Point> > hulls(contours.size());
            for(unsigned int i=0; i<contours.size(); i++)
                cv::convexHull(cv::Mat(contours[i]), hulls[i], false);

            image.convertTo(outimg, CV_8UC3);
            cv::drawContours(outimg, hulls, -1, cv::Scalar(255, 0, 255), 1);
            cv::imshow("hulls", outimg);
            cv::imwrite("../images/hulls_cyto.png", outimg);

            end = (clock() - start) / CLOCKS_PER_SEC;
            printf("Finished with CCA and convex hulls, time: %f\n", end);


            // Gaussian Mixture Modeling
            start = clock();
            printf("Beginning Gaussian Mixture Modeling...\n");
            cv::Mat gray;
            image.convertTo(gray, CV_8U);
            cv::cvtColor(gray, gray, CV_BGR2GRAY);
            gray.convertTo(gray, CV_64F, 1/255.0);

            // create initial probabilities based on convex hulls
            float initialProbs[width*height][2];
            for(int row=0; row < height; row++)
            {
                for(int col=0; col < width; col++)
                {
                    for(unsigned int hullIndex=0; hullIndex < hulls.size(); hullIndex++)
                    {
                        if(cv::pointPolygonTest(hulls[hullIndex], cv::Point2f(row, col), false) >= 0)
                        {
                            initialProbs[row + col*height][0] = 1;
                            initialProbs[row + col*height][1] = 0;
                        }
                        else
                        {
                            initialProbs[row + col*height][0] = 0;
                            initialProbs[row + col*height][1] = 1;
                        }
                    }
                }
            }
            gray = gray.reshape(0, gray.rows*gray.cols);
            cv::Mat initialProbMat(width*height, 2, CV_32F, initialProbs);
            cv::Mat outputProbs;
            cv::Mat labels;
            cv::Ptr<cv::ml::EM> cell_gmm;
            printf("Found initial labels\n");

            // current performing 1 epochs -- feeding the output probabilities
            // in as the initial probs
            int epochs = 1;
            for(int i=0; i<epochs; i++)
            {
                cell_gmm = cv::ml::EM::create();
                cell_gmm->setClustersNumber(2);
                cell_gmm->trainM(gray, initialProbMat, cv::noArray(), labels, outputProbs);
                initialProbMat = cv::Mat(outputProbs);
                printf("Finished GMM epoch: %i\n", i);
            }

            labels = labels.reshape(0, image.rows);
            labels.convertTo(outimg, CV_8U, 255);
            cv::imshow("GMM", outimg);
            cv::imwrite("../images/raw_gmm.png", outimg);

            end = (clock() - start) / CLOCKS_PER_SEC;
            printf("Finished with Gaussian Mixture Modeling, time:%f\n", end);


            // GMM Post processing
            start = clock();
            printf("Beginning GMM Output post processing...\n");

            // magic numbers
            double AREA_THRESHOLD = 50.0;

            // opencv wants to find white object on a black background,
            // so we want to invert the labels before findContours
            labels.convertTo(labels, CV_8U, 255);
            for(int row=0; row<labels.rows; row++)
            {
                for(int col=0; col<labels.cols; col++)
                {
                    unsigned char *pixel = &labels.at<uchar>(row, col);
                    *pixel == 0 ? *pixel = 1 : *pixel = 0;
                }
            }
            // vector<vector<cv::Point> > contours;
            cv::findContours(labels, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
            vector<vector<cv::Point> > clumpBoundaries = vector<vector<cv::Point> >();
            for(unsigned int i=0; i<contours.size(); i++)
            {
                vector<cv::Point> contour = contours[i];
                double area = cv::contourArea(contour);
                if(area > AREA_THRESHOLD)
                {
                    clumpBoundaries.push_back(contour);
                }
            }
            unsigned int numClumps = clumpBoundaries.size();
            image.convertTo(outimg, CV_8UC3);
            cv::drawContours(outimg, clumpBoundaries, -1, cv::Scalar(255, 0, 255), 1.5);
            cv::imshow("clumpBoundaries", outimg);
            cv::imwrite("../images/clumpBoundaries.png", outimg);
            vector<cv::Rect> boundingRects = vector<cv::Rect>();
            for(unsigned int i=0; i<clumpBoundaries.size(); i++)
            {
                cv::Mat mask = cv::Mat::zeros(image.rows, image.cols, CV_8U);
                cv::drawContours(mask, clumpBoundaries, i, cv::Scalar(255), CV_FILLED);
                cv::Mat fullMasked = cv::Mat(image.rows, image.cols, CV_8U);
                fullMasked.setTo(cv::Scalar(255, 0, 255));
                image.copyTo(fullMasked, mask);
                cv::Rect rect = cv::boundingRect(clumpBoundaries[i]);
                boundingRects.push_back(rect);
                char buffer[200];
                sprintf(buffer, "clump_%i", i);
                cv::Mat clump = cv::Mat(fullMasked, rect);
                clump.convertTo(outimg, CV_8UC3);
                cv::imshow(buffer, outimg);
                char temp[] = "../images/clumps/";
                strcat(temp, buffer);
                strcat(temp, ".png");
                cv::imwrite(temp, outimg);
            }

            end = (clock() - start) / CLOCKS_PER_SEC;
            printf("Finished post processing, clumps found:%i, time:%f\n", numClumps, end);


            // clean up
            end = (clock() - total) / CLOCKS_PER_SEC;
            printf("Segmentation finished, total time:%f\n", end);
            free(vlimg);
            printf("^C to exit\n");
            cv::waitKey(0);
        }
    };
}
