#include "ClumpSegmentation.h"
#include "opencv2/opencv.hpp"
#include "../VLFeatWrapper.cpp"
#include "DRLSE.h"
#include "SegmenterTools.h"

using namespace std;

namespace segment {
    cv::Mat drlse_denoise(cv::Mat phi, cv::Mat g, float lambda, float mu, float alpha, float epsilon,
                          float timestep) {
        int iter_in = 2;
        vector <cv::Mat> gGradient = calcGradient(g);
        cv::Mat vx = getGradientX(gGradient);
        cv::Mat vy = getGradientY(gGradient);
        for (int i = 0; i < iter_in; i++) {
            vector <cv::Mat> gradient = calcGradient(phi);
            vector <cv::Mat> curvatureXY = drlse::calcCurvatureXY(gradient);
            cv::Mat Nx = curvatureXY[0];
            cv::Mat Ny = curvatureXY[1];
            cv::Mat curvature = calcDivergence(Nx, Ny);
            cv::Mat regularizer = drlse::calcSignedDistanceReg(phi);
            cv::Mat dirac = drlse::calcDiracDelta(phi, epsilon);
            cv::Mat areaTerm = dirac.mul(g);
            cv::Mat edgeTerm = dirac.mul(vx.mul(Nx) + vy.mul(Ny)) +
                               dirac.mul(g).mul(curvature);
            phi += timestep * (mu * regularizer +
                               lambda * edgeTerm +
                               alpha * areaTerm);
        }
        return phi;
    }

    /*
         runQuickshift takes an image and params and runs Quickshift on it, using the VL_Feat implementation
         Returns:
         cv::Mat = image after quickshift is applied
         Params:
         cv::Mat mat = the image
         int kernelsize = the kernel or window size of the quickshift applied
         int maxdist = the largest distance a pixel can be from it's root
       */
    cv::Mat runQuickshift(cv::Mat *mat, int kernelsize, int maxdist, bool debug) {
        int channels = mat->channels();
        int width = mat->cols;
        int height = mat->rows;

        cv::Mat tempMat;
        mat->copyTo(tempMat);
        tempMat.convertTo(tempMat, CV_64FC3, 1 / 255.0);
        double *cvmat = (double *) tempMat.data;
        double *vlmat = (double *) calloc(channels * width * height, sizeof(double));

        // create VLFeatWrapper object
        VLFeatWrapper vlf_wrapper = VLFeatWrapper(width, height, channels);
        vlf_wrapper.debug = debug;
        vlf_wrapper.verifyVLFeat();

        // apply quickshift from VLFeat
        vlf_wrapper.convertOPENCV_VLFEAT(cvmat, vlmat);
        int superpixelcount = vlf_wrapper.quickshift(vlmat, kernelsize, maxdist);
        vlf_wrapper.convertVLFEAT_OPENCV(vlmat, cvmat);

        cv::Mat postQuickShift = cv::Mat(height, width, CV_64FC3, cvmat);
        cv::Mat outmat;
        postQuickShift.copyTo(outmat);
        outmat.convertTo(outmat, CV_8UC3, 255);
        free(vlmat);

        //if (debug) image->log("Super pixels found via quickshift: %i\n", superpixelcount);
        return outmat;
    }

    /*
      runCanny runs canny edge detection on an image, and dilates and erodes it to close holes
      Returns:
      cv::Mat = edges found post dilate/erode
      Params:
      cv::Mat mat = image to find edged in
      int threshold1 = first threshold for the hysteresis procedure.
      int threshold2 = second threshold for the hysteresis procedure.
    */
    cv::Mat runCanny(cv::Mat mat, int threshold1, int threshold2, bool erodeFlag) {
        cv::Mat postEdgeDetection;
        mat.copyTo(postEdgeDetection);
        cv::Mat blurred;
        cv::blur(mat, blurred, cv::Size(2, 2)); //TODO: Shouldn't this be a cv::GaussianBlur..?
        cv::Canny(blurred, postEdgeDetection, threshold1, threshold2);

        if (erodeFlag) {
            // TODO these values for dilate and erode possibly should be configurable
            cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
            kernel = cv::Mat();
            cv::dilate(postEdgeDetection, postEdgeDetection, kernel, cv::Point(-1, -1), 1);
            cv::erode(postEdgeDetection, postEdgeDetection, kernel, cv::Point(-1, -1), 1);
        }

        return postEdgeDetection;
    }

    vector<vector<cv::Point>> runFindConvexHulls(vector<vector<cv::Point>> contours) {
        vector<vector<cv::Point>> hulls(contours.size());
        for (unsigned int i = 0; i < contours.size(); i++)
            cv::convexHull(cv::Mat(contours[i]), hulls[i], false);
        return hulls;
    }

    /*
      runGmm creates 2 Gaussian Mixture Models, one for cell pixels and one for background pixels,
      then returns the result of the labels generated by these models
      Returns:
      cv::Mat = labels found per pixel
      Params:
      cv::Mat mat = image to process
      vector<vector<cv::Point>> hulls = convex hulls to provide initial labeling
      int maxGmmIterations = maximum number of iterations to allow the gmm to train
    */
    cv::Mat runGmm(cv::Mat *mat, vector<vector<cv::Point>> hulls, int maxGmmIterations) {
        cv::Mat grayscaleMat;
        mat->convertTo(grayscaleMat, CV_8UC3);
        cv::cvtColor(grayscaleMat, grayscaleMat, CV_BGR2GRAY);

        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2);
        clahe->apply(grayscaleMat, grayscaleMat);


        grayscaleMat.convertTo(grayscaleMat, CV_64FC1);
        grayscaleMat = grayscaleMat.reshape(0, grayscaleMat.rows * grayscaleMat.cols);

        //Foreground black, background white
        cv::Mat probCluster1 = cv::Mat::ones(mat->rows, mat->cols, CV_32F);
        cv::drawContours(probCluster1, hulls, -1, (0), -1);
        probCluster1 = probCluster1.reshape(0, mat->rows * mat->cols);

        //Foreground white, background black
        cv::Mat probCluster2;
        cv::bitwise_not(probCluster1, probCluster2);

        cv::Mat initialProbMat;
        cv::hconcat(probCluster1, probCluster2, initialProbMat);

        cv::Mat labels;
        cv::Ptr<cv::ml::EM> cell_gmm;
        cv::TermCriteria termCrit = cv::TermCriteria();
        termCrit.type = cv::TermCriteria::COUNT;
        termCrit.maxCount = maxGmmIterations;
        cell_gmm = cv::ml::EM::create();
        cell_gmm->setTermCriteria(termCrit);
        cell_gmm->setClustersNumber(2);
        cell_gmm->trainM(grayscaleMat, initialProbMat, cv::noArray(), labels, cv::noArray());

        labels = labels.reshape(0, mat->rows);

        cv::Mat outmat;
        labels.copyTo(outmat);
        outmat.convertTo(outmat, CV_8UC1, 255);

        outmat = runGmmCleanup(mat, outmat);

        return outmat;
    }

    cv::Mat runGmmCleanup(cv::Mat *mat, cv::Mat gmmPredictions) {
        float timestep = 5;
        float mu = 0.04;
        float epsilon = 1.5;

        cv::Mat edgeEnforcer = drlse::calcEdgeEnforcer(*mat);
        cv::Mat initialPhi;

        gmmPredictions.convertTo(initialPhi, CV_32FC1, 1.0/255.0);
        for (int i = 0; i < initialPhi.rows; i++) {
            float *row = initialPhi.ptr<float>(i);
            for (int j = 0; j < initialPhi.cols; j++) {
                float value = row[j];
                if (value != 0) row[j] = -2;
                else row[j] = 2;
            }
        }
        cv::Mat phi = initialPhi;
        int iter_out = 3;
        float alpha = -2.5;
        float lambda = 5;

        for (int i = 0; i < iter_out; i++) {
            phi = drlse_denoise(phi, edgeEnforcer, lambda, mu, alpha, epsilon, timestep);
        }
        alpha = 0;
        phi = drlse_denoise(phi, edgeEnforcer, lambda, mu, alpha, epsilon, timestep);

        cv::bitwise_not(phi, phi);

        phi.convertTo(phi, CV_8UC1, 255.0);

        return phi;
    }

    /*
    findFinalClumpBoundaries takes an image and a threshold and returns all the contours whose
    size is greater than the threshold
    Returns:
    vector<vector<cv::Point> > = the contours found
    Params:
    cv::Mat mat = the input image
    int minAreaThreshold = the minimum area, all contours smaller than this are discarded
    */
    vector<vector<cv::Point>> findFinalClumpBoundaries(cv::Mat mat, double minAreaThreshold) {

        //Crop gmm because some gmm outputs have a white border which interferes with find contours
        int padding = 2;
        cv::Rect cropRect(padding, padding, mat.cols - 2 * padding, mat.rows - 2 * padding);
        mat = mat(cropRect);

        vector<vector<cv::Point>> contours;
        cv::findContours(mat, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

        double maxArea = 0;
        double secArea = 0;
        vector<vector<cv::Point>> clumpBoundaries = vector<vector<cv::Point> >();


        for (unsigned int i = 0; i < contours.size(); i++) {
            vector<cv::Point> contour = contours[i];
            double area = cv::contourArea(contour);

            //printf("Clump %d\n", i);
            if (area > minAreaThreshold) {
                vector<vector<cv::Point>> candidateContours;
                candidateContours.push_back(contour);
                if (area > 4000000) {
                    printf("Clump %d's contour of area %f too big\n", i, area);
                    for (int erosion_size = 1; erosion_size < 10; erosion_size++) {
                        printf("Clump %d, eroding with size %d\n", i, erosion_size);
                        cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                                                    cv::Size(2 * erosion_size + 1, 2 * erosion_size + 1),
                                                                    cv::Point(erosion_size, erosion_size));
                        cv::Mat tmpMat = cv::Mat(mat.rows, mat.cols, CV_8UC1);
                        cv::drawContours(tmpMat, vector<vector<cv::Point>>{contour}, 0, 255, -1);
                        cv::erode(tmpMat, tmpMat, element);
                        vector<vector<cv::Point>> erodeContours;
                        cv::findContours(tmpMat, erodeContours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);

                        if (erodeContours.size() <= 1) continue;
                        candidateContours.clear();

                        bool contoursSmallEnough = true;

                        for (vector<cv::Point> &erodeContour : erodeContours) {
                            tmpMat.release();
                            tmpMat = cv::Mat(mat.rows, mat.cols, CV_8UC1);
                            cv::drawContours(tmpMat, vector<vector<cv::Point>>{erodeContour}, 0, 255, -1);
                            cv::dilate(tmpMat, tmpMat, element);
                            vector<vector<cv::Point>> dilateContours;
                            cv::findContours(tmpMat, dilateContours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
                            for (vector<cv::Point> &dilateContour : dilateContours) {
                                area = cv::contourArea(dilateContour);
                                if (area > 4000000) contoursSmallEnough = false;
                                candidateContours.push_back(dilateContour);
                            }

                        }
                        if (contoursSmallEnough) {
                            break;
                        }
                    }
                }


                for (vector<cv::Point> &candidateContour : candidateContours) {
                    area = cv::contourArea(candidateContour);
                    if (candidateContours.size() > 1) {
                        printf("Clump %d, eroded, new area: %f\n", i, area);
                    }
                    if (area > maxArea) {
                        secArea = maxArea;
                        maxArea = area;
                    }
                    //Undo the cropping
                    for(unsigned int j = 0; j < candidateContour.size(); j++) {
                        candidateContour[j] = cv::Point(candidateContour[j].x + padding, candidateContour[j].y + padding);
                    }
                    clumpBoundaries.push_back(candidateContour);
                }


            }
        }
        printf("MAX AREA: %f\nSEC MAX AREA: %f\n", maxArea, secArea);
        return clumpBoundaries;
    }

}