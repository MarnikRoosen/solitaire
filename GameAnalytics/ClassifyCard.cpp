#include "stdafx.h"
#include "ClassifyCard.h"
#include <string>

ClassifyCard::ClassifyCard()
{
	standardCardSize.width = 150;
	standardCardSize.height = 200;
	generateMoments();
	generateImageVector();
	std::vector<string> type = { "rank", "black_suit", "red_suit" };
	for (int i = 0; i < type.size(); i++)
	{
		String name = "../GameAnalytics/knnData/trained_" + type.at(i) + ".yml";
		if (!fileExists(name))
		{
			getTrainedData(type.at(i));
		}
	}
	kNearest_rank = ml::KNearest::load<ml::KNearest>("../GameAnalytics/knnData/trained_rank.yml");
	kNearest_black_suit = ml::KNearest::load<ml::KNearest>("../GameAnalytics/knnData/trained_black_suit.yml");
	kNearest_red_suit = ml::KNearest::load<ml::KNearest>("../GameAnalytics/knnData/trained_red_suit.yml");
}

std::pair<classifiers, classifiers> ClassifyCard::classifyCard(std::pair<Mat, Mat> cardCharacteristics)
{
	// initialize variables
	std::pair<classifiers, classifiers> cardType;
	std::vector<std::pair<classifiers, std::vector<double>>> huMoments_list;
	std::vector<std::pair<classifiers, cv::Mat>> image_list;
	std::string type = "rank";
	std::vector<std::vector<cv::Point> > contours;
	std::vector<cv::Vec4i> hierarchy;
	cv::Mat src = cardCharacteristics.first;
	cv::Mat blurredImg, grayImg, threshImg , diff, resizedBlurredImg, resizedThreshImg;
	cv::Mat ROI, resizedROI;
	double lowestValueOfShape;
	double huMomentsA[7];
	int indexOfLowestValueUsingComparison;
	int lowestValueOfComparison;
	int indexOfLowestValueUsingShape;

	for (int i = 0; i < 2; i++)
	{	
		// determine color for the suitclassification
		if (type != "rank")
		{
			Mat3b hsv;
			cvtColor(cardCharacteristics.first, hsv, COLOR_BGR2HSV);
			Mat1b mask1, mask2;
			inRange(hsv, Scalar(0, 70, 50), Scalar(10, 255, 255), mask1);
			inRange(hsv, Scalar(170, 70, 50), Scalar(180, 255, 255), mask2);
			Mat1b mask = mask1 | mask2;
			int nonZero = countNonZero(mask);
			if (nonZero > 0)
			{
				type = "red_suit";
			}
		}
		
		// process the src
		cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(5, 5), 0);
		threshold(blurredImg, threshImg, 150, 255, THRESH_BINARY_INV);
		cv::findContours(threshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

 		if (contours.empty())
		{
			cardType.first = EMPTY;
			cardType.second = EMPTY;
			return cardType;
		}

		// Sort and remove objects that are too small
		std::sort(contours.begin(), contours.end(), [] (const vector<Point>& c1, const vector<Point>& c2)
			-> bool { return contourArea(c1, false) > contourArea(c2, false); });

		if (type == "rank" && contours.size() > 1 && contourArea(contours.at(1), false) > 15.0)
		{
			cardType.first = TEN;
		}
		else
		{
			ROI = threshImg(boundingRect(contours.at(0)));
			if (type == "rank")
			{
				cv::resize(ROI, resizedROI, cv::Size(RESIZED_TYPE_WIDTH, RESIZED_TYPE_HEIGHT));
				huMoments_list = rankHuMoments;
				image_list = rankImages;
			}
			else
			{
				cv::resize(ROI, resizedROI, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));
				(type == "red_suit") ? huMoments_list = red_suitHuMoments : huMoments_list = black_suitHuMoments;
				(type == "red_suit") ? image_list = red_suitImages : image_list = black_suitImages;
			}
			cv::GaussianBlur(resizedROI, resizedBlurredImg, cv::Size(3, 3), 0);
			cv::threshold(resizedBlurredImg, resizedThreshImg, 140, 255, THRESH_BINARY);
			cv::findContours(resizedThreshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);


			// COMPARISON METHOD
			lowestValueOfComparison = INT_MAX;
			indexOfLowestValueUsingComparison = classifyTypeUsingComparison(image_list, resizedThreshImg, lowestValueOfComparison);
			if (type == "rank")
			{
				cardType.first = image_list.at(indexOfLowestValueUsingComparison).first;

			}
			else
			{
				cardType.second = image_list.at(indexOfLowestValueUsingComparison).first;
			}

			/*
			// SHAPE ANALYSIS - KNN
			HuMoments(moments(contours.at(0)), huMomentsA);
			lowestValueOfShape = DBL_MAX;
			indexOfLowestValueUsingShape = classifyTypeWithShape(huMoments_list, huMomentsA, lowestValueOfShape);
			if (lowestValueOfShape > 0.15 || huMoments_list.at(indexOfLowestValueUsingShape).first == '6' || huMoments_list.at(indexOfLowestValueUsingShape).first == '9')
			{
				// not good enough with shape analysis
				if (type == "rank")
				{
					cardType.first = classifyTypeWithKnn(resizedROI, kNearest_rank);

					((huMoments_list.at(indexOfLowestValueUsingComparison).first) == (cardType.first)) ? amountOfIncorrectThrowAways++ : amountOfCorrectThrowAways++;
					//std::cout << "Expected: " << static_cast<char>(huMoments_list.at(indexOfLowestValueUsingShape).first) << " with " << lowestValueOfShape << " certainty - actual: "
					//	<< static_cast<char>(cardType.first) << std::endl;
				}
				else
				{
					(type == "red_suit") ? cardType.second = classifyTypeWithKnn(resizedROI, kNearest_red_suit) : cardType.second = classifyTypeWithKnn(resizedROI, kNearest_black_suit);

					((huMoments_list.at(indexOfLowestValueUsingShape).first) == (cardType.second)) ? amountOfIncorrectThrowAways++ : amountOfCorrectThrowAways++;
					//std::cout << "Expected: " << static_cast<char>(huMoments_list.at(indexOfLowestValueUsingComparison).first) << " with " << lowestValuee << " certainty - actual: "
					//	<< static_cast<char>(cardType.second) << std::endl;
				}
			}
			else
			{
				// good enough with shape analysis
				(type == "rank") ? cardType.first = huMoments_list.at(indexOfLowestValueUsingShape).first : cardType.second = huMoments_list.at(indexOfLowestValueUsingShape).first;

				amountOfKnns++;
				//std::cout << "Identified: " << static_cast<char>(huMoments_list.at(indexOfLowestValueUsingComparison).first) << " with " << lowestValuee << " certainty" << std::endl;
			}*/
		}
		type = "black_suit";
		src = cardCharacteristics.second;
	}
	return cardType;
}

