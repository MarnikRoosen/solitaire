#include "stdafx.h"
#include "GenerateTrainingData.h"
#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include<opencv2/ml/ml.hpp>

#include<iostream>
#include<vector>

using namespace std;
using namespace cv;

const int MIN_CONTOUR_AREA = 100;
const int RESIZED_IMAGE_WIDTH = 20;
const int RESIZED_IMAGE_HEIGHT = 30;
const string basePath = "C:/Users/marni/source/repos/gameAnalysis/x64/Debug/";


GenerateTrainingData::GenerateTrainingData()
{
}

int generateData() {

	Mat grayImg;
	Mat blurredImg;
	Mat threshImg;
	Mat threshImgCopy;

	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	cv::Mat matClassificationInts;
	cv::Mat matTrainingImagesAsFlattenedFloats;

	// number 10 can already be recognized when there are 2 contours in a rank image
	std::vector<int> intValidChars = { '1', '2', '3', '4', '5', '6', '7', '8', '9','A','J','K','Q'};

	Mat trainingNumbersImg = imread(basePath + "trainingImg.png");
	if (trainingNumbersImg.empty()) {
		std::cout << "error: image not read from file\n\n";
		return(0);
	}

	cvtColor(trainingNumbersImg, grayImg, CV_BGR2GRAY);
	GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
	adaptiveThreshold(blurredImg,threshImg,255,cv::ADAPTIVE_THRESH_GAUSSIAN_C,cv::THRESH_BINARY_INV,11,2);
	imshow("threshImg", threshImg);
	threshImgCopy = threshImg.clone();	// findcontours modifies the image

	findContours(threshImgCopy, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

	for (int i = 0; i < contours.size(); i++) {
		if (contourArea(contours[i]) > MIN_CONTOUR_AREA) {
			Rect boundingRect = cv::boundingRect(contours[i]);
			rectangle(trainingNumbersImg, boundingRect, cv::Scalar(0, 0, 255), 2);

			Mat ROI = threshImg(boundingRect);
			Mat ROIResized;
			resize(ROI, ROIResized, Size(RESIZED_IMAGE_WIDTH, RESIZED_IMAGE_HEIGHT));

			imshow("matROI", ROI);
			imshow("matROIResized", ROIResized);
			imshow("imgTrainingNumbers", trainingNumbersImg);

			int intChar = cv::waitKey(0);	// get user input

			if (intChar == 27) {        // if esc key was pressed
				return(0);
			}
			else if (find(intValidChars.begin(), intValidChars.end(), intChar) != intValidChars.end()) {
				matClassificationInts.push_back(intChar);
				Mat matImageFloat;
				ROIResized.convertTo(matImageFloat, CV_32FC1);
				Mat matImageFlattenedFloat = matImageFloat.reshape(1, 1);
				matTrainingImagesAsFlattenedFloats.push_back(matImageFlattenedFloat);
				// knn requires flattened float images
			}
		}
	}

	cout << "training complete" << endl;

	// save classifications to xml file

	cv::FileStorage fsClassifications("classifications.xml", cv::FileStorage::WRITE);
	if (fsClassifications.isOpened() == false) {
		std::cout << "error, unable to open training classifications file, exiting program\n\n";
		return(0);
	}

	fsClassifications << "classifications" << matClassificationInts;
	fsClassifications.release();

	cv::FileStorage fsTrainingImages("images.xml", cv::FileStorage::WRITE);
	if (fsTrainingImages.isOpened() == false) {
		std::cout << "error, unable to open training images file, exiting program\n\n";
		return(0);
	}

	fsTrainingImages << "images" << matTrainingImagesAsFlattenedFloats;
	fsTrainingImages.release(); 

	return(0);
}
