#include "opencv2/opencv.hpp"
#include "../objects/Clump.h"
#include "LSM.h"
#include <ctime>
using namespace std;

namespace segment {
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
            int rows = mat.rows;
            int cols = mat.cols;

            cv::Mat f = cv::Mat(rows, cols, CV_32FC1);
            cv::Mat b = cv::Mat(rows, cols, CV_32FC1);

            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                    float value = mat.at<float>(i, j);
                    //f=(1/2/sigma)*(1+cos(pi*x/sigma));
                    f.at<float>(i, j) = (1.0 / 2.0 / sigma) * (1 + cos(M_PI * value / sigma));

                    //(x<=sigma) & (x>=-sigma);
                    b.at<float>(i, j) = int(value <= sigma && value >= -sigma);
                }
            }
            cv::multiply(f, b, f);
            return f;
        }

        //Returns a divergence of the given matrix
        cv::Mat calcDivergence(cv::Mat x, cv::Mat y) {
            vector <cv::Mat> gradientX = calcGradient(x);
            vector <cv::Mat> gradientY = calcGradient(y);
            cv::Mat gradientXX = getGradientX(gradientX);
            cv::Mat gradientXY = getGradientY(gradientX);
            cv::Mat gradientYX = getGradientX(gradientY);
            cv::Mat gradientYY = getGradientY(gradientY);
            return gradientXX + gradientYY;
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

        vector <cv::Mat> calcGradient(cv::Mat image) {
            int rows = image.rows;
            int cols = image.cols;

            cv::Mat gradientX = cv::Mat(rows, cols, CV_32FC1);
            cv::Mat gradientY = cv::Mat(rows, cols, CV_32FC1);

            float dx = 0.0;
            float dy = 0.0;

            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                    if (j == 0) {
                        dx = image.at<float>(i, j + 1) - image.at<float>(i, j);
                    } else if (j == cols - 1) {
                        dx = image.at<float>(i, j) - image.at<float>(i, j - 1);
                    } else {
                        dx = 0.5 * (image.at<float>(i, j + 1) - image.at<float>(i, j - 1));
                    }

                    if (i == 0) {
                        dy = image.at<float>(i + 1, j) - image.at<float>(i, j);
                    } else if (i == rows - 1) {
                        dy = image.at<float>(i, j) - image.at<float>(i - 1, j);
                    } else {
                        dy = 0.5 * (image.at<float>(i + 1, j) - image.at<float>(i - 1, j));
                    }

                    gradientX.at<float>(i, j) = dx;
                    gradientY.at<float>(i, j) = dy;
                }
            }

            return {gradientX, gradientY};
        }

        //Returns the magnitude for the given matricies
        cv::Mat calcGradientMagnitude(cv::Mat img) {
            vector <cv::Mat> gradient = calcGradient(img);
            return calcMagnitude(getGradientX(gradient), getGradientY(gradient));
        }

        cv::Mat calcHeaviside(cv::Mat mat) {
            cv::Mat heaviside = mat.clone();
            for (int i = 0; i < heaviside.rows; i++) {
                float *row = heaviside.ptr<float>(i);
                for (int j = 0; j < heaviside.cols; j++) {
                    float value = row[j];
                    if (value <= 0) row[j] = 1;
                    else row[j] = 0;
                }
            }
            return heaviside;
        }

        //Returns the magnitude for the given matricies
        cv::Mat calcMagnitude(cv::Mat x, cv::Mat y) {
            cv::Mat magnitude;
            cv::magnitude(x, y, magnitude);
            return magnitude;
        }

        //Applies the normalized derivative of the potential function ......
        cv::Mat calcPotential(cv::Mat gradientMagnitude) {
            int rows = gradientMagnitude.rows;
            int cols = gradientMagnitude.cols;

            cv::Mat a = cv::Mat(rows, cols, CV_32FC1);
            cv::Mat b = cv::Mat(rows, cols, CV_32FC1);
            cv::Mat c = cv::Mat(rows, cols, CV_32FC1);

            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                    float value = gradientMagnitude.at<float>(i, j);
                    //a = (s>=0) & (s<=1)
                    a.at<float>(i, j) = int(value >= 0 && value <= 1);
                    //b = (s>1)
                    b.at<float>(i, j) = int(value > 1);
                    //c = sin(2*pi*s)
                    c.at<float>(i, j) = sin(2 * M_PI * value);
                }
            }

            cv::Mat d, e;
            //d = a.*sin(2*pi*s)
            cv::multiply(a, c, d);
            //e = b.*(s-1)
            cv::multiply(b, gradientMagnitude - 1, e);

            //return a.*sin(2*pi*s)/(2*pi)+b.*(s-1)
            return c / (2 * M_PI) + e;

        }

        //A unary LSF energy term, following the equation - μdiv(dp(|∇φi|)∇φi)
        cv::Mat calcSignedDistanceReg(cv::Mat mat) {
            vector <cv::Mat> gradient = calcGradient(mat);
            cv::Mat gradientX = getGradientX(gradient);
            cv::Mat gradientY = getGradientY(gradient);
            cv::Mat gradientMagnitude = calcMagnitude(gradientX, gradientY);

            cv::Mat potential = calcPotential(gradientMagnitude);
            int rows = gradientMagnitude.rows;
            int cols = gradientMagnitude.cols;

            cv::Mat a = cv::Mat(rows, cols, CV_32FC1);
            cv::Mat b = cv::Mat(rows, cols, CV_32FC1);
            cv::Mat c = cv::Mat(rows, cols, CV_32FC1);
            cv::Mat d = cv::Mat(rows, cols, CV_32FC1);

            for (int i = 0; i < rows; i++) {
                for (int j = 0; j < cols; j++) {
                    float potentialValue = potential.at<float>(i, j);
                    float gradientMagnitudeValue = gradientMagnitude.at<float>(i, j);

                    //a = (ps~=0)
                    a.at<float>(i, j) = int(potentialValue != 0);
                    //b = (ps==0)
                    b.at<float>(i, j) = int(potentialValue == 0);

                    //c = (s~=0)
                    c.at<float>(i, j) = int(gradientMagnitudeValue != 0);
                    //d = (s==0)
                    d.at<float>(i, j) = int(gradientMagnitudeValue == 0);
                }
            }

            //a = (ps~=0).*ps
            cv::multiply(a, potential, a);
            //a = (ps~=0).*ps+(ps==0)
            a = a + b;

            //c = (s~=0).*s
            cv::multiply(c, gradientMagnitude, c);
            //c = (s~=0).*s+(s==0)
            c = c + d;

            cv::Mat dps;
            //dps=((ps~=0).*ps+(ps==0))./((s~=0).*s+(s==0));
            cv::divide(a, c, dps);

            //a = dps.*phi_x
            cv::multiply(dps, gradientX, a);
            //b = dps.*phi_y
            cv::multiply(dps, gradientY, b);

            //f = div(dps.*phi_x - phi_x, dps.*phi_y - phi_y) + 4*del2(phi);
            return calcDivergence(a - gradientX, b - gradientY) + 4 * del2(mat);
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

        cv::Mat getGradientX(vector <cv::Mat> gradient) {
            return gradient[0];
        }

        cv::Mat getGradientY(vector <cv::Mat> gradient) {
            return gradient[1];
        }

        bool hasOverlap(cv::Mat mat1, cv::Mat mat2) {
            cv::Mat thresholdMat1, thresholdMat2;
            cv::threshold(mat1, thresholdMat1, 0, 1, CV_THRESH_BINARY_INV);
            cv::threshold(mat2, thresholdMat2, 0, 1, CV_THRESH_BINARY_INV);
            cv::Mat overlapMask;
            cv::bitwise_and(thresholdMat1, thresholdMat2, overlapMask);
            return cv::countNonZero(overlapMask) > 0;
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