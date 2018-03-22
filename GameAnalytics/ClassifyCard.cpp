#include "stdafx.h"
#include "ClassifyCard.h"
#include <string>

ClassifyCard::ClassifyCard()
{
	standardCardSize.width = 200;
	standardCardSize.height = 266;
	generateMoments();
}

std::pair<classifiers, classifiers> ClassifyCard::classifyCardUsingShape(std::pair<Mat, Mat> cardCharacteristics)
{
	std::pair<classifiers, classifiers> cardType;
	String type = "rank";
	Mat src = cardCharacteristics.first;
	Mat blurredImg, grayImg;
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
			}
		}
		
		// process the src
		cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		threshold(blurredImg, src, 130, 255, THRESH_BINARY_INV);
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
			vector<std::pair<classifiers, cv::Mat>> list;
			if (type == "black_suit" || type == "red_suit")
			{
				cv::resize(ROI, resizedROI, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));
				list = suitImages;
			}
			else
			{
				cv::resize(ROI, resizedROI, cv::Size(RESIZED_TYPE_WIDTH, RESIZED_TYPE_HEIGHT));
				list = rankImages;
			}
			threshold(resizedROI, resizedROI, 130, 255, THRESH_BINARY);
			cv::findContours(resizedROI, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
			double best = 100;
			int best_int = 0;
			for (int i = 0; i < list.size(); i++)
			{
				cv::Mat grayImg1, blurredImg1, threshImg1;
				cv::Mat src1 = list.at(i).second;
				// process the src
				cvtColor(src1, grayImg1, COLOR_BGR2GRAY);
				cv::GaussianBlur(grayImg1, blurredImg1, Size(1, 1), 0);
				threshold(blurredImg1, threshImg1, 130, 255, THRESH_BINARY);
				vector<vector<Point> > contours1;
				vector<Vec4i> hierarchy;
				cv::findContours(threshImg1, contours1, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
				double ret = matchShapes(contours.at(0), contours1.at(0), CV_CONTOURS_MATCH_I1, 0.0);
				if (best > ret)
				{
					best = ret;
					best_int = i;
				}
			}
			if (best > 0.05)
			{
				//std::cout << static_cast<char>(list.at(best_int).first) << " with " << best << ", not good enough! Trying again w/ knn --- ";
				if (type == "black_suit" || type == "red_suit")
				{
					cardType.second = classifyTypeWithKnn(cardCharacteristics.second, type);
				//	std::cout << "Actual type was " << static_cast<char>(cardType.second) << std::endl;
				}
				else
				{
					cardType.first = classifyTypeWithKnn(cardCharacteristics.first, type);
				//	std::cout << "Actual type was " << static_cast<char>(cardType.first) << std::endl;
				}
			}
			else
			{
				if (type == "black_suit" || type == "red_suit")
				{
					cardType.second = list.at(best_int).first;
				}
				else
				{
					cardType.first = list.at(best_int).first;
				}
			}
		}
		type = "black_suit";
		src = cardCharacteristics.second;
	}
	return cardType;
}

void ClassifyCard::generateMoments()
{
	vector<string> rankClassifiersList = { "2", "3", "4", "5", "6", "7", "8", "9", "J", "Q", "K", "A" };
	vector<string> suitClassifiersList = { "D", "S", "H", "C" };
	for (int i = 0; i < rankClassifiersList.size(); i++)
	{
		std::pair<classifiers, cv::Mat> pair;
		Mat src = imread("../GameAnalytics/testImages/" + rankClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}
		pair.first = classifiers(char(rankClassifiersList.at(i).at(0)));
		pair.second = src.clone();
		//copyMakeBorder(src, pair.second, 1, 1, 1, 1, BORDER_CONSTANT, Scalar(0, 0, 0));
		rankImages.push_back(pair);
	}
	for (int i = 0; i < suitClassifiersList.size(); i++)
	{
		std::pair<classifiers, cv::Mat> pair;
		Mat src = imread("../GameAnalytics/testImages/" + suitClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}
		pair.first = classifiers(char(suitClassifiersList.at(i).at(0)));
		pair.second = src.clone();
		//copyMakeBorder(src, pair.second, 1, 1, 1, 1, BORDER_CONSTANT, Scalar(0, 0, 0));
		suitImages.push_back(pair);
	}
}

std::pair<classifiers, classifiers> ClassifyCard::classifyCardsWithKnn(std::pair<Mat, Mat> cardCharacteristics)
{
	Mat temp1, temp2;
	std::pair<classifiers, classifiers> cardType;
	String type = "rank";
	Mat src = cardCharacteristics.first;
	Mat blurredImg, grayImg;
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
		}
	}
		Mat classificationInts;      // read in classification data
		Mat trainingImagesAsFlattenedFloats;	// read in trained images
		getTrainedData(type, classificationInts, trainingImagesAsFlattenedFloats);

		Ptr<ml::KNearest>  kNearest(ml::KNearest::create());
		kNearest->train(trainingImagesAsFlattenedFloats, cv::ml::ROW_SAMPLE, classificationInts);

		// process the src
		cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		threshold(blurredImg, src, 130, 255, THRESH_BINARY_INV);
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
			}
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

