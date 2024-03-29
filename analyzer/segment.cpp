// segment.cpp
// initial program to apply c++ tools from various libraries, including vlfeat and opencv
// to recreate the pipeline of cell segmentation described in the paper
// by Zhi Lu et al. :
// https://cs.adelaide.edu.au/~zhi/publications/paper_TIP_Jan04_2015_Finalised_two_columns.pdf

/*
The following is about linking libraries - still applicable, but hopefully
most of it is taken care of via `make`

Linking the vlfeat library requires some doing:

g++ segment.cpp -I$VLROOT -L$VLROOT/bin/glnxa64/ -lvl
where $VLROOT is the path to your vlroot source files, so ex: /usr/local/lib/vlfeat-0.9.21

then setup and env variable in the terminal,
export LD_LIBRARY_PATH=/usr/local/lib/vlfeat-0.9.21/bin/glnxa64
(or append this to the existing LD_LIBRARY_PATH if needed)

then run the executable and cross your fingers

For command line arguments:
how to use reference: https://theboostcpplibraries.com/boost.program_options
get boost on linux: https://stackoverflow.com/questions/12578499/how-to-install-boost-on-ubuntu
		/use the apt-get version

compliation example: g++ -std=gnu++17 segment.cpp -l boost_program_options
	Make sure to include the "-l boost_program_options" otherwise you will get errors that do not make sense and the program will not work
*/

//#include <napi.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include "Segmenter.h"
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>

using namespace std;
using namespace boost::program_options;

int main (int argc, const char * argv[])
{
  //TODO: Cleanup segmenter cosntructors
    // Default parameters
    // Quickshift params
    int kernelsize = 2;
    int maxdist = 4;
    // Canny params
    int threshold1 = 20;
    int threshold2 = 40;
    // GMM params
    int maxGmmIterations = 10;
    // GMM post processing params
    double minAreaThreshold = 1000.0;
    // MSER params
    double nucleiSize = 35; // Average nuclei size in um
    double nucleiSizeError = 10; // Nuclei size +/- error
    int delta = 3, minArea = -1, maxArea = -1;
    double maxVariation = 0.2, minDiversity = 0.3;
    double minCircularity = 0.5;
    // Cell segmentation params
    double dt = 5; //Time step
    double epsilon = 1.5; //Pixel spacing
    double mu = 0.04; //Contour length weighting parameter
    double kappa = 13;
    double chi = 3;

    try
    {
        options_description desc{"Options"};
        desc.add_options()
          ("help,h", "Help screen")
          ("dt", value<float>()->default_value(dt), "Timestep")
          ("epsilon", value<float>()->default_value(epsilon), "Pixel spacing")
          ("mu", value<float>()->default_value(mu), "Contour length weight")
          ("kappa", value<float>()->default_value(kappa), "Unary term weight")
          ("chi", value<float>()->default_value(chi), "Binary term weight")
          ("maxVariation", value<float>()->default_value(maxVariation), "Max variation")
          ("minDiversity", value<float>()->default_value(minDiversity), "Min diversity")
          ("minCircularity", value<float>()->default_value(minCircularity), "Min circularity")
          ("minAreaThreshold", value<float>()->default_value(minAreaThreshold), "Min area threshold")
          ("kernelsize", value<int>()->default_value(kernelsize), "Kernel size")
          ("maxdist", value<int>()->default_value(maxdist), "Max distance")
          ("threshold1", value<int>()->default_value(threshold1), "Threshold1")
          ("threshold2", value<int>()->default_value(threshold2), "Threshold2")
          ("maxGmmIterations", value<int>()->default_value(maxGmmIterations), "Max GMM iterations")
          ("nucleiSize", value<float>()->default_value(nucleiSize), "Nuclei size")
          ("nucleiSizeError", value<float>()->default_value(nucleiSizeError), "Nuclei size error")
          ("delta", value<int>()->default_value(delta), "Delta")
          ("minArea", value<int>()->default_value(minArea), "Min area")
          ("maxArea", value<int>()->default_value(maxArea), "Max area")
          ("image,i", value<std::string>()->default_value(""), "Input image")
          ("imageResolution", value<float>()->required(), "Image resolution pixels/μm")
          ("directory,d", value<std::string>()->default_value(""), "Input directory");
        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);

      	if (vm.count("help")) {
      	  cout << desc << "\n";
      	  return 1;
      	}
	if (vm.count("help")) {
	  cout << desc << "\n";
	  return 1;
	}

        int minArea, maxArea;

        if (vm["minArea"].as<int>() == -1) {
            minArea = (vm["nucleiSize"].as<float>() - vm["nucleiSizeError"].as<float>())
                    * vm["imageResolution"].as<float>();
        } else {
            minArea = vm["minArea"].as<int>();
        }

        if (vm["maxArea"].as<int>() == -1) {
            maxArea = (vm["nucleiSize"].as<float>() + vm["nucleiSizeError"].as<float>())
                      * vm["imageResolution"].as<float>();
        } else {
            maxArea = vm["maxArea"].as<int>();
        }

        segment::Segmenter seg = segment::Segmenter(
            vm["kernelsize"].as<int>(),
            vm["maxdist"].as<int>(),
            vm["threshold1"].as<int>(),
            vm["threshold2"].as<int>(),
            vm["maxGmmIterations"].as<int>(),
            vm["minAreaThreshold"].as<float>(),
            vm["delta"].as<int>(),
            minArea,
            maxArea,
            vm["maxVariation"].as<float>(),
            vm["minDiversity"].as<float>(),
            vm["minCircularity"].as<float>(),
            vm["dt"].as<float>(),
            vm["epsilon"].as<float>(),
            vm["mu"].as<float>(),
            vm["kappa"].as<float>(),
            vm["chi"].as<float>()
        );

        vector<boost::filesystem::path> images;

	    string directory = vm["directory"].as<std::string>();
        string image = vm["image"].as<std::string>();

        if (directory.empty() && image.empty()) {
            image = "./images/EDF/EDF000.png";
        }

        if (!directory.empty()) {

            boost::filesystem::path path(directory);
            if (boost::filesystem::is_directory(directory)) {
                boost::filesystem::directory_iterator iter(path), eod;
                BOOST_FOREACH(boost::filesystem::path const& file, make_pair(iter, eod)){
                    if (is_regular_file(file)) {
                        if (file.has_extension()) {
                            if (file.extension().string() == ".png" ||
                                file.extension().string() == ".tiff") {
                                images.push_back(file);
                            }
                        }
                    }
                }
            } else {
                printf("Not a directory: %s\n", directory.c_str());
            }

        }

        if (!image.empty()) {
            images.push_back(image);
        }

        sort(images.begin(), images.end());

        for (boost::filesystem::path const& image : images) {
            seg.runSegmentation(image.string());
        }

    }
    catch (const exception &ex)
    {
      std::cerr << ex.what() << '\n';
    }
}
