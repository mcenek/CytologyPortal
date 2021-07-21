#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"
#include "../functions/SegmenterTools.h"
#include "DRLSE.h"
#include <ctime>
using namespace std;

namespace segment {
    namespace drlse {

        /*
         * updatePhi evolves the level set front, phi, using the modified DRLSE algorithm
         */
        void updatePhi(Cell *cellI, Clump *clump, double dt, double epsilon, double mu, double kappa, double chi) {
            //getPhi returns phi that is cropped to a bounding box of phi and its neighbors + padding
            cv::Mat phi = cellI->getPhi();

            //Get the bounding box of phi and its neighbors
            cv::Rect boundingBoxWithNeighbors = cellI->boundingBoxWithNeighbors;
            //Fix dimensions to account for extra padding (from the getPhi function)
            boundingBoxWithNeighbors.width = phi.cols;
            boundingBoxWithNeighbors.height = phi.rows;

            //Crop edge enforcer to a bounding box of phi and its neighbors + padding
            cv::Mat edgeEnforcer = clump->edgeEnforcer.clone();
            edgeEnforcer = edgeEnforcer(boundingBoxWithNeighbors);

            //Crop clump prior to a bounding box of phi and its neighbors + padding
            cv::Mat clumpPrior = clump->clumpPrior.clone();
            clumpPrior = clumpPrior(boundingBoxWithNeighbors);

            //Perform the modified DRLSE algorithm
            vector <cv::Mat> gradient = calcGradient(phi);
            cv::Mat regularizer = calcSignedDistanceReg(phi, gradient);
            cv::Mat dirac = calcDiracDelta(phi, epsilon);
            cv::Mat gac = calcGeodesicTerm(dirac, gradient, edgeEnforcer, clumpPrior);
            cv::Mat binaryEnergy = calcAllBinaryEnergy(cellI, edgeEnforcer, clumpPrior, phi, dirac);

            phi += dt * (
                    mu * regularizer +
                    kappa * gac +
                    chi * binaryEnergy
            );

            cellI->setPhi(phi);
        }

        /*
         * calcAllBinaryEnergy finds the binary energy with all of a cell's neighbors
         */
        cv::Mat calcAllBinaryEnergy(Cell *cellI, cv::Mat edgeEnforcer, cv::Mat clumpPrior, cv::Mat phiI, cv::Mat dirac) {
            cv::Mat binaryEnergy = cv::Mat::zeros(phiI.rows, phiI.cols, CV_32FC1);
            for (unsigned int cellIdxJ = 0; cellIdxJ < cellI->neighbors.size(); cellIdxJ++) {
                Cell *cellJ = cellI->neighbors[cellIdxJ];

                if (cellI == cellJ) {
                    continue;
                }

                //Get the neighbor cellJ's phi with the same bounding box as cellI's phi
                //This is possible since phiI's bounding box is of phiI and its neighbors
                cv::Rect boundingBoxWithNeighbors = cellI->boundingBoxWithNeighbors;
                cv::Mat phiJ = cellJ->getPhi(boundingBoxWithNeighbors);
                binaryEnergy += calcBinaryEnergy(phiJ, edgeEnforcer, clumpPrior, dirac);
            }
            return binaryEnergy;
        }

        /*
         * calcBinaryEnergy finds the binary energy between two phis
         */
        cv::Mat calcBinaryEnergy(cv::Mat mat, cv::Mat edgeEnforcer, cv::Mat clumpPrior, cv::Mat dirac) {
            cv::Mat overAreaTerm;
            cv::Mat heaviside = calcHeavisideInv(mat);
            overAreaTerm = clumpPrior.mul(edgeEnforcer);
            overAreaTerm = overAreaTerm.mul(dirac);
            overAreaTerm = overAreaTerm.mul(heaviside);
            return overAreaTerm;
        }

        /*
         * calcCurvatureXY finds the X and Y curvature components of a gradient.
         */
        vector <cv::Mat> calcCurvatureXY(vector <cv::Mat> gradient) {
            //Small number is used to avoid division by 0
            double smallNumber = 1e-10;
            cv::Mat gradientX = getGradientX(gradient);
            cv::Mat gradientY = getGradientY(gradient);
            cv::Mat gradientMagnitude = calcMagnitude(gradientX, gradientY);
            cv::Mat Nx, Ny;
            cv::divide(gradientX, gradientMagnitude + smallNumber, Nx);
            cv::divide(gradientY, gradientMagnitude + smallNumber, Ny);
            return {Nx, Ny};
        }

        /*
         * calcDiracDelta finds the dirac delta of a matrix with the specified sigma
         */
        cv::Mat calcDiracDelta(cv::Mat mat, double sigma) {
            cv::Mat diracDelta = cv::Mat(mat.rows, mat.cols, CV_32FC1);
            for (int i = 0; i < mat.rows; i++) {
                float *row = mat.ptr<float>(i);
                float *diracDeltaRow = diracDelta.ptr<float>(i);
                for (int j = 0; j < mat.cols; j++) {
                    float value = row[j];
                    if (value <= sigma && value >= -sigma) {
                        diracDeltaRow[j] = (1.0 / 2.0 / sigma) * (1 + cos(M_PI * value / sigma));
                    } else {
                        diracDeltaRow[j] = 0;
                    }
                }
            }
            return diracDelta;
        }


        /*
         * calcEdgeEnforcer finds the edge enforcer of a matrix
         * g = 1/(1+f) where f is the magnitude of the gradient
         * of the gaussian convolution of the matrix
         */
        cv::Mat calcEdgeEnforcer(cv::Mat mat) {
            mat = mat.clone();
            cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);
            mat.convertTo(mat, CV_32FC1);

            //Find the gaussian kernel of kernel size 15 and sigma 1.5
            cv::Mat gaussianKernel = cv::getGaussianKernel(15, 1.5, CV_32FC1);
            //Multiply the gaussianKernel with its transpose
            cv::mulTransposed(gaussianKernel, gaussianKernel, false);
            //Smooth the gaussian kernel with gradient convolution
            cv::Mat gaussian = conv2(mat, gaussianKernel, "same");
            //Find the gradient of the gaussian
            vector<cv::Mat> gaussianGradient = calcGradient(gaussian);
            cv::Mat gaussianGradientX = getGradientX(gaussianGradient);
            cv::Mat gaussianGradientY = getGradientY(gaussianGradient);
            //f=Ix.^2+Iy.^2
            cv::Mat gaussianGradientXSq, gaussianGradientYSq;
            cv::multiply(gaussianGradientX, gaussianGradientX, gaussianGradientXSq);
            cv::multiply(gaussianGradientY, gaussianGradientY, gaussianGradientYSq);
            cv::Mat gaussianGradientSq = gaussianGradientXSq + gaussianGradientYSq;
            //g=1./(1+f)
            cv::Mat edgeEnforcer = 1 / (1 + gaussianGradientSq);
            return edgeEnforcer;
        }

        /*
         * A unary LSF energy term, following the equation - κδε(φi)div(hCg∇φi|∇φi|)
         */
        cv::Mat calcGeodesicTerm(cv::Mat dirac, vector <cv::Mat> gradient, cv::Mat edgeEnforcer, cv::Mat clumpPrior) {
            cv::Mat distPair = edgeEnforcer.mul(clumpPrior);
            vector<cv::Mat> distPairGradient = calcGradient(distPair);

            cv::Mat vxd = getGradientX(distPairGradient);
            cv::Mat vyd = getGradientY(distPairGradient);

            vector <cv::Mat> Nxy = calcCurvatureXY(gradient);
            cv::Mat Nx = Nxy[0];
            cv::Mat Ny = Nxy[1];
            cv::Mat curvature = calcDivergence(Nx, Ny);

            return dirac.mul(vxd.mul(Nx) + vyd.mul(Ny)) + dirac.mul(clumpPrior).mul(edgeEnforcer).mul(curvature);
        }

        /*
         * calcHeavisideInv finds the inverse of the heaviside of the specified matrix
         * For each pixel:
         *   Set to 0 when pixel > 0
         *   Set to 1 otherwise
         */
        cv::Mat calcHeavisideInv(cv::Mat mat) {
            cv::Mat heavisideInv = cv::Mat(mat.rows, mat.cols, CV_32FC1);
            for (int i = 0; i < heavisideInv.rows; i++) {
                float *row = mat.ptr<float>(i);
                float *heavisideInvRow = heavisideInv.ptr<float>(i);
                for (int j = 0; j < heavisideInv.cols; j++) {
                    float value = row[j];
                    if (value > 0) heavisideInvRow[j] = 0;
                    else heavisideInvRow[j] = 1;
                }
            }
            return heavisideInv;
        }

