#ifndef LSM_H
#define LSM_H

#include "../objects/Clump.h"

namespace segment {
    namespace drlse {

        void updatePhi(Cell *cellI, Clump *clump, double dt, double epsilon, double mu, double kappa, double chi);

        cv::Mat calcAllBinaryEnergy(Cell *cellI, cv::Mat edgeEnforcer, cv::Mat clumpPrior, cv::Mat phiI, cv::Mat dirac);

        cv::Mat calcBinaryEnergy(cv::Mat mat, cv::Mat edgeEnforcer, cv::Mat clumpPrior, cv::Mat dirac);

        vector<cv::Mat> calcCurvatureXY(vector<cv::Mat> gradient);

        cv::Mat calcEdgeEnforcer(cv::Mat mat);

        //A unary LSF energy term, following the equation - κδε(φi)div(hCg∇φi|∇φi|)
        cv::Mat calcGeodesicTerm(cv::Mat dirac, vector<cv::Mat> gradient, cv::Mat edgeEnforcer, cv::Mat clumpPrior);

        cv::Mat calcDiracDelta(cv::Mat mat, double sigma);

        cv::Mat calcHeavisideInv(cv::Mat mat);

        //A unary LSF energy term, following the equation - μdiv(dp(|∇φi|)∇φi)
        cv::Mat calcSignedDistanceReg(cv::Mat mat, vector<cv::Mat> gradient);

        cv::Mat conv2( const cv::Mat& input, const cv::Mat& kernel, const char* shape);

        cv::Mat del2(cv::Mat mat);

        //Returns the original matrix as a 1 channel CV_32F
        cv::Mat ensureMatrixType(cv::Mat matrix);

        vector<vector<cv::Point>> getContours(cv::Mat mat);

        cv::Mat neumannBoundCond(cv::Mat mat);
    }
}

#endif //LSM_H
