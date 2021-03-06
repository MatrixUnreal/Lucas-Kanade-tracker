#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/highgui.hpp"

#include <iostream>
#include <ctype.h>

using namespace cv;
using namespace std;

static void help()
{
	// print a welcome message, and the OpenCV version
	cout << "\nThis is a demo of Lukas-Kanade optical flow lkdemo(),\n"
		"Using OpenCV version " << CV_VERSION << endl;
	cout << "\nIt uses camera by default, but you can provide a path to video as an argument.\n";
	cout << "\nHot keys: \n"
		"\tESC - quit the program\n"
		"\tr - auto-initialize tracking\n"
		"\tc - delete all the points\n"
		"\tn - switch the \"night\" mode on/off\n"
		"To add/remove a feature point click it\n" << endl;
}

Point2f point;
bool addRemovePt = false;

static void onMouse(int event, int x, int y, int
	//flags
	, void*
	//param
	)
{
	if (event == EVENT_LBUTTONDOWN)
	{
		point = Point2f((float)x, (float)y);
		addRemovePt = true;
	}
}

int main(int argc, char** argv)
{
	VideoCapture cap;

	const string name = "Output.avi";
	int fourcc = CV_FOURCC('X', 'V', 'I', 'D');
	double dWidth = cap.get(CV_CAP_PROP_FRAME_WIDTH); //get the width of frames of the video
	double dHeight = cap.get(CV_CAP_PROP_FRAME_HEIGHT); //get the height of frames of the video
	Size frameSize(static_cast<int>(dWidth), static_cast<int>(dHeight));
	VideoWriter oVideoWriter(name, fourcc, 18, frameSize, true); //initialize the VideoWriter object 

	TermCriteria termcrit(TermCriteria::COUNT | TermCriteria::EPS, 20, 0.03);
	Size subPixWinSize(10, 10), winSize(31, 31);

	const int MAX_COUNT = 500;
	bool needToInit = false;
	bool nightMode = false;

	help();
	cv::CommandLineParser parser(argc, argv, "{@input|0|}");
	string input = parser.get<string>("@input");

	if (input.size() == 1 && isdigit(input[0]))
		cap.open(input[0] - '0');
	else
		cap.open(input);

	if (!cap.isOpened())
	{
		cout << "Could not initialize capturing...\n";
		return 0;
	}

	namedWindow("LK Demo", 1);
	setMouseCallback("LK Demo", onMouse, 0);

	Mat gray, prevGray, image, frame;
	vector<Point2f> points[2];

	for (;;)
	{
		cap >> frame;
		if (frame.empty())
			break;

		frame.copyTo(image);
		cvtColor(image, gray, COLOR_BGR2GRAY);

		if (nightMode)
			image = Scalar::all(0);

		if (needToInit)
		{
			// automatic initialization
			goodFeaturesToTrack(gray, points[1], MAX_COUNT, 0.01, 10, Mat(), 3, 3, 0);//, 0.04);
			cornerSubPix(gray, points[1], subPixWinSize, Size(-1, -1), termcrit);
			addRemovePt = false;
		}
		else if (!points[0].empty())
		{
			vector<uchar> status;
			vector<float> err;
			if (prevGray.empty())
				gray.copyTo(prevGray);
			calcOpticalFlowPyrLK(prevGray, gray, points[0], points[1], status, err, winSize,
				3, termcrit, 0, 0.001);
			size_t i, k;
			for (i = k = 0; i < points[1].size(); i++)
			{
				if (addRemovePt)
				{
					if (norm(point - points[1][i]) <= 5)
					{
						addRemovePt = false;
						continue;
					}
				}

				if (!status[i])
					continue;

				points[1][k++] = points[1][i];
				circle(image, points[1][i], 3, Scalar(0, 255, 0), -1, 8);
			}
			points[1].resize(k);
		}

		if (addRemovePt && points[1].size() < (size_t)MAX_COUNT)
		{
			vector<Point2f> tmp;
			tmp.push_back(point);
			cornerSubPix(gray, tmp, winSize, Size(-1, -1), termcrit);
			points[1].push_back(tmp[0]);
			addRemovePt = false;
		}

		needToInit = false;

		oVideoWriter << image;

		imshow("LK Demo", image);

		char c = (char)waitKey(10);
		if (c == 27)
			break;
		switch (c)
		{
		case 'r':
			needToInit = true;
			break;
		case 'c':
			points[0].clear();
			points[1].clear();
			break;
		case 'n':
			nightMode = !nightMode;
			break;
		}

		std::swap(points[1], points[0]);
		cv::swap(prevGray, gray);
	}

	return 0;
}

