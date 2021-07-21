#ifndef OVERLAPPINGCELLSEGMENTATION_H
#define OVERLAPPINGCELLSEGMENTATION_H

#include "../objects/Clump.h"

namespace segment {
    /*
       Overlapping Segmentation (Distance Map)
       https://cs.adelaide.edu.au/~zhi/publications/paper_TIP_Jan04_2015_Finalised_two_columns.pdf
       http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.231.9150&rep=rep1&type=pdf
     */


    bool isConverged(Cell *cellI);

    bool clumpHasSingleCell(Clump *clump);

    void startOverlappingCellSegmentationThread(Image *image, Clump *clump, int clumpIdx,
                                                double dt, double epsilon, double mu, double kappa, double chi);

    void runOverlappingSegmentation(Image *image, double dt, double epsilon, double mu, double kappa, double chi);

    void updatePhi(Cell *cellI, Clump *clump, double dt, double epsilon, double mu, double kappa, double chi);

}


#endif //OVERLAPPINGCELLSEGMENTATION_H
