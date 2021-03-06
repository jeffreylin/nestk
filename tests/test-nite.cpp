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
#include <Winbase.h>
#include <Windows.h>

using namespace cv;
using namespace ntk;

namespace opt
{
  // Included with the original Nestk code. Not currently used/tested with DoF processing
  ntk::arg<bool> high_resolution("--highres", "High resolution color image.", 0);
}

int focalPointX = 320;
int focalPointY = 240;
bool updateFocalPoint = false;	// wow... this is friggin hackish x.x

void customWindowMouseCallback(int event, int x, int y, int flags, void* param){
	switch(event){
		case CV_EVENT_LBUTTONUP:
			focalPointX = x;
			focalPointY = y;
			updateFocalPoint = true;	// wow, super lame handling... x.x
			printf("Setting focal point to: (%i, %i)", x, y);
			break;
	}
}

void findMinAndMaxAndNumberOfUniques(cv::Mat1f arr){
	int w = arr.cols;
	int h = arr.rows;
	float depth_min = 2048.0;	// v. bad i know - too lazy to look up int class min/max vals
	float depth_max = -2048.0;
	std::set<float> uniqueFloats;
	for(int i=0; i<h; i++){
		for(int j=0; j<w; j++){
			float val = arr(i,j);	//does this work?	//yep =]
			//^ are these in meters =D? - seems like it
			//the tape measure agrees!
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

float film35MMToScreen(float in){
	// OUTPUT IMAGE PROPERTIES
	float HORIZONTAL_PIXELS = 640;	//in pixels

	// FILM PROPERTIES (35mm film)
	// ^kinda sorta, because 35mm film is 3:2, but kinect images are 4:3
	float FILM_WIDTH = 0.036;	//in meters

	return in * (HORIZONTAL_PIXELS/FILM_WIDTH);
}

// try f = 0.035m, n = 1.4
float getCircleOfConfusion(float focalLength, float fStop, float focusedDistance, float subjectDistance){
	// via http://en.wikipedia.org/wiki/Circle_of_confusion
	float f = focalLength;	// in meters
	float n = fStop;		// in meters
	float s_1 = focusedDistance;	// in meters
	float s_2 = subjectDistance;	// in meters
	return (abs(s_2-s_1)/s_2) * (f*f/(n*(s_1-f)));	// in meters
}

int roundToNearestOdd(float in){
	// we do this, because the Gaussian filter in OpenCV expects odd filter sizes
	return int(in/2)*2+1;
}

cv::Mat3b* previousImg;
std::map<int, cv::Mat3b> renderedGaussians;

cv::Mat3b memoizedGaussian(cv::Mat3b color, int blur){
	// CHECK FOR EASY CASES
	if(blur==1){return color;}
	if(blur>color.cols){blur=roundToNearestOdd(color.cols);}

	// GO
	//printf("values in cache: %i ", (int) renderedGaussians.size());
	//printf("blurring %i", blur);
	//printf("\n");
	/*	this is now done in the main loop - arggg gross... x.x
	if(&color != previousImg){
		printf("color %i not equal to previousImg %i", &color, previousImg);
		renderedGaussians.clear();
		previousImg = &color;
	}
	*/
	std::map<int, cv::Mat3b>::iterator possibleMatch = renderedGaussians.find(blur);
	if(possibleMatch != renderedGaussians.end()){	// element found
		//printf("cache hit for %i \n", blur);
		return possibleMatch->second;
	}
	else{	// element is not found
		printf("cache miss for %i \n", blur);
		cv::Mat3b blurred = cvCreateMat(color.rows, color.cols, color.type());
		cv::GaussianBlur(color, blurred, cv::Size(blur,blur), 0);
		renderedGaussians[blur] = blurred;
		return blurred;
	}
}

float focalDistance = 2;	// in meters
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
		//SETUP
	// CAMERA PROPERTIES
	float FOCAL_LENGTH = 0.035;	// in meters
	float FSTOP = 1.4;			// absolute units
	if(updateFocalPoint){focalDistance=depthRaw(focalPointY,focalPointX);updateFocalPoint=false;}
	//^NOTE: this "backwards" accessor order is correct
	printf("Blurring using %.3fmm lens at f%.3f focused at %.3f meters\n", FOCAL_LENGTH, FSTOP, focalDistance);

		// MAIN LOOP
	int w = output.cols;
	int h = output.rows;
	for(int i=0; i<h; i++){
		for(int j=0; j<w; j++){
			float d = depthRaw(i,j);

			/*
			output(i,j) = Vec3b(0,0,255);	// colors are BGR
			*/

			/*
			// hard cut off function
			float depthOfField = 0.25;	// in meters
			output(i,j) = (abs(d-focalDistance)<(depthOfField/2.0)) ? 
				color(i,j) : blurred(i,j);
			*/

			// DoF Equation
			float pixelsToBlur = 
				roundToNearestOdd(film35MMToScreen(getCircleOfConfusion(
					FOCAL_LENGTH, FSTOP, focalDistance, d
				)));
			cv::Mat3b blurred = memoizedGaussian(
				color, pixelsToBlur
			);
			output(i,j) = blurred(i,j);
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

  // Start the grabber
  grabber.setBodyEventDetector(false);	//modded nite_rgbd_grabber to gt this to work x.x
  grabber.initialize();
  grabber.start();

  // Holder for the current image.
  RGBDImage image;

  // Image post processor. Compute mappings when RGB resolution is 1280x1024.
  ///NiteProcessor post_processor;

  namedWindow("depth");
  namedWindow("color");
  namedWindow("custom");
  cvSetMouseCallback("depth", customWindowMouseCallback);
  cvSetMouseCallback("color", customWindowMouseCallback);
  cvSetMouseCallback("custom", customWindowMouseCallback);
  
  int start,end;
  int customStart, customEnd;
  while (true)
  {
	start = GetTickCount();
	grabber.waitForNextFrame();
	grabber.copyImageTo(image);	//takes (by GetTickCount) between 0-16ms

	// Setup color image
	cv::Mat3b debug_color_img;
	image.mappedRgb().copyTo(debug_color_img);

	// Setup depth image
	cv::Mat1b raw_depth_img;
	image.depth().copyTo(raw_depth_img);

	customStart = GetTickCount();
	// Setup custom image
	// We'll use this for our DoF processing =]
	cv::Mat3b custom_img = customProcessing(debug_color_img, raw_depth_img);
	renderedGaussians.clear();	//meh, this shouldn't be done here... =/
	printf("custom rendered in ~%ims ", GetTickCount()-customStart);

	// DEBUGGING / EXPERIMENTATION
	//findMinAndMaxAndNumberOfUniques(raw_depth_img);

	// Show images =]
    imshow("depth", normalize_toMat1b(raw_depth_img));
    imshow("color", debug_color_img);
	imshow("custom", custom_img);
	end = GetTickCount();
	printf("all images rendered in ~%ims ", end-start);
	printf("\n");
    if(cv::waitKey(2)==' '){
		cv::imwrite("color.tif", debug_color_img);
		cv::imwrite("depth.tif", raw_depth_img);
		cv::imwrite("custom.tif", custom_img);
		printf("Images Saved \n");
	}
  }

  return app.exec();
}