int ClassifyCard::classifyTypeUsingComparison(std::vector<std::pair<classifiers, cv::Mat>> &image_list, cv::Mat &resizedROI, int &lowestValue)
{
	cv::Mat diff;
	int lowestIndex = INT_MAX;
	int nonZero;
	for (int i = 0; i < image_list.size(); i++)
	{
		cv::compare(image_list.at(i).second, resizedROI, diff, cv::CMP_NE);
		nonZero = cv::countNonZero(diff);
		if (nonZero < lowestValue)
		{
			lowestIndex = i;
			lowestValue = nonZero;
		}
	}
	return lowestIndex;
}

int ClassifyCard::classifyTypeWithShape(vector<std::pair<classifiers, std::vector<double>>> &list, double  huMomentsA[7], double &lowestValue)
{
	// source: https://github.com/opencv/opencv/blob/master/modules/imgproc/src/matchcontours.cpp
	
	int indexOfLowestValue = 0;
	int indexOfSecondLowestValue = 0;
	double secondLowestValue = DBL_MAX;
	for (int i = 0; i < list.size(); i++)
	{
		int signHuMomentA, signHuMomentB;
		double eps = 1.e-5;
		double result = 0;
		bool anyA = false, anyB = false;

		for (int j = 0; j < 7; j++)
		{
			double ama = fabs(huMomentsA[j]);
			double amb = fabs(list.at(i).second.at(j));

			if (ama > 0)
				anyA = true;
			if (amb > 0)
				anyB = true;

			if (huMomentsA[j] > 0)
				signHuMomentA = 1;
			else if (huMomentsA[j] < 0)
				signHuMomentA = -1;
			else
				signHuMomentA = 0;
			if (list.at(i).second.at(j) > 0)
				signHuMomentB = 1;
			else if (list.at(i).second.at(j) < 0)
				signHuMomentB = -1;
			else
				signHuMomentB = 0;

			if (ama > eps && amb > eps)
			{
				ama = 1. / (signHuMomentA * log10(ama));
				amb = 1. / (signHuMomentB * log10(amb));
				result += fabs(-ama + amb);	// subtract 1/mB from 1/mA
			}
			if (anyA != anyB)
				result = DBL_MAX;
			//If anyA and anyB are both true, the result is correct.
			//If anyA and anyB are both false, the distance is 0, perfect match.
			//If only one is true, then it's a false 0 and return large error.
		}

		if (lowestValue > result)
		{
			if (secondLowestValue > lowestValue)
			{
				secondLowestValue = lowestValue;
				indexOfSecondLowestValue = indexOfLowestValue;
			}
			lowestValue = result;
			indexOfLowestValue = i;
		}
		else if (secondLowestValue > result)
		{
			secondLowestValue = result;
			indexOfSecondLowestValue = i;
		}
	}
	//std::cout << lowestValueOfShape << " " << indexOfLowestValueUsingShape << " " << secondLowestValue << " " << indexOfSecondLowestValue << std::endl;
	if (lowestValue * 1.3 > secondLowestValue)
	{
		lowestValue = DBL_MAX;
	}
	return indexOfLowestValue;
}

