#include "ChanVese.h"

#include "../objects/Clump.h"
#include "../functions/SegmenterTools.h"

namespace segment {
    namespace chanVese {
        void updatePhi(Cell *cellI, Clump *clump, double dt, double miu, double v, double lambda1, double lambda2, double epsilon) {
            cv::Mat dirac = calcDiracDelta(cellI->phi, epsilon);

            cv::Mat totalVariation = calcTotalVariation(cellI->phi);


            cv::Mat image = clump->extract();
            cv::cvtColor(image, image, CV_RGB2GRAY);
            image.convertTo(image, CV_32FC1, 1/255.0);

            cv::Mat mat = image;

            double c1 = getAverage(mat, cellI->phi, epsilon);
            double c2 = getAverage(mat, 1 - cellI->phi, epsilon);

            cv::Mat avgMat1 = (mat - c1);
            cv::pow(avgMat1, 2, avgMat1);

            cv::Mat avgMat2 = (mat - c2);
            cv::pow(avgMat2, 2, avgMat2);

            cellI->phi -= dt * dirac.mul(miu * totalVariation - v - lambda1 * avgMat1 + lambda2 * avgMat2);

        }

        cv::Mat calcHeaviside(cv::Mat mat, float epsilon) {
            cv::Mat heaviside = mat.clone();
            for (int i = 0; i < heaviside.rows; i++) {
                float *row = heaviside.ptr<float>(i);
                for (int j = 0; j < heaviside.cols; j++) {
                    float value = row[j];
                    row[j] = (1.0 / 2.0) * (1 + (2/M_PI) * atan(value / epsilon));
                }
            }
            return heaviside;
        }

        double getAverage(cv::Mat mat, cv::Mat phi, float epsilon) {
            cv::Mat heaviside = calcHeaviside(phi, epsilon);
            cv::Mat insidePhi = mat.mul(heaviside);
            double sum = cv::sum(insidePhi)[0];
            double total = cv::sum(heaviside)[0];
            double average = sum / (0.01 + total);
            return average;
        }

        cv::Mat calcDiracDelta(cv::Mat phi, double epsilon) {
            cv::Mat phiSq;
            cv::pow(phi, 2, phiSq);
            cv::Mat dirac = epsilon / (M_PI * (pow(epsilon, 2) + phiSq));
            return dirac;
        }

        cv::Mat calcTotalVariation(cv::Mat phi) {
            vector<cv::Mat> gradient = calcGradient(phi);
            cv::Mat gradientX = getGradientX(gradient);
            cv::Mat gradientY = getGradientY(gradient);
            cv::Mat divergence = calcDivergence(gradientX, gradientY);
            cv::Mat gradientMagnitude = calcMagnitude(gradientX, gradientY) + 0.01;

            cv::Mat totalVariation;
            cv::divide(divergence, gradientMagnitude, totalVariation);
            return totalVariation;
        }

    }
}