classifiers ClassifyCard::classifyTypeWithKnn(const Mat & image, String type)
{
	Mat src = image.clone();
	Mat blurredImg, grayImg;
	Mat classificationInts;      // read in classification data
	Mat trainingImagesAsFlattenedFloats;	// read in trained images
	getTrainedData(type, classificationInts, trainingImagesAsFlattenedFloats);
	
	Ptr<ml::KNearest>  kNearest(ml::KNearest::create());
	kNearest->train(trainingImagesAsFlattenedFloats, cv::ml::ROW_SAMPLE, classificationInts);

	// process the src
	cvtColor(src, grayImg, COLOR_BGR2GRAY);
	cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
	threshold(blurredImg, src, 130, 255, THRESH_BINARY_INV);
	vector<vector<Point> > contours;
	vector<Vec4i> hierarchy;
	cv::findContours(src, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

	// Sort and remove objects that are too small
	std::sort(contours.begin(), contours.end(), [](const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });
	if (type == "rank" && contours.size() > 1 && contourArea(contours.at(1), false) > 15.0)
	{
		return TEN;
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
		}
		Mat ROIFloat;
		resizedROI.convertTo(ROIFloat, CV_32FC1);
		Mat ROIFlattenedFloat = ROIFloat.reshape(1, 1);
		Mat CurrentChar(0, 0, CV_32F);
		kNearest->findNearest(ROIFlattenedFloat, 1, CurrentChar);
		float fltCurrentChar = (float)CurrentChar.at<float>(0, 0);
		if (type == "black_suit" || type == "red_suit")
		{
			return classifiers(char(int(fltCurrentChar)));
		}
		else
		{
			return classifiers(char(int(fltCurrentChar)));
		}
	}
}

std::pair<Mat, Mat> ClassifyCard::segmentRankAndSuitFromCard(const Mat & aCard)
{
	Mat card;
	if (card.size() != standardCardSize)
	{
		resize(aCard, card, standardCardSize);
	}
	// Get the rank and suit from the resized card
	Rect myRankROI(4, 6, 30, 34);
	Mat rank(card, myRankROI);
	Rect mySuitROI(4, 38, 34, 30);
	Mat suit(card, mySuitROI);
	std::pair<Mat, Mat> cardCharacteristics = std::make_pair(rank, suit);
	return cardCharacteristics;
}

void ClassifyCard::getTrainedData(String type, cv::Mat& class_ints, cv::Mat& train_images)
{
	Mat classificationInts, trainingImagesAsFlattenedFloats;

	FileStorage fsClassifications(type + "_classifications.xml", FileStorage::READ);	// type = rank, black_suit or red_suit
	if (!fsClassifications.isOpened()) {

		std::cout << "Unable to open training classifications file, trying to generate it\n\n";
		Mat trainingImg = imread("../GameAnalytics/trainingImages/" + type + "TrainingImg.png");

		if (!trainingImg.data)
		{
			std::cout << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}

		generateTrainingData(trainingImg, type);		// If training data hasn't been generated yet
		FileStorage fsClassifications(type + "_classifications.xml", FileStorage::READ);
	}
	if (!fsClassifications.isOpened())
	{
		cout << "Unable to open training classification file again, exiting program" << std::endl;
		exit(EXIT_FAILURE);
	}

	fsClassifications[type + "_classifications"] >> classificationInts;
	fsClassifications.release();
	class_ints = classificationInts;

	FileStorage fsTrainingImages(type + "_images.xml", FileStorage::READ);

	if (!fsTrainingImages.isOpened()) {
		cout << "Unable to open training images file, exiting program" << std::endl;
		exit(EXIT_FAILURE);
	}
	fsTrainingImages[type + "_images"] >> trainingImagesAsFlattenedFloats;
	fsTrainingImages.release();
	train_images = trainingImagesAsFlattenedFloats;
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

	FileStorage fsClassifications(outputPreName + "_classifications.xml", FileStorage::WRITE);
	if (fsClassifications.isOpened() == false) {
		std::cout << "error, unable to open training classifications file, exiting program\n\n";
		exit(EXIT_FAILURE);
	}

	fsClassifications << outputPreName + "_classifications" << classificationInts;
	fsClassifications.release();

	FileStorage fsTrainingImages(outputPreName + "_images.xml", FileStorage::WRITE);

	if (fsTrainingImages.isOpened() == false) {
		std::cout << "error, unable to open training images file, exiting program\n\n";
		exit(EXIT_FAILURE);
	}

	fsTrainingImages << outputPreName + "_images" << trainingImagesAsFlattenedFloats;
	fsTrainingImages.release();
}