void ClassifyCard::generateMoments()
{
	vector<string> rankClassifiersList = { "2", "3", "4", "5", "6", "7", "8", "9", "J", "Q", "K", "A" };
	vector<string> black_suitClassifiersList = { "S", "C" };
	vector<string> red_suitClassifiersList = { "D", "H" };

	for (int i = 0; i < rankClassifiersList.size(); i++)
	{
		Mat src = imread("../GameAnalytics/testImages/" + rankClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}
		cv::Mat grayImg, blurredImg, threshImg;
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		cv::threshold(blurredImg, threshImg, 130, 255, THRESH_BINARY);
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;
		cv::findContours(threshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		double ma[7];
		HuMoments(moments(contours.at(0)), ma);
		std::vector<double> mb(std::begin(ma), std::end(ma));
		std::pair<classifiers, std::vector<double>> pair;
		pair.first = classifiers(char(rankClassifiersList.at(i).at(0)));
		pair.second = mb;
		rankHuMoments.push_back(pair);
	}
	for (int i = 0; i < red_suitClassifiersList.size(); i++)
	{
		Mat src = imread("../GameAnalytics/testImages/" + red_suitClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}

		cv::Mat grayImg, blurredImg, threshImg;
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		cv::threshold(blurredImg, threshImg, 130, 255, THRESH_BINARY);
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;
		cv::findContours(threshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		double ma[7];
		HuMoments(moments(contours.at(0)), ma);
		std::vector<double> mb(std::begin(ma), std::end(ma));
		std::pair<classifiers, std::vector<double>> pair;
		pair.first = classifiers(char(red_suitClassifiersList.at(i).at(0)));
		pair.second = mb;
		red_suitHuMoments.push_back(pair);
	}
	for (int i = 0; i < black_suitClassifiersList.size(); i++)
	{
		Mat src = imread("../GameAnalytics/testImages/" + black_suitClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}

		cv::Mat grayImg, blurredImg, threshImg;
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		cv::threshold(blurredImg, threshImg, 130, 255, THRESH_BINARY);
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;
		cv::findContours(threshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		double ma[7];
		HuMoments(moments(contours.at(0)), ma);
		std::vector<double> mb(std::begin(ma), std::end(ma));
		std::pair<classifiers, std::vector<double>> pair;
		pair.first = classifiers(char(black_suitClassifiersList.at(i).at(0)));
		pair.second = mb;
		black_suitHuMoments.push_back(pair);
	}
}

void ClassifyCard::generateImageVector()
{
	vector<string> rankClassifiersList = { "2", "3", "4", "5", "6", "7", "8", "9", "J", "Q", "K", "A" };
	vector<string> black_suitClassifiersList = { "S", "C" };
	vector<string> red_suitClassifiersList = { "D", "H" };

	for (int i = 0; i < rankClassifiersList.size(); i++)
	{
		Mat src = imread("../GameAnalytics/testImages/" + rankClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}
		cv::Mat grayImg, blurredImg, threshImg;
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		cv::threshold(blurredImg, threshImg, 140, 255, THRESH_BINARY);
		std::pair<classifiers, cv::Mat> pair;
		pair.first = classifiers(char(rankClassifiersList.at(i).at(0)));
		pair.second = threshImg.clone();
		rankImages.push_back(pair);
	}
	for (int i = 0; i < red_suitClassifiersList.size(); i++)
	{
		Mat src = imread("../GameAnalytics/testImages/" + red_suitClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}

		cv::Mat grayImg, blurredImg, threshImg;
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		cv::threshold(blurredImg, threshImg, 140, 255, THRESH_BINARY);
		std::pair<classifiers, cv::Mat> pair;
		pair.first = classifiers(char(red_suitClassifiersList.at(i).at(0)));
		pair.second = threshImg.clone();
		red_suitImages.push_back(pair);
	}
	for (int i = 0; i < black_suitClassifiersList.size(); i++)
	{
		Mat src = imread("../GameAnalytics/testImages/" + black_suitClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}

		cv::Mat grayImg, blurredImg, threshImg;
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		cv::threshold(blurredImg, threshImg, 140, 255, THRESH_BINARY);
		std::pair<classifiers, cv::Mat> pair;
		pair.first = classifiers(char(black_suitClassifiersList.at(i).at(0)));
		pair.second = threshImg.clone();
		black_suitImages.push_back(pair);
	}
}

classifiers ClassifyCard::classifyTypeWithKnn(const Mat & image, const Ptr<ml::KNearest> & kNearest)
{
	Mat src = image.clone();
	Mat ROIFloat;
	src.convertTo(ROIFloat, CV_32FC1);
	Mat ROIFlattenedFloat = ROIFloat.reshape(1, 1);
	Mat CurrentChar(0, 0, CV_32F);
	kNearest->findNearest(ROIFlattenedFloat, 1, CurrentChar);
	float fltCurrentChar = (float)CurrentChar.at<float>(0, 0);
	return classifiers(char(int(fltCurrentChar)));
}

std::pair<Mat, Mat> ClassifyCard::segmentRankAndSuitFromCard(const Mat & aCard)
{
	Mat card;
	if (card.size() != standardCardSize)
	{
		resize(aCard, card, standardCardSize);
	}
	// Get the rank and suit from the resized card
	Rect myRankROI(4, 4, 22, 26);
	Mat rank(card, myRankROI);
	cv::resize(rank, rank, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));
	Rect mySuitROI(4, 31, 22, 20);
	Mat suit(card, mySuitROI);
	cv::resize(suit, suit, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));
	std::pair<Mat, Mat> cardCharacteristics = std::make_pair(rank, suit);
	return cardCharacteristics;
}

void ClassifyCard::getTrainedData(String type)
{
	Mat classificationInts, trainingImagesAsFlattenedFloats;

	FileStorage fsClassifications("../GameAnalytics/knnData/" + type + "_classifications.xml", FileStorage::READ);	// type = rank, black_suit or red_suit
	if (!fsClassifications.isOpened()) {

		std::cout << "Unable to open training classifications file, trying to generate it\n\n";
		Mat trainingImg = imread("../GameAnalytics/knnData/" + type + "TrainingImg.png");

		if (!trainingImg.data)
		{
			std::cout << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}

		generateTrainingData(trainingImg, type);		// If training data hasn't been generated yet
		FileStorage fsClassifications("../GameAnalytics/knnData/" + type + "_classifications.xml", FileStorage::READ);
	}
	if (!fsClassifications.isOpened())
	{
		cout << "Unable to open training classification file again, exiting program" << std::endl;
		exit(EXIT_FAILURE);
	}

	fsClassifications[type + "_classifications"] >> classificationInts;
	fsClassifications.release();

	FileStorage fsTrainingImages("../GameAnalytics/knnData/" + type + "_images.xml", FileStorage::READ);

	if (!fsTrainingImages.isOpened()) {
		cout << "Unable to open training images file, exiting program" << std::endl;
		exit(EXIT_FAILURE);
	}
	fsTrainingImages[type + "_images"] >> trainingImagesAsFlattenedFloats;
	fsTrainingImages.release();

	Ptr<ml::KNearest>  kNearest(ml::KNearest::create());
	kNearest->train(trainingImagesAsFlattenedFloats, cv::ml::ROW_SAMPLE, classificationInts);
	kNearest->save("../GameAnalytics/knnData/trained_" + type + ".yml");
}

void ClassifyCard::generateTrainingData(cv::Mat trainingImage, String outputPreName) {

	// initialize variables
	Mat grayImg, blurredImg, threshImg, threshImgCopy;
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;
	Mat classificationInts, trainingImagesAsFlattenedFloats;
	std::vector<int> intValidChars = { '1', '2', '3', '4', '5', '6', '7', '8', '9', 'J', 'Q', 'K', 'A',	// ranks
		'S', 'C', 'H', 'D' };	// suits (spades, clubs, hearts, diamonds)
	Mat trainingNumbersImg = trainingImage;

	// change the training image to black/white and find the contours of characters
	cvtColor(trainingNumbersImg, grayImg, CV_BGR2GRAY);
	GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
	threshold(blurredImg, threshImg, 130, 255, THRESH_BINARY_INV);
	threshImgCopy = threshImg.clone();	// findcontours modifies the image -> use a cloned image
	findContours(threshImgCopy, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);


	// show each character from the training image and let the user input which character it is
	for (int i = 0; i < contours.size(); i++) {
		if (contourArea(contours[i]) > MIN_CONTOUR_AREA) {
			Rect boundingRect = cv::boundingRect(contours[i]);
			rectangle(trainingNumbersImg, boundingRect, cv::Scalar(0, 0, 255), 2);

			Mat ROI = threshImg(boundingRect);
			Mat ROIResized;
			if (outputPreName == "black_suit" || outputPreName == "red_suit")
			{
				cv::resize(ROI, ROIResized, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));
			}
			else
			{
				cv::resize(ROI, ROIResized, cv::Size(RESIZED_TYPE_WIDTH, RESIZED_TYPE_HEIGHT));
			}			imshow("ROIResized", ROIResized);
			imshow("TrainingsNumbers", trainingNumbersImg);

			int intChar = cv::waitKey(0);	// get user input
			if (intChar == 27) {        // if esc key was pressed
				return;
			}
			else if (find(intValidChars.begin(), intValidChars.end(), intChar) != intValidChars.end()) {
				classificationInts.push_back(intChar);
				Mat imageFloat;
				ROIResized.convertTo(imageFloat, CV_32FC1);
				Mat imageFlattenedFloat = imageFloat.reshape(1, 1);
				trainingImagesAsFlattenedFloats.push_back(imageFlattenedFloat);
				// knn requires flattened float images
			}
		}
	}

	cout << "training complete" << endl;

	// save classifications to xml file

	FileStorage fsClassifications("../GameAnalytics/knnData/" + outputPreName + "_classifications.xml", FileStorage::WRITE);
	if (fsClassifications.isOpened() == false) {
		std::cout << "error, unable to open training classifications file, exiting program\n\n";
		exit(EXIT_FAILURE);
	}

	fsClassifications << outputPreName + "_classifications" << classificationInts;
	fsClassifications.release();

	FileStorage fsTrainingImages("../GameAnalytics/knnData/" + outputPreName + "_images.xml", FileStorage::WRITE);

	if (fsTrainingImages.isOpened() == false) {
		std::cout << "error, unable to open training images file, exiting program\n\n";
		exit(EXIT_FAILURE);
	}

	fsTrainingImages << outputPreName + "_images" << trainingImagesAsFlattenedFloats;
	fsTrainingImages.release();
}

