/**
 * This file is part of the nestk library.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Nicolas Burrus <nicolas.burrus@uc3m.es>, (C) 2010
 */

// ORIGINAL INCLUDES
#include <ntk/ntk.h>
#include <ntk/utils/debug.h>
#include <ntk/camera/nite_rgbd_grabber.h>

#include <QApplication>
#include <QDir>
#include <QMutex>

// CUSTOM INCLUES
#include <set>

using namespace cv;
using namespace ntk;

namespace opt
{
  ntk::arg<bool> high_resolution("--highres", "High resolution color image.", 0);
}

void findMinAndMaxAndNumberOfUniques(cv::Mat1f arr){
	int w = arr.cols;
	int h = arr.rows;
	float depth_min = 2048.0;	// v. bad i know - too lazy to look up int class
	float depth_max = -2048.0;
	std::set<float> uniqueFloats;
	for(int i=0; i<h; i++){
		for(int j=0; j<w; j++){
			float val = arr(i,j);	//does this work?	//yep =]
			//^ are these in meters =D? - seems like it
			//the measuring tape agrees!
			if(val<depth_min){depth_min=val;}
			if(depth_max<val){depth_max=val;}
			uniqueFloats.insert(val);
		}
	}
	printf("min = %f ", depth_min);
	printf("max = %f ", depth_max);
	printf("uniques = %i ", uniqueFloats.size());
	printf("\n");
}

cv::Mat3b customProcessing(cv::Mat3b color, cv::Mat1f depthRaw)
// color is the color image
// depthRaw is the pre-aligned raw depth data (values are from 0-1023 i think)
// output is the output image that this function should populate
// color and depthRaw should be READ-ONLY, please don't mess them up? =]
// feel free to write to output all you want though =]
{
	// init output matrix
	cv::Mat3b output = cvCreateMat(color.rows, color.cols, color.type());

	// CODE FOR CUSTOM_IMG OUTPUT
	int w = output.cols;
	int h = output.rows;
	for(int i=0; i<h; i++){
		for(int j=0; j<w; j++){
			output(i,j) = Vec3b(0,0,255);	// colors are BGR
		}
	}

	// remember that gaussian kernels need to be odd dimesions ex: Size(11,11)
	//cv::GaussianBlur(color, output, Size(11,11), 0);
	
	return output;
}

int main(int argc, char **argv)
{
  // Parse command line options.
  arg_base::set_help_option("-h");
  arg_parse(argc, argv);

  // Set debug level to 1.
  ntk::ntk_debug_level = 1;

  // Set current directory to application directory.
  // This is to find Nite config in config/ directory.
  QApplication app (argc, argv);
  QDir::setCurrent(QApplication::applicationDirPath());

  // Declare the frame grabber.
  NiteRGBDGrabber grabber;

  // High resolution 1280x1024 RGB Image.
  if (opt::high_resolution())
    grabber.setHighRgbResolution(true);

  // Start the grabber.
  grabber.setBodyEventDetector(false);
  grabber.initialize();
  grabber.start();

  // Holder for the current image.
  RGBDImage image;

  // Image post processor. Compute mappings when RGB resolution is 1280x1024.
  ///NiteProcessor post_processor;

  namedWindow("depth");
  namedWindow("color");
  namedWindow("users");
  namedWindow("custom");

  // quick img saving
  grabber.waitForNextFrame();
  grabber.copyImageTo(image);
  ///post_processor.processImage(image);
  cv::Mat1b debug_depth_img = normalize_toMat1b(image.depth());
  cv::Mat3b debug_color_img;
  image.mappedRgb().copyTo(debug_color_img);
  //cv::imwrite("color.png", debug_color_img);
  //cv::imwrite("depth.png", debug_depth_img);
  printf("asdf");

  while (true)
  {
	grabber.waitForNextFrame();
	grabber.copyImageTo(image);

	// Setup depth image
	cv::Mat1b debug_depth_img = normalize_toMat1b(image.depth());

	// Setup color image
	cv::Mat3b debug_color_img;
	image.mappedRgb().copyTo(debug_color_img);

	// Setup users image
	//(this appears to be the built-in person detection in nite_rgbd_grabber.cpp)
	//cv::Mat3b debug_users;
    //image.fillRgbFromUserLabels(debug_users);

	// Setup custom image
	// We'll use this for our DoF processing =]
	cv::Mat3b custom_img = customProcessing(debug_color_img, image.depth());

	// DEBUGGING / EXPERIMENTATION
	findMinAndMaxAndNumberOfUniques(image.depth());

	// Show images =]
    imshow("depth", debug_depth_img);
    imshow("color", debug_color_img);
    //imshow("users", debug_users);
	imshow("custom", custom_img);
    cv::waitKey(10);
  }

  return app.exec();
}
