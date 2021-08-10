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
#include <iostream>

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
        Image imagetest = Image("images/EDF/testsinglecell.png");
        Image image = Image(fileName);
        optimizeParameters(imagetest);

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
    void Segmenter::optimizeParameters(Image image){
        //focus on nuclei detection
        debug = true;
        
        
        //Image image2 = Image(filename2); 
        //cv::Mat testImage = cv::imread(filename1);
        //testImage = Segmenter::maskNuclei(testImage);

        
        cv::Mat outimg;

        

        double Bestdelta, BestminArea, BestmaxArea, BestmaxVariation, BestminDiversity, BestminCircularity;
        Bestdelta = this->delta;
        BestminArea = this->minArea;
        BestmaxArea = this->maxArea;
        BestmaxVariation = this->maxVariation ;
        BestminDiversity = this->minDiversity ;
        BestminCircularity = this->minCircularity ;
        // call on masked image again.
        bool Optimized = false;
        Image image1= image;
        
        cv::Mat gmmPredictions = runPreprocessing(&image1, kernelsize, maxdist, threshold1, threshold2, maxGmmIterations);
        image1.writeMatrix("gmmPredictions.yml", gmmPredictions);

        // Saving the matrix to png requires a threshold
        cv::threshold(gmmPredictions, outimg, 0, 256, CV_THRESH_BINARY);
        image1.writeImage("gmmPredictions.png", outimg);
        outimg.release();       
        if (debug) image1.log("Beginning GMM Output post processing...\n");

        // Finds the clump boundaries using the gmmPredictions mask
        vector <vector<cv::Point>> clumpBoundaries = findFinalClumpBoundaries(gmmPredictions, minAreaThreshold);

        // Color the clumps different colors and then write to png file
        outimg = drawColoredContours(image1.mat, &clumpBoundaries);
        image1.writeImage("clump_boundaries.png", outimg);
        if (debug) {
            //cv::imshow("Clump Segmentation", outimg);
            //cv::waitKey(0);
        }
        outimg.release();

        // Create a Clump object for each clump boundary
        image1.createClumps(clumpBoundaries);        
        if (debug) image.log("Beginning MSER nuclei detection...\n");        
                
        for(int i =0; i<this->minArea;i++){
                BestminArea = i;
                     for(int j = this->minArea; j<this->maxArea;j++){  
                        BestmaxArea = j;
                        for(double k =0.1; k<1;k + .01){
                                BestmaxVariation = k;
                                for(double l =0.8; l<4;i+ .1){
                                        Bestdelta = l;
                                        for(double m =0.1; m<1;i+.01){
                                                BestminDiversity = m;
                                                for(int intensity = 50; intensity < 200; intensity + 1){
                                                
                                                
                                                //std::cout << "\n about to run nuclei detetion";
                                                int total = runNucleiDetectionandMask(&image1, Bestdelta, BestminArea, BestmaxArea, BestmaxVariation, BestminDiversity, BestminCircularity, debug, intensity, false);
                                                if(debug == true){
                                                        std::cout << "\n" << total;
                                                }
                                                // Display and save nuclei to an image
                                                //outimg = image1.getNucleiBoundaries();
                                                

                                                
                                                // test nuclei detection on masked image
                                                Image image2 = image1; 

                                                int total2 = runNucleiDetectionandMask(&image2, Bestdelta, BestminArea, BestmaxArea, BestmaxVariation, BestminDiversity, BestminCircularity, debug, intensity, true);
                                                if(debug == true){
                                                        std::cout << "\n" << total2;
                                                }
                                                //system("pause");
                                                if (total2 ==1){        
                                                        Optimized = true;
                                                        break;
                                                 }
                                                
                                        
                                        }
                                        if(Optimized == true){break;}
                                }
                                if(Optimized == true){break;}
                        }
                        if(Optimized == true){break;}
                     }
                     if(Optimized == true){break;}
                }
                if(Optimized == true){break;}
                }
               // if(Optimized == true){break;}
                //Optimized= true;
        //}

        
        
        this->delta = Bestdelta;
        this->minArea = BestminArea;
        this->maxArea = BestmaxArea;
        this->maxVariation = BestmaxVariation;
        this->minDiversity = BestminDiversity;
        this->minCircularity = BestminCircularity;
        std::cout << "Best:" << Bestdelta << " " << BestminArea << " " << BestmaxArea << " " << BestmaxVariation << " " << BestminDiversity << " " << BestminCircularity;
        outimg = image1.getNucleiBoundaries();
        image.writeImage("nucleiBoundaries.png", outimg);
        outimg.release();


    }
    cv::Mat Segmenter::createNucleiHist(Image image, int delta, int minArea, int maxArea, double maxVariation, double minDiversity, double minCircularity, bool debug){
        runNucleiDetection( &image,  delta,  minArea, maxArea,  maxVariation,  minDiversity,  minCircularity,  debug);
        cv::Mat outimg;
        outimg = image.getNucleiBoundaries();
        image.writeImage("nucleiBoundaries.png", outimg);
        outimg.release();
        //run nuclei detection
        cv::Mat img1 = cv::imread("nucleiBoundaries.png", CV_LOAD_IMAGE_GRAYSCALE);
        cv::Mat histogram;
        int histSize = 256;
        //read in image
        float range[] = {0,256};
        const float* histRange[] = {range};
        bool uniform = true, accumulate = false;
        cv::calcHist(&img1, 1, 0, cv::Mat(), histogram, 1, &histSize, histRange, uniform, accumulate);
        //create hist and return
        return histogram;
        
        //return dataset;                                     
    }
    


}