/////////////cornerDetector_Demo///////////////////////////
/*
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/highgui.hpp"

#include <ctype.h>
#include <iostream>

using namespace cv;
using namespace std;

/// Global variables
Mat src, src_gray;
Mat myHarris_dst; Mat myHarris_copy; Mat Mc;
Mat myShiTomasi_dst; Mat myShiTomasi_copy;

int myShiTomasi_qualityLevel = 50;
int myHarris_qualityLevel = 50;
int max_qualityLevel = 100;

double myHarris_minVal; double myHarris_maxVal;
double myShiTomasi_minVal; double myShiTomasi_maxVal;

RNG rng(12345);

const char* myHarris_window = "My Harris corner detector";
const char* myShiTomasi_window = "My Shi Tomasi corner detector";

/// Function headers
void myShiTomasi_function(int, void*);
void myHarris_function(int, void*);

//
// @function main
//
int main(int argc, char** argv)
{
	/// Load source image and convert it to gray
	CommandLineParser parser(argc, argv, "{@input | ../data/stuff.jpg | input image}");
	//src = imread(parser.get<String>("@input"), IMREAD_COLOR);
	VideoCapture cap(0);
	for(int i=0;i<10;i++)
	cap >> src;
	if (src.empty())
	{
		cout << "Could not open or find the image!\n" << endl;
		cout << "Usage: " << argv[0] << " <Input image>" << endl;
		return -1;
	}
	cvtColor(src, src_gray, COLOR_BGR2GRAY);

	/// Set some parameters
	int blockSize = 3; int apertureSize = 3;

	/// My Harris matrix -- Using cornerEigenValsAndVecs
	myHarris_dst = Mat::zeros(src_gray.size(), CV_32FC(6));
	Mc = Mat::zeros(src_gray.size(), CV_32FC1);

	cornerEigenValsAndVecs(src_gray, myHarris_dst, blockSize, apertureSize, BORDER_DEFAULT);

	// calculate Mc 
	for (int j = 0; j < src_gray.rows; j++)
	{
		for (int i = 0; i < src_gray.cols; i++)
		{
			float lambda_1 = myHarris_dst.at<Vec6f>(j, i)[0];
			float lambda_2 = myHarris_dst.at<Vec6f>(j, i)[1];
			Mc.at<float>(j, i) = lambda_1 * lambda_2 - 0.04f*pow((lambda_1 + lambda_2), 2);
		}
	}

	minMaxLoc(Mc, &myHarris_minVal, &myHarris_maxVal, 0, 0, Mat());

	// Create Window and Trackbar 
	namedWindow(myHarris_window, WINDOW_AUTOSIZE);
	createTrackbar(" Quality Level:", myHarris_window, &myHarris_qualityLevel, max_qualityLevel, myHarris_function);
	myHarris_function(0, 0);

	/// My Shi-Tomasi -- Using cornerMinEigenVal
	myShiTomasi_dst = Mat::zeros(src_gray.size(), CV_32FC1);
	cornerMinEigenVal(src_gray, myShiTomasi_dst, blockSize, apertureSize, BORDER_DEFAULT);

	minMaxLoc(myShiTomasi_dst, &myShiTomasi_minVal, &myShiTomasi_maxVal, 0, 0, Mat());

	// Create Window and Trackbar 
	namedWindow(myShiTomasi_window, WINDOW_AUTOSIZE);
	createTrackbar(" Quality Level:", myShiTomasi_window, &myShiTomasi_qualityLevel, max_qualityLevel, myShiTomasi_function);
	myShiTomasi_function(0, 0);

	waitKey(0);
	return(0);
}

//
// @function myShiTomasi_function
//
void myShiTomasi_function(int, void*)
{
	myShiTomasi_copy = src.clone();

	if (myShiTomasi_qualityLevel < 1) { myShiTomasi_qualityLevel = 1; }

	for (int j = 0; j < src_gray.rows; j++)
	{
		for (int i = 0; i < src_gray.cols; i++)
		{
			if (myShiTomasi_dst.at<float>(j, i) > myShiTomasi_minVal + (myShiTomasi_maxVal - myShiTomasi_minVal)*myShiTomasi_qualityLevel / max_qualityLevel)
			{
				circle(myShiTomasi_copy, Point(i, j), 4, Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255)), -1, 8, 0);
			}
		}
	}
	imshow(myShiTomasi_window, myShiTomasi_copy);
}

//
// @function myHarris_function
//
void myHarris_function(int, void*)
{
	myHarris_copy = src.clone();

	if (myHarris_qualityLevel < 1) { myHarris_qualityLevel = 1; }

	for (int j = 0; j < src_gray.rows; j++)
	{
		for (int i = 0; i < src_gray.cols; i++)
		{
			if (Mc.at<float>(j, i) > myHarris_minVal + (myHarris_maxVal - myHarris_minVal)*myHarris_qualityLevel / max_qualityLevel)
			{
				circle(myHarris_copy, Point(i, j), 4, Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255)), -1, 8, 0);
			}
		}
	}
	imshow(myHarris_window, myHarris_copy);
}
*/