int ClassifyCard::getAmountOfCorrectThrowAways()
{
	return amountOfCorrectThrowAways;
}

int ClassifyCard::getAmountOfIncorrectThrowAways()
{
	return amountOfIncorrectThrowAways;
}

int ClassifyCard::getAmountOfKnns()
{
	return amountOfKnns;
}

std::pair<classifiers, classifiers> ClassifyCard::classifyCardWithKnn(std::pair<Mat, Mat> cardCharacteristics)
{
	Mat temp1, temp2;
	std::pair<classifiers, classifiers> cardType;
	String type = "rank";
	Mat src = cardCharacteristics.first;
	Mat blurredImg, grayImg;
	Ptr<ml::KNearest>  kNearest;
	for (int i = 0; i < 2; i++)
	{
		if (type == "black_suit" || type == "red_suit")
		{
			Mat3b hsv;
			cvtColor(src, hsv, COLOR_BGR2HSV);
			Mat1b mask1, mask2;
			inRange(hsv, Scalar(0, 70, 50), Scalar(10, 255, 255), mask1);
			inRange(hsv, Scalar(170, 70, 50), Scalar(180, 255, 255), mask2);
			Mat1b mask = mask1 | mask2;
			int nonZero = countNonZero(mask);
			if (nonZero > 0)	// red!
			{
				type = "red_suit";
				kNearest = kNearest_red_suit;
			}
			else
			{
				kNearest = kNearest_black_suit;
			}
		}

		// process the src
		cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(3, 3), 0);
		threshold(blurredImg, src, 140, 255, THRESH_BINARY_INV);
		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;
		cv::findContours(src, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

		// Sort and remove objects that are too small
		std::sort(contours.begin(), contours.end(), [](const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });
		if (type == "rank" && contours.size() > 1 && contourArea(contours.at(1), false) > 15.0)
		{
			cardType.first = TEN;
		}
		else
		{
			cv::Mat ROI = src(boundingRect(contours.at(0)));
			cv::Mat resizedROI;
			if (type == "black_suit" || type == "red_suit")
			{
				cv::resize(ROI, resizedROI, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));
			}
			else
			{
				cv::resize(ROI, resizedROI, cv::Size(RESIZED_TYPE_WIDTH, RESIZED_TYPE_HEIGHT));
				kNearest = kNearest_rank;
			}
			cv::GaussianBlur(resizedROI, resizedROI, cv::Size(5, 5), 0);
			threshold(resizedROI, resizedROI, 130, 255, THRESH_BINARY);
			Mat ROIFloat;
			resizedROI.convertTo(ROIFloat, CV_32FC1);
			Mat ROIFlattenedFloat = ROIFloat.reshape(1, 1);
			Mat CurrentChar(0, 0, CV_32F);
			kNearest->findNearest(ROIFlattenedFloat, 1, CurrentChar);
			float fltCurrentChar = (float)CurrentChar.at<float>(0, 0);
			if (type == "black_suit" || type == "red_suit")
			{
				temp2 = resizedROI.clone();
				cardType.second = classifiers(char(int(fltCurrentChar)));
			}
			else
			{
				temp1 = resizedROI.clone();
				cardType.first = classifiers(char(int(fltCurrentChar)));
			}
		}
		type = "black_suit";
		src = cardCharacteristics.second;
	}
	stringstream ss;
	ss << static_cast<char>(cardType.first);
	imwrite("../GameAnalytics/testImages/" + ss.str() + ".png", temp1);
	stringstream ss2;
	ss2 << static_cast<char>(cardType.second);
	imwrite("../GameAnalytics/testImages/" + ss2.str() + ".png", temp2);
	return cardType;
}