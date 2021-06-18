#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"
#include "../functions/SegmenterTools.h"
#include "DRLSE.h"
#include <ctime>
using namespace std;

namespace segment {
    namespace drlse {

        void updatePhi(Cell *cellI, Clump *clump, double dt, double epsilon, double mu, double kappa, double chi) {
            cv::Mat phi = cellI->getPhi();

            cv::Rect boundingBoxWithNeighbors = cellI->boundingBoxWithNeighbors;
            boundingBoxWithNeighbors.width = phi.cols;
            boundingBoxWithNeighbors.height = phi.rows;

            cv::Mat edgeEnforcer = clump->edgeEnforcer.clone();
            edgeEnforcer = edgeEnforcer(boundingBoxWithNeighbors);

            cv::Mat clumpPrior = clump->clumpPrior.clone();
            clumpPrior = clumpPrior(boundingBoxWithNeighbors);

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

        cv::Mat calcAllBinaryEnergy(Cell *cellI, cv::Mat edgeEnforcer, cv::Mat clumpPrior, cv::Mat phiI, cv::Mat dirac) {
            cv::Mat binaryEnergy = cv::Mat::zeros(phiI.rows, phiI.cols, CV_32FC1);
            for (unsigned int cellIdxJ = 0; cellIdxJ < cellI->neighbors.size(); cellIdxJ++) {
                Cell *cellJ = cellI->neighbors[cellIdxJ];

                if (cellI == cellJ) {
                    continue;
                }

                cv::Rect boundingBoxWithNeighbors = cellI->boundingBoxWithNeighbors;
                cv::Mat phiJ = cellJ->getPhi(boundingBoxWithNeighbors);
                binaryEnergy += calcBinaryEnergy(phiJ, edgeEnforcer, clumpPrior, dirac);
            }
            return binaryEnergy;
        }

        cv::Mat calcBinaryEnergy(cv::Mat mat, cv::Mat edgeEnforcer, cv::Mat clumpPrior, cv::Mat dirac) {
            cv::Mat overAreaTerm;
            cv::Mat heaviside = calcHeavisideInv(mat);
            overAreaTerm = clumpPrior.mul(edgeEnforcer);
            overAreaTerm = overAreaTerm.mul(dirac);
            overAreaTerm = overAreaTerm.mul(heaviside);
            return overAreaTerm;
        }

        vector <cv::Mat> calcCurvatureXY(vector <cv::Mat> gradient) {
            double smallNumber = 1e-10;
            cv::Mat gradientX = getGradientX(gradient);
            cv::Mat gradientY = getGradientY(gradient);
            cv::Mat gradientMagnitude = calcMagnitude(gradientX, gradientY);
            cv::Mat Nx, Ny;
            cv::divide(gradientX, gradientMagnitude + smallNumber, Nx);
            cv::divide(gradientY, gradientMagnitude + smallNumber, Ny);
            return {Nx, Ny};
        }

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



        cv::Mat calcEdgeEnforcer(cv::Mat mat) {
            mat = mat.clone();
            cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);
            mat.convertTo(mat, CV_32FC1);

            //G=fspecial('gaussian',15,sigma)
            cv::Mat gaussianKernel = cv::getGaussianKernel(15, 1.5, CV_32FC1);
            cv::mulTransposed(gaussianKernel, gaussianKernel, false);
            //Img_smooth=conv2(Img,G,'same')
            cv::Mat gaussian = conv2(mat, gaussianKernel, "same");
            //[Ix,Iy]=gradient(Img_smooth)
            vector <cv::Mat> gaussianGradient = calcGradient(gaussian);
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

        //A unary LSF energy term, following the equation - κδε(φi)div(hCg∇φi|∇φi|)
        cv::Mat calcGeodesicTerm(cv::Mat dirac, vector <cv::Mat> gradient, cv::Mat edgeEnforcer, cv::Mat clumpPrior) {
            cv::Mat distPair = edgeEnforcer.mul(clumpPrior);
            vector <cv::Mat> distPairGradient = calcGradient(distPair);

            cv::Mat vxd = getGradientX(distPairGradient);
            cv::Mat vyd = getGradientY(distPairGradient);

            vector <cv::Mat> Nxy = calcCurvatureXY(gradient);
            cv::Mat Nx = Nxy[0];
            cv::Mat Ny = Nxy[1];
            cv::Mat curvature = calcDivergence(Nx, Ny);

            return dirac.mul(vxd.mul(Nx) + vyd.mul(Ny)) + dirac.mul(clumpPrior).mul(edgeEnforcer).mul(curvature);
        }

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

        //A unary LSF energy term, following the equation - μdiv(dp(|∇φi|)∇φi)
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

        //Returns the original matrix as a 1 channel CV_32F
        cv::Mat ensureMatrixType(cv::Mat matrix) {
            cv::Mat tmpMatrix = matrix.clone();
            if (tmpMatrix.channels() >= 3) cv::cvtColor(tmpMatrix, tmpMatrix, CV_BGR2GRAY, 1);
            if (tmpMatrix.type() != CV_32F) tmpMatrix.convertTo(tmpMatrix, CV_32F, 1.0 / 255);
            return tmpMatrix;
        }

        //find biggest contour instead, moe to segtools
        vector<vector<cv::Point>> getContours(cv::Mat mat) {
            mat.convertTo(mat, CV_8UC1, 255);
            vector<vector<cv::Point>> contours;
            cv::findContours(mat, contours, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_NONE);
            //cv::drawContours(contourMat, contours, 0, cv::Scalar(0.));
            return contours;
        }



        cv::Mat neumannBoundCond(cv::Mat mat) {
            int rows = mat.rows;
            int cols = mat.cols;

            cv::Mat nbc = mat.clone();

            //g([1 nrow],[1 ncol]) = g([3 nrow-2],[3 ncol-2]);
            nbc.at<float>(0, 0) = nbc.at<float>(2, 2);
            nbc.at<float>(0, cols - 1) = nbc.at<float>(2, cols - 3);
            nbc.at<float>(rows - 1, 0) = nbc.at<float>(rows - 3, 2);
            nbc.at<float>(rows - 1, cols - 1) = nbc.at<float>(rows - 3, cols - 3);

            //g([1 nrow],2:end-1) = g([3 nrow-2],2:end-1);
            for (int j = 1; j < cols - 1; j++) {
                nbc.at<float>(0, j) = nbc.at<float>(2, j);
                nbc.at<float>(rows - 1, j) = nbc.at<float>(rows - 3, j);
            }

            //g(2:end-1,[1 ncol]) = g(2:end-1,[3 ncol-2]);
            for (int i = 1; i < rows - 1; i++) {
                nbc.at<float>(i, 0) = nbc.at<float>(i, 2);
                nbc.at<float>(i, cols - 1) = nbc.at<float>(i, cols - 3);
            }

            return nbc;
        }
    }
}
