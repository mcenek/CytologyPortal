#include "SubImage.h"
#include "opencv2/opencv.hpp"
#include "../functions/Preprocessing.h"

namespace segment {
    SubImage::SubImage(cv::Mat *image, int i, int j, int subMatWidth, int subMatHeight, int paddingWidth, int paddingHeight) {
        this->image = image;
        this->x = i * subMatWidth;
        this->y = j * subMatHeight;
        this->subMatWidth = subMatWidth;
        this->subMatHeight = subMatHeight;
        this->i = i;
        this->j = j;
        this->maxI = ceil(image->cols / (double) subMatWidth) - 1;
        this->maxJ = ceil(image->rows / (double) subMatHeight) - 1;
        this->paddingWidth = paddingWidth;
        this->paddingHeight = paddingHeight;
        this->mat = getMat();
    }


    cv::Mat SubImage::getMat() {
        return crop(image, x, y, subMatWidth, subMatHeight, paddingWidth, paddingHeight);
    }

    cv::Mat SubImage::undoPadding() {
        int x = this->paddingWidth;
        int y = this->paddingHeight;
        int width = this->mat.cols - 2 * this->paddingWidth;
        int height = this->mat.rows - 2 * this->paddingHeight;

        if (this->i == 0) {
            x = 0;
        }
        if (this->i == 0 || this->i == this->maxI) {
            width += this->paddingWidth;
        }
        if (this->maxI == 0) {
            width += this->paddingWidth;
        }
        if (this->j == 0) {
            y = 0;
        }
        if (this->j == 0 || this->j == this->maxJ) {
            height += this->paddingHeight;
        }
        if (this->maxJ == 0) {
            height += this->paddingHeight;
        }

        cv::Mat cropped = crop(&this->mat, x, y, width, height, 0, 0);
        return cropped;

    }

    cv::Mat SubImage::crop(cv::Mat *mat, int x, int y, int width, int height, int paddingWidth, int paddingHeight) {
        cv::Rect returnRect = cv::Rect(x - paddingWidth, y - paddingHeight, width + (paddingWidth * 2), height + (paddingHeight * 2));
        if (returnRect.x < 0) {
            returnRect.width -= -returnRect.x;
            returnRect.x = 0;
        }
        if (returnRect.y < 0) {
            returnRect.height -= -returnRect.y;
            returnRect.y = 0;
        }
        if (returnRect.x + returnRect.width >= mat->cols)
            returnRect.width = mat->cols - returnRect.x;
        if (returnRect.y + returnRect.height >= mat->rows)
            returnRect.height = mat->rows - returnRect.y;
        cv::Mat croppedMat = (*mat)(returnRect);
        return croppedMat;
    }

    // Static method (not part of SubImage class) that divides a matrix into the
    // specified number of subImages
    // numberSubMatX: number of subMats along the horizontal axis
    // numberSubMatY: number of subMats along the vertical axis
    // paddingWidth: percent of overlap along the horizontal axis
    // paddingHeight: percent of overlap along the vertical axis
    vector<SubImage> splitMat(cv::Mat *mat, int numberSubMatX, int numberSubMatY, double paddingWidth, double paddingHeight) {
        int subMatWidth = ceil(mat->cols / (double) numberSubMatX);
        int subMatHeight = ceil(mat->rows / (double) numberSubMatY);

        vector<SubImage> subImages;
        for (int i = 0; i < numberSubMatX; i++) {
            for (int j = 0; j < numberSubMatY; j++) {
                paddingWidth = ceil(paddingWidth * subMatWidth);
                paddingHeight = ceil(paddingHeight * subMatHeight);
                SubImage subImage = SubImage(mat, i, j, subMatWidth, subMatHeight, paddingWidth, paddingHeight);
                subImages.push_back(subImage);
            }
        }
        return subImages;
    }
}