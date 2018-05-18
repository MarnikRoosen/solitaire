#include "stdafx.h"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/core.hpp"
#include "opencv2/opencv.hpp"
#include "opencv2/features2d.hpp"
#include "opencv2/imgcodecs.hpp"

#include <vector>
#include <utility>
#include <Windows.h>
#include <string>

using namespace cv;
using namespace std;

const int RESIZED_TYPE_WIDTH = 40;
const int RESIZED_TYPE_HEIGHT = 50;
const int MIN_CONTOUR_AREA = 100;

enum classifiers : char { TWO = '2', THREE = '3', FOUR = '4', FIVE = '5', SIX = '6',
	SEVEN = '7', EIGHT = '8', NINE = '9', TEN , JACK = 'J', QUEEN = 'Q', KING = 'K', ACE = 'A',
	CLUBS = 'C', SPADES = 'S', HEARTS = 'H', DIAMONDS = 'D', UNKNOWN = '?', EMPTY = '/'};

class ClassifyCard
{
public:
	ClassifyCard();

	// INITIALIZATION
	void getTrainedData(String type);								// getting the knn data from the correct map in the repos
	void generateTrainingData(cv::Mat trainingImage, String outputPreName);	// generating knn data if it doesn't exist yet
	void generateImageVector();										// initialization of the image vector used for the subtraction method

	// MAIN FUNCTIONS
	std::pair<Mat, Mat> segmentRankAndSuitFromCard(const Mat & aCard);
	std::pair<classifiers, classifiers> classifyCard(std::pair<Mat, Mat> cardCharacteristics);	// main function for classification of the rank/suit images
	
	
	// CLASSIFICATION METHODS
	int classifyTypeUsingShape(										// return: the index of image_list of the best match
		std::vector<std::pair<classifiers, cv::Mat>> &image_list,	// input: list of known (already classified) images
		std::vector<std::vector<cv::Point>> &contours,				// input: the contour of the unknown image
		double &lowestValueUsingShape);								// output: the certainty of the classification (lower is better)

	int classifyTypeUsingSubtraction(								// return: the index of image_list of the best match
		std::vector<std::pair<classifiers, cv::Mat>> &image_list,	// input: list of known (already classified) images
		cv::Mat &resizedROI,										// input: the unknown image
		int &lowestValue);											// output: the certainty of the classification (lower is better)

	classifiers classifyTypeUsingKnn(								// return: the classified type
		const Mat & image,											// input: the unknown image
		const Ptr<ml::KNearest> & kNearest);						// input: the correct trained knn algorithm


private:
	Rect myRankROI = Rect(4, 3, 22, 27);	// hardcoded values, but thanks to the resizing and consistent cardextraction, that is possible
	Rect mySuitROI = Rect(4, 30, 22, 21);

	vector<std::pair<classifiers, std::vector<double>>> rankHuMoments;
	vector<std::pair<classifiers, std::vector<double>>> red_suitHuMoments;
	vector<std::pair<classifiers, std::vector<double>>> black_suitHuMoments;
	vector<std::pair<classifiers, cv::Mat>> rankImages;
	vector<std::pair<classifiers, cv::Mat>> red_suitImages;
	vector<std::pair<classifiers, cv::Mat>> black_suitImages;
	int amountOfPerfectSegmentations = 0;
	Ptr<ml::KNearest>  kNearest_rank;
	Ptr<ml::KNearest>  kNearest_black_suit;
	Ptr<ml::KNearest>  kNearest_red_suit;

	std::pair<classifiers, classifiers> cardType;
	std::vector<std::pair<classifiers, cv::Mat>> image_list;
	std::string type;
	std::vector<std::vector<cv::Point> > contours;
	std::vector<cv::Vec4i> hierarchy;
	cv::Mat src, blurredImg, grayImg, threshImg, diff, resizedBlurredImg, resizedThreshImg;
	cv::Mat ROI, resizedROI;
	cv::Mat3b hsv;
	cv::Mat1b mask1, mask2, mask;
	int nonZero;

	Mat ROIFloat, ROIFlattenedFloat;
	float fltCurrentChar;
};

inline bool fileExists(const std::string& name) {
	ifstream f(name.c_str());
	return f.good();
}