        /*
         * calcSignedDistanceReg is a unary LSF energy term, following the equation - μdiv(dp(|∇φi|)∇φi)
         */
        cv::Mat calcSignedDistanceReg(cv::Mat mat, vector<cv::Mat> gradient) {
            cv::Mat gradientX = getGradientX(gradient);
            cv::Mat gradientY = getGradientY(gradient);
            cv::Mat gradientMagnitude = calcMagnitude(gradientX, gradientY);

            cv::Mat potential = cv::Mat(mat.rows, mat.cols, CV_32FC1);
            cv::Mat divX = cv::Mat(mat.rows, mat.cols, CV_32FC1);
            cv::Mat divY = cv::Mat(mat.rows, mat.cols, CV_32FC1);

            for (int i = 0; i < potential.rows; i++) {
                float *gradientXRow = gradientX.ptr<float>(i);
                float *gradientYRow = gradientY.ptr<float>(i);
                float *gradMagRow = gradientMagnitude.ptr<float>(i);
                float *potentialRow = potential.ptr<float>(i);
                float *divXRow = divX.ptr<float>(i);
                float *divYRow = divY.ptr<float>(i);
                for (int j = 0; j < potential.cols; j++) {
                    float gradientMag = gradMagRow[j];
                    if (gradientMag >= 0 && gradientMag <= 1) {
                        potentialRow[j] = sin(2 * M_PI * gradientMag) / (2 * M_PI);
                    } else if (gradientMag > 1) {
                        potentialRow[j] = gradientMag - 1;
                    } else {
                        potentialRow[j] = 0;
                    }
                    float potential = potentialRow[j];
                    float dps = ((potential != 0) ? potential : 1) / ((gradientMag != 0) ? gradientMag : 1);
                    divXRow[j] = dps * gradientXRow[j] - gradientXRow[j];
                    divYRow[j] = dps * gradientYRow[j] - gradientYRow[j];
                }
            }
            return calcDivergence(divX, divY) + 4 * del2(mat);
        }

        /*
         * conv2 implements the MATLAB conv2 which finds the convolution of a matrix
         */
        cv::Mat conv2(const cv::Mat &input, const cv::Mat &kernel, const char *shape) {
            cv::Mat flipped_kernel;
            cv::flip(kernel, flipped_kernel, -1);
            cv::Point2i pad;
            cv::Mat result, padded;
            padded = input;
            pad = cv::Point2i(0, 0);
            if (strcmp(shape, "same") == 0) {
                padded = input;
                pad = cv::Point2i(0, 0);
            } else if (strcmp(shape, "valid") == 0) {
                padded = input;
                pad = cv::Point2i(kernel.cols - 1, kernel.rows - 1);
            } else if (strcmp(shape, "full") == 0) {
                pad = cv::Point2i(kernel.cols - 1, kernel.rows - 1);
                cv::copyMakeBorder(input, padded, pad.y, pad.y, pad.x, pad.x, cv::BORDER_CONSTANT);
            } else {
                throw runtime_error("Unsupported convolutional shape");
            }
            cv::Rect region = cv::Rect(pad.x / 2, pad.y / 2, padded.cols - pad.x, padded.rows - pad.y);
            cv::filter2D(padded, result, -1, flipped_kernel, cv::Point(-1, -1), 0, cv::BORDER_CONSTANT);
            return result(region);
        }