/*
#include "opencv2/video/tracking.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/highgui.hpp"

#include <ctype.h>
#include <iostream>


using namespace cv;
using namespace std;

/// Global variables
Mat src, src_gray;
int thresh = 200;
int max_thresh = 255;

const char* source_window = "Source image";
const char* corners_window = "Corners detected";

/// Function header
void cornerHarris_demo(int, void*);


int main(int argc, char** argv)
{
	/// Load source image and convert it to gray
	CommandLineParser parser(argc, argv, "{@input | ../data/building.jpg | input image}");
	//src = imread(parser.get<String>("@input"), IMREAD_COLOR);

	VideoCapture cap(0);
	for (int i = 0; i<10; i++)
		cap >> src;

	if (src.empty())
	{
		cout << "Could not open or find the image!\n" << endl;
		cout << "Usage: " << argv[0] << " <Input image>" << endl;
		return -1;
	}
	cvtColor(src, src_gray, COLOR_BGR2GRAY);

	/// Create a window and a trackbar
	namedWindow(source_window, WINDOW_AUTOSIZE);
	createTrackbar("Threshold: ", source_window, &thresh, max_thresh, cornerHarris_demo);
	imshow(source_window, src);

	cornerHarris_demo(0, 0);

	waitKey(0);
	return(0);
}
//
// @function cornerHarris_demo
// @brief Executes the corner detection and draw a circle around the possible corners
//
void cornerHarris_demo(int, void*)
{

	Mat dst, dst_norm, dst_norm_scaled;
	dst = Mat::zeros(src.size(), CV_32FC1);

	/// Detector parameters
	int blockSize = 2;
	int apertureSize = 3;
	double k = 0.04;

	/// Detecting corners
	cornerHarris(src_gray, dst, blockSize, apertureSize, k, BORDER_DEFAULT);

	/// Normalizing
	normalize(dst, dst_norm, 0, 255, NORM_MINMAX, CV_32FC1, Mat());
	convertScaleAbs(dst_norm, dst_norm_scaled);

	/// Drawing a circle around corners
	for (int j = 0; j < dst_norm.rows; j++)
	{
		for (int i = 0; i < dst_norm.cols; i++)
		{
			if ((int)dst_norm.at<float>(j, i) > thresh)
			{
				circle(dst_norm_scaled, Point(i, j), 5, Scalar(0), 2, 8, 0);
			}
		}
	}
	/// Showing the result
	namedWindow(corners_window, WINDOW_AUTOSIZE);
	imshow(corners_window, dst_norm_scaled);
}
*/
/*
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include <iostream>

using namespace cv;
using namespace std;

/// Global variables
Mat src, src_gray;

int maxCorners = 10;
int maxTrackbar = 25;

RNG rng(12345);
const char* source_window = "Image";

/// Function header
void goodFeaturesToTrack_Demo(int, void*);

int main(int argc, char** argv)
{
	/// Load source image and convert it to gray
	CommandLineParser parser(argc, argv, "{@input | ../data/pic3.png | input image}");
	//src = imread(parser.get<String>("@input"), IMREAD_COLOR);
	VideoCapture cap(0);
	for (int i = 0; i<10; i++)
		cap >> src;
	if (src.empty())
	{
		cout << "Could not open or find the image!\n" << endl;
		cout << "Usage: " << argv[0] << " <Input image>" << endl;
		return -1;
	}
	cvtColor(src, src_gray, COLOR_BGR2GRAY);

	/// Create Window
	namedWindow(source_window, WINDOW_AUTOSIZE);

	/// Create Trackbar to set the number of corners
	createTrackbar("Max  corners:", source_window, &maxCorners, maxTrackbar, goodFeaturesToTrack_Demo);

	imshow(source_window, src);

	goodFeaturesToTrack_Demo(0, 0);

	waitKey(0);
	return(0);
}

//
// @function goodFeaturesToTrack_Demo.cpp
// @brief Apply Shi-Tomasi corner detector
//
void goodFeaturesToTrack_Demo(int, void*)
{
	if (maxCorners < 1) { maxCorners = 1; }

	/// Parameters for Shi-Tomasi algorithm
	vector<Point2f> corners;
	double qualityLevel = 0.01;
	double minDistance = 10;
	int blockSize = 3, gradiantSize = 3;
	bool useHarrisDetector = false;
	double k = 0.04;

	/// Copy the source image
	Mat copy;
	copy = src.clone();

	/// Apply corner detection
	goodFeaturesToTrack(src_gray,
		corners,
		maxCorners,
		qualityLevel,
		minDistance,
		Mat(),
		blockSize,
		gradiantSize,
		useHarrisDetector
		//,k
	);


	/// Draw corners detected
	cout << "** Number of corners detected: " << corners.size() << endl;
	int r = 4;
	for (size_t i = 0; i < corners.size(); i++)
	{
		circle(copy, corners[i], r, Scalar(rng.uniform(0, 255), rng.uniform(0, 255), rng.uniform(0, 255)), -1, 8, 0);
	}

	/// Show what you got
	namedWindow(source_window, WINDOW_AUTOSIZE);
	imshow(source_window, copy);

	/// Set the neeed parameters to find the refined corners
	Size winSize = Size(5, 5);
	Size zeroZone = Size(-1, -1);
	TermCriteria criteria = TermCriteria(TermCriteria::EPS + TermCriteria::COUNT, 40, 0.001);

	/// Calculate the refined corner locations
	cornerSubPix(src_gray, corners, winSize, zeroZone, criteria);

	/// Write them down
	for (size_t i = 0; i < corners.size(); i++)
	{
		cout << " -- Refined Corner [" << i << "]  (" << corners[i].x << "," << corners[i].y << ")" << endl;
	}
}*/
/*
#include "opencv2/objdetect.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"

#include <iostream>
#include <cstdio>
#include <vector>
#include <algorithm>

using namespace std;
using namespace cv;

// Functions for facial feature detection
static void help();
static void detectFaces(Mat&, vector<Rect_<int> >&, string);
static void detectEyes(Mat&, vector<Rect_<int> >&, string);
static void detectNose(Mat&, vector<Rect_<int> >&, string);
static void detectMouth(Mat&, vector<Rect_<int> >&, string);
static void detectFacialFeaures(Mat&, const vector<Rect_<int> >, string, string, string);

string input_image_path;
string	face_cascade_path="C:\\Rustam\\Library\\OpenCV32\\opencv\\build\\etc\\haarcascades\\haarcascade_frontalface_alt2.xml",
		eye_cascade_path ="C:\\Rustam\\Library\\OpenCV32\\opencv\\build\\etc\\haarcascades\\haarcascade_eye_tree_eyeglasses.xml", 
		nose_cascade_path="", 
		mouth_cascade_path="C:\\Rustam\\Library\\OpenCV32\\opencv\\build\\etc\\haarcascades\\haarcascade_smile.xml";

int main(int argc, char** argv)
{
//	cv::CommandLineParser parser(argc, argv,
//		"{eyes||}{nose||}{mouth||}{help h||}");
//	if (parser.has("help"))
//	{
//		help();
//		return 0;
//	}
//	input_image_path = parser.get<string>(0);
//	face_cascade_path = parser.get<string>(1);
//	eye_cascade_path = parser.has("eyes") ? parser.get<string>("eyes") : "";
//	nose_cascade_path = parser.has("nose") ? parser.get<string>("nose") : "";
//	mouth_cascade_path = parser.has("mouth") ? parser.get<string>("mouth") : "";
	if (input_image_path.empty() || face_cascade_path.empty())
	{
		cout << "IMAGE or FACE_CASCADE are not specified";
	//	return 1;
	}

	// Load image and cascade classifier files
	Mat image;
	VideoCapture cap(0);
	//for (int i = 0; i<10; i++)
	while (1)
	{
		cap >> image;
		//image = imread(input_image_path);

		// Detect faces and facial features
		vector<Rect_<int> > faces;
		detectFaces(image, faces, face_cascade_path);
		detectFacialFeaures(image, faces, eye_cascade_path, nose_cascade_path, mouth_cascade_path);

		imshow("Result", image);
		waitKey(10);
	}
	
	return 0;
}

static void help()
{
	cout << "\nThis file demonstrates facial feature points detection using Haarcascade classifiers.\n"
		"The program detects a face and eyes, nose and mouth inside the face."
		"The code has been tested on the Japanese Female Facial Expression (JAFFE) database and found"
		"to give reasonably accurate results. \n";

	cout << "\nUSAGE: ./cpp-example-facial_features [IMAGE] [FACE_CASCADE] [OPTIONS]\n"
		"IMAGE\n\tPath to the image of a face taken as input.\n"
		"FACE_CASCSDE\n\t Path to a haarcascade classifier for face detection.\n"
		"OPTIONS: \nThere are 3 options available which are described in detail. There must be a "
		"space between the option and it's argument (All three options accept arguments).\n"
		"\t-eyes=<eyes_cascade> : Specify the haarcascade classifier for eye detection.\n"
		"\t-nose=<nose_cascade> : Specify the haarcascade classifier for nose detection.\n"
		"\t-mouth=<mouth-cascade> : Specify the haarcascade classifier for mouth detection.\n";


	cout << "EXAMPLE:\n"
		"(1) ./cpp-example-facial_features image.jpg face.xml -eyes=eyes.xml -mouth=mouth.xml\n"
		"\tThis will detect the face, eyes and mouth in image.jpg.\n"
		"(2) ./cpp-example-facial_features image.jpg face.xml -nose=nose.xml\n"
		"\tThis will detect the face and nose in image.jpg.\n"
		"(3) ./cpp-example-facial_features image.jpg face.xml\n"
		"\tThis will detect only the face in image.jpg.\n";

	cout << " \n\nThe classifiers for face and eyes can be downloaded from : "
		" \nhttps://github.com/opencv/opencv/tree/master/data/haarcascades";

	cout << "\n\nThe classifiers for nose and mouth can be downloaded from : "
		" \nhttps://github.com/opencv/opencv_contrib/tree/master/modules/face/data/cascades\n";
}

static void detectFaces(Mat& img, vector<Rect_<int> >& faces, string cascade_path)
{
	CascadeClassifier face_cascade;
	face_cascade.load(cascade_path);

	face_cascade.detectMultiScale(img, faces, 1.15, 3, 0 | CASCADE_SCALE_IMAGE, Size(30, 30));
	return;
}

static void detectFacialFeaures(Mat& img, const vector<Rect_<int> > faces, string eye_cascade,
	string nose_cascade, string mouth_cascade)
{
	for (unsigned int i = 0; i < faces.size(); ++i)
	{
		// Mark the bounding box enclosing the face
		Rect face = faces[i];
		rectangle(img, Point(face.x, face.y), Point(face.x + face.width, face.y + face.height),
			Scalar(255, 0, 0), 1, 4);

		// Eyes, nose and mouth will be detected inside the face (region of interest)
		Mat ROI = img(Rect(face.x, face.y, face.width, face.height));

		// Check if all features (eyes, nose and mouth) are being detected
		bool is_full_detection = false;
		if ((!eye_cascade.empty()) && (!nose_cascade.empty()) && (!mouth_cascade.empty()))
			is_full_detection = true;

		// Detect eyes if classifier provided by the user
		if (!eye_cascade.empty())
		{
			vector<Rect_<int> > eyes;
			detectEyes(ROI, eyes, eye_cascade);

			// Mark points corresponding to the centre of the eyes
			for (unsigned int j = 0; j < eyes.size(); ++j)
			{
				Rect e = eyes[j];
				circle(ROI, Point(e.x + e.width / 2, e.y + e.height / 2), 3, Scalar(0, 255, 0), -1, 8);
				// rectangle(ROI, Point(e.x, e.y), Point(e.x+e.width, e.y+e.height),
				//Scalar(0, 255, 0), 1, 4); 
			}
		}

		// Detect nose if classifier provided by the user
		double nose_center_height = 0.0;
		if (!nose_cascade.empty())
		{
			vector<Rect_<int> > nose;
			detectNose(ROI, nose, nose_cascade);

			// Mark points corresponding to the centre (tip) of the nose
			for (unsigned int j = 0; j < nose.size(); ++j)
			{
				Rect n = nose[j];
				circle(ROI, Point(n.x + n.width / 2, n.y + n.height / 2), 3, Scalar(0, 255, 0), -1, 8);
				nose_center_height = (n.y + n.height / 2);
			}
		}

		// Detect mouth if classifier provided by the user
		double mouth_center_height = 0.0;
		if (!mouth_cascade.empty())
		{
			vector<Rect_<int> > mouth;
			detectMouth(ROI, mouth, mouth_cascade);

			for (unsigned int j = 0; j < mouth.size(); ++j)
			{
				Rect m = mouth[j];
				mouth_center_height = (m.y + m.height / 2);

				// The mouth should lie below the nose
				if ((is_full_detection) && (mouth_center_height > nose_center_height))
				{
					rectangle(ROI, Point(m.x, m.y), Point(m.x + m.width, m.y + m.height), Scalar(0, 255, 0), 1, 4);
				}
				else if ((is_full_detection) && (mouth_center_height <= nose_center_height))
					continue;
				else
					rectangle(ROI, Point(m.x, m.y), Point(m.x + m.width, m.y + m.height), Scalar(0, 255, 0), 1, 4);
			}
		}

	}

	return;
}

static void detectEyes(Mat& img, vector<Rect_<int> >& eyes, string cascade_path)
{
	CascadeClassifier eyes_cascade;
	eyes_cascade.load(cascade_path);

	eyes_cascade.detectMultiScale(img, eyes, 1.20, 5, 0 | CASCADE_SCALE_IMAGE, Size(30, 30));
	return;
}

static void detectNose(Mat& img, vector<Rect_<int> >& nose, string cascade_path)
{
	CascadeClassifier nose_cascade;
	nose_cascade.load(cascade_path);

	nose_cascade.detectMultiScale(img, nose, 1.20, 5, 0 | CASCADE_SCALE_IMAGE, Size(30, 30));
	return;
}

static void detectMouth(Mat& img, vector<Rect_<int> >& mouth, string cascade_path)
{
	CascadeClassifier mouth_cascade;
	mouth_cascade.load(cascade_path);

	mouth_cascade.detectMultiScale(img, mouth, 1.10, 5, 0 | CASCADE_SCALE_IMAGE, Size(30, 30));
	return;
}*/
