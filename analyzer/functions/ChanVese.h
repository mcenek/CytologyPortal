#ifndef CHANVESE_H
#define CHANVESE_H

#include "../objects/Clump.h"

namespace segment {
    namespace chanVese {
        void updatePhi(Cell *cellI, Clump *clump, double dt, double miu, double v, double lambda1, double lambda2, double epsilon);

        cv::Mat calcHeaviside(cv::Mat mat, float epsilon);

        double getAverage(cv::Mat mat, cv::Mat phi, float epsilon);

        cv::Mat calcDiracDelta(cv::Mat phi, double epsilon);

        cv::Mat calcTotalVariation(cv::Mat phi);
    }
}


#endif //CHANVESE_H
