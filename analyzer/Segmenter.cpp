#include <iostream>
#include <chrono>
#include "Segmenter.h"
#include "functions/EvaluateSegmentation.h"
#include "functions/SegmenterTools.h"
#include "functions/ClumpSegmentation.h"
#include "functions/InitialCellSegmentation.h"
#include "functions/NucleiDetection.h"
#include "functions/OverlappingCellSegmentation.h"
#include "functions/Preprocessing.h"

extern "C" {
#include "vl/quickshift.h"
}

using namespace std;

namespace segment {
    // Constructor
    Segmenter::Segmenter(int kernelsize, int maxdist, int thres1, int thres2, int maxGmmIterations,
                         int minAreaThreshold, int delta, int minArea, int maxArea, double maxVariation,
                         double minDiversity, double minCircularity) {
        setCommonValues();
        this->kernelsize = kernelsize;
        this->maxdist = maxdist;
        this->maxGmmIterations = maxGmmIterations;
        this->threshold1 = thres1;
        this->threshold2 = thres2;
        this->minAreaThreshold = minAreaThreshold;
        this->delta = delta;
        this->minArea = minArea;
        this->maxArea = maxArea;
        this->maxVariation = maxVariation;
        this->minDiversity = minDiversity;
        this->minCircularity = minCircularity;
    }

    // Constructor helper
    void Segmenter::setCommonValues() {
        pink = cv::Scalar(1.0, 0, 1.0);
    }


    void Segmenter::runSegmentation(string fileName) {
        debug = true;
        auto total = chrono::high_resolution_clock::now();

        double end;

        Image image = Image(fileName);


        cv::Mat outimg;


        auto startClumpSeg = chrono::high_resolution_clock::now();
        auto start = chrono::high_resolution_clock::now();

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning Preprocessing...\n");

        // Check for a gmmPredictions matrix save file, if it doesn't exist
        // run preprocessing
        cv::Mat gmmPredictions = image.loadMatrix("gmmPredictions.yml");
        if (gmmPredictions.empty()) {
            // GMM predictions is a black and white photo of the input images
            // Where black is the background and white are the clumps
            gmmPredictions = runPreprocessing(&image, kernelsize, maxdist, threshold1, threshold2, maxGmmIterations);
            image.writeMatrix("gmmPredictions.yml", gmmPredictions);

            // Saving the matrix to png requires a threshold
            cv::threshold(gmmPredictions, outimg, 0, 256, CV_THRESH_BINARY);
            image.writeImage("gmmPredictions.png", outimg);
            outimg.release();
        }

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished with preprocessing, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning GMM Output post processing...\n");

        // Finds the clump boundaries using the gmmPredictions mask
        vector <vector<cv::Point>> clumpBoundaries = findFinalClumpBoundaries(gmmPredictions, minAreaThreshold);

        // Color the clumps different colors and then write to png file
        outimg = drawColoredContours(image.mat, &clumpBoundaries);
        image.writeImage("clump_boundaries.png", outimg);
        if (debug) {
            //cv::imshow("Clump Segmentation", outimg);
            //cv::waitKey(0);
        }
        outimg.release();

        // Create a Clump object for each clump boundary
        image.createClumps(clumpBoundaries);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        unsigned int numClumps = clumpBoundaries.size();
        if (debug) image.log("Finished GMM post processing, clumps found:%i, time: %f\n", numClumps, end);

        double endClumpSeg = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - startClumpSeg).count() / 1000000.0;
        if (debug) image.log("Finished clump segmentation, time: %f\n", endClumpSeg);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning MSER nuclei detection...\n");

        // Find nuclei in each clump
        runNucleiDetection(&image, delta, minArea, maxArea, maxVariation, minDiversity, minCircularity, debug);

        // Display and save nuclei to an image
        outimg = image.getNucleiBoundaries();
        image.writeImage("nucleiBoundaries.png", outimg);
        outimg.release();

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished MSER nuclei detection, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning initial cell segmentation...\n");

        // Takes the nuclei and creates a new Cell object for each since each cell has a nuclei
        // Estimates the initial cell boundaries of the cell by associating each point inside the
        // clump with the nearest nucleus. Then overlapping region is extrapolated with an ellipse.
        outimg = runInitialCellSegmentation(&image, threshold1, threshold2, debug);

        // Display and save initial cell boundaries to png file
        image.writeImage("initial_cell_boundaries.png", outimg);
        if (debug) {
            //cv::imshow("Initial Cell Segmentation", outimg);
            //cv::waitKey(0);
        }
        outimg.release();

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished initial cell segmentation, time: %f\n", end);

        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning segmentation by level set functions...\n");

