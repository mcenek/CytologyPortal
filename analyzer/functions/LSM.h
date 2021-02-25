#ifndef SEGMENTER_CPP_LSM_H
#define SEGMENTER_CPP_LSM_H

#include "../objects/Clump.h"

namespace segment {
        vector<cv::Mat> calcCurvatureXY(vector<cv::Mat> gradient);

        cv::Mat calcDiracDelta(cv::Mat mat, double sigma);

        //Returns a divergence of the given matrix
        cv::Mat calcDivergence(cv::Mat x, cv::Mat y);

        cv::Mat calcEdgeEnforcer(cv::Mat mat);

        //A unary LSF energy term, following the equation - κδε(φi)div(hCg∇φi|∇φi|)
        cv::Mat calcGeodesicTerm(cv::Mat dirac, vector<cv::Mat> gradient, cv::Mat edgeEnforcer, cv::Mat clumpPrior);

        std::vector<cv::Mat> calcGradient(cv::Mat image);

        //Returns the magnitude for the given matricies
        cv::Mat calcGradientMagnitude(cv::Mat img);

        cv::Mat calcHeaviside(cv::Mat mat);

        //Returns the magnitude for the given matricies
        cv::Mat calcMagnitude(cv::Mat x, cv::Mat y);

        //Applies the normalized derivative of the potential function ......
        cv::Mat calcPotential(cv::Mat gradientMagnitude);

        //A unary LSF energy term, following the equation - μdiv(dp(|∇φi|)∇φi)
        cv::Mat calcSignedDistanceReg(cv::Mat mat);

        cv::Mat conv2( const cv::Mat& input, const cv::Mat& kernel, const char* shape);

        cv::Mat del2(cv::Mat mat);

        //Returns the original matrix as a 1 channel CV_32F
        cv::Mat ensureMatrixType(cv::Mat matrix);

        vector<vector<cv::Point>> getContours(cv::Mat mat);


        cv::Mat getGradientX(vector<cv::Mat> gradient);

        cv::Mat getGradientY(vector<cv::Mat> gradient);

        bool hasOverlap(cv::Mat mat1, cv::Mat mat2);

        cv::Mat neumannBoundCond(cv::Mat mat);

}

#endif //SEGMENTER_CPP_LSM_H