        /*
         * del2 implements the MATLAB del2 function which finds
         * the discrete approximation of the  laplacian of a matrix
         */
        cv::Mat del2(cv::Mat mat) {
            int rows = mat.rows;
            int cols = mat.cols;

            cv::Mat laplacian = cv::Mat(rows, cols, CV_32FC1);

            // Interior
            for (int x = 1; x < rows - 1; x++) {
                for (int y = 1; y < cols - 1; y++) {
                    if (x > 0 && y > 0 && x < rows - 1 && y < cols - 1) {
                        laplacian.at<float>(x, y) = ((mat.at<float>(x - 1, y) + mat.at<float>(x + 1, y) +
                                                      mat.at<float>(x, y - 1) + mat.at<float>(x, y + 1)) -
                                                     4 * mat.at<float>(x, y)) / 4;
                    }
                }
            }

            // Sides
            for (int x = 0; x < rows; x++) {
                for (int y = 0; y < cols; y++) {
                    if (x <= 0 || y <= 0 || x >= rows - 1 || y >= cols - 1) {
                        if (x == 0 && y == 0) {
                        } else if (y == 0 && x < rows - 1) {
                            laplacian.at<float>(x, y) = (-5 * mat.at<float>(x, y + 1) + 4 * mat.at<float>(x, y + 2) -
                                                         mat.at<float>(x, y + 3) + 2 * mat.at<float>(x, y) +
                                                         mat.at<float>(x + 1, y) +
                                                         mat.at<float>(x - 1, y) - 2 * mat.at<float>(x, y)) / 4;
                        } else if (x == 0 && y < cols - 1) {
                            laplacian.at<float>(x, y) = (-5 * mat.at<float>(x + 1, y) + 4 * mat.at<float>(x + 2, y) -
                                                         mat.at<float>(x + 3, y) + 2 * mat.at<float>(x, y) +
                                                         mat.at<float>(x, y + 1) +
                                                         mat.at<float>(x, y - 1) - 2 * mat.at<float>(x, y)) / 4;
                        } else if (y == cols - 1 && x > 0 && x < rows - 1) {
                            laplacian.at<float>(x, y) = (-5 * mat.at<float>(x, y - 1) + 4 * mat.at<float>(x, y - 2) -
                                                         mat.at<float>(x, y - 3) + 2 * mat.at<float>(x, y) +
                                                         mat.at<float>(x + 1, y) +
                                                         mat.at<float>(x - 1, y) - 2 * mat.at<float>(x, y)) / 4;
                        } else if (x == rows - 1 && y > 0 && y < cols - 1) {
                            laplacian.at<float>(x, y) = (-5 * mat.at<float>(x - 1, y) + 4 * mat.at<float>(x - 2, y) -
                                                         mat.at<float>(x - 3, y) + 2 * mat.at<float>(x, y) +
                                                         mat.at<float>(x, y + 1) +
                                                         mat.at<float>(x, y - 1) - 2 * mat.at<float>(x, y)) / 4;
                        }
                    }
                }
            }

            // Corners
            int x, y;

            //Upper-left
            x = 0;
            y = 0;
            laplacian.at<float>(x, y) = (-5 * mat.at<float>(x, y + 1) + 4 * mat.at<float>(x, y + 2) -
                                         mat.at<float>(x, y + 3) + 2 * mat.at<float>(x, y) -
                                         5 * mat.at<float>(x + 1, y) +
                                         4 * mat.at<float>(x + 2, y) - mat.at<float>(x + 3, y) +
                                         2 * mat.at<float>(x, y)) / 4;

            //Bottom-right
            x = rows - 1;
            y = cols - 1;
            laplacian.at<float>(x, y) = (-5 * mat.at<float>(x, y - 1) + 4 * mat.at<float>(x, y - 2) -
                                         mat.at<float>(x, y - 3) + 2 * mat.at<float>(x, y) -
                                         5 * mat.at<float>(x - 1, y) +
                                         4 * mat.at<float>(x - 2, y) - mat.at<float>(x - 3, y) +
                                         2 * mat.at<float>(x, y)) / 4;

            //Bottom-left
            x = 0;
            y = cols - 1;
            laplacian.at<float>(x, y) = (-5 * mat.at<float>(x, y - 1) + 4 * mat.at<float>(x, y - 2) -
                                         mat.at<float>(x, y - 3) + 2 * mat.at<float>(x, y) -
                                         5 * mat.at<float>(x + 1, y) +
                                         4 * mat.at<float>(x + 2, y) - mat.at<float>(x + 3, y) +
                                         2 * mat.at<float>(x, y)) / 4;

            //Upper-right
            x = rows - 1;
            y = 0;
            laplacian.at<float>(x, y) = (-5 * mat.at<float>(x, y + 1) + 4 * mat.at<float>(x, y + 2) -
                                         mat.at<float>(x, y + 3) + 2 * mat.at<float>(x, y) -
                                         5 * mat.at<float>(x - 1, y) +
                                         4 * mat.at<float>(x - 2, y) - mat.at<float>(x - 3, y) +
                                         2 * mat.at<float>(x, y)) / 4;

            return laplacian;
        }
    }
}