        // Use level sets to find the actual cell boundaries by shrinking the initial cell
        // boundaries' overlapping extrapolated contour (the ellipse) until the level set converges.
        runOverlappingSegmentation(&image);

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished level set cell segmentation, time: %f\n", end);
        //displayResults(&image.clumps);


        // clean up
        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - total).count() / 1000000.0;

        if (debug || totalTimed) image.log("Segmentation finished, total time: %f\n", end);

        outimg = image.getFinalResult();
        image.writeImage("cell_boundaries.png", outimg);
        outimg.release();


        start = chrono::high_resolution_clock::now();
        if (debug) image.log("Beginning segmentation evaluation...\n");
        // If ground truths exist, use them to find the dice coefficient of the segmentation.
        double dice = evaluateSegmentation(&image);
        image.log("Dice coefficient: %f\n", dice);

        ofstream diceFile;
        boost::filesystem::path writePath = image.getWritePath("dice", ".txt");
        diceFile.open(writePath.string());
        diceFile << dice;
        diceFile.close();

        end = std::chrono::duration_cast<std::chrono::microseconds>(
                chrono::high_resolution_clock::now() - start).count() / 1000000.0;
        if (debug) image.log("Finished segmentation evaluation, time: %f\n", end);


        if (debug) {
            //cv::imshow("Overlapping Cell Segmentation", outimg);
            //cv::waitKey(0);
        }
    }
    void Segmenter::optimizeParameters(string filename1, string filename2){
        //focus on nuclei detection
        debug = true;
        
        Image image1 = Image(filename1); 
        Image image2 = Image(filename2); 
        cv::Mat outimg;
        double* firstHist;
        firstHist = createNucleiHist(image1, delta, minArea, maxArea, maxVariation, minDiversity, minCircularity, debug);
        double* secondHist;
        secondHist = createNucleiHist(image2, delta, minArea, maxArea, maxVariation, minDiversity, minCircularity, debug);
        

        //cv::Mat::fixed<2,266> dataset = mat(2,266);
        //dataset = {firstHist, secondHist};
        
        //use single cells for responses
        //use contour with blkank background  and simulate with background same intensity as cytom plasm
        
        //using namespace mlpack;
        //double validationSize = .5;
        //hpt::HyperParameterTuner<createNucleiHist, mlpack::cv::Accuracy, mlpack::cv::SimpleCV, ens::GridSearch > hpt(validationSize, dataset, responses); // need to figure out responses
        double Bestdelta, BestminArea, BestmaxArea, BestmaxVariation, BestminDiversity, BestminCircularity;
        

        
        
        this->delta = Bestdelta;
        this->minArea = BestminArea;
        this->maxArea = BestmaxArea;
        this->maxVariation = BestmaxVariation;
        this->minDiversity = BestminDiversity;
        this->minCircularity = BestminCircularity;
        std::cout << "Best:" << Bestdelta << " " << BestminArea << " " << BestmaxArea << " " << BestmaxVariation << " " << BestminDiversity << " " << BestminCircularity;



    }
    double* createNucleiHist(Image image, int delta, int minArea, int maxArea, double maxVariation, double minDiversity, double minCircularity, bool debug){
        runNucleiDetection( &image,  delta,  minArea, maxArea,  maxVariation,  minDiversity,  minCircularity,  debug);
        cv::Mat outimg;
        outimg = image.getNucleiBoundaries();
        image.writeImage("nucleiBoundaries.png", outimg);
        outimg.release();
        //double dataset[256];
        // = Segmenter::toHistogram("nucleiBoundaries.png");
        //source: https://docs.opencv.org/3.4/d5/d98/tutorial_mat_operations.html
        cv::Mat img1 = cv::imread("nucleiBoundaries.png", CV_LOAD_IMAGE_GRAYSCALE);
        int histogram[256];

        for(int y = 0; y < img1.rows; y++){
        for(int x = 0; x < img1.cols; x++){
                histogram[(int)img1.at<uchar>(y,x)]++;
        }
        }
        //for(int i = 0; i < 256; i++){ cout<<histogram[i]<<" "; }
        /* int hist_w = img1.cols; 
        int hist_h = img1.rows;
        int bin_w = cvRound((double) hist_w/256); */
        std::sort(histogram, histogram + 256, greater<int>());
        double histogram2[256];
        for(int z = 0; z< 256; z++){
                histogram2[z] = (double) histogram[z];
        }
       
        

        /* for(int i = 0; i < 255; i++){ // normalize intensity
                histogram2[i] = ((double)histogram[i]/max)*histImage.rows;
        } */
        return histogram2;
        
        //return dataset;                                     
    }


}

