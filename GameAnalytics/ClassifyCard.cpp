#include "stdafx.h"
#include "ClassifyCard.h"

ClassifyCard::ClassifyCard()
{
	generateImageVector();	// initialize the images used for classification using comparison
	std::vector<string> type = { "rank", "black_suit", "red_suit" };
	for (int i = 0; i < type.size(); i++)
	{
		String name = "../GameAnalytics/knnData/trained_" + type.at(i) + ".yml";	// get the trained data from the knn classifier
		if (!fileExists(name))
		{
			Mat trainingImg = imread("../GameAnalytics/knnData/" + type.at(i) + "TrainingImg.png");
			generateTrainingData(trainingImg, type.at(i));
		}
	}
	kNearest_rank = ml::KNearest::load<ml::KNearest>("../GameAnalytics/knnData/trained_rank.yml");	// load frequently used knn algorithms as member variables
	kNearest_black_suit = ml::KNearest::load<ml::KNearest>("../GameAnalytics/knnData/trained_black_suit.yml");
	kNearest_red_suit = ml::KNearest::load<ml::KNearest>("../GameAnalytics/knnData/trained_red_suit.yml");
}

/****************************************************
 *	INITIALIZATIONS
 ****************************************************/

void ClassifyCard::generateTrainingData(const cv::Mat & trainingImage, const String & outputPreName) {

	// initialize variables
	Mat grayImg, threshImg;
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;
	Mat classificationInts, trainingImagesAsFlattenedFloats;
	std::vector<int> intValidChars = { '1', '2', '3', '4', '5', '6', '7', '8', '9', 'J', 'Q', 'K', 'A',	// ranks
		'S', 'C', 'H', 'D' };	// suits (spades, clubs, hearts, diamonds)
	Mat trainingNumbersImg = trainingImage;

	// change the training image to black/white and find the contours of characters
	cv::cvtColor(trainingNumbersImg, grayImg, CV_BGR2GRAY);
	cv::threshold(grayImg, threshImg, 130, 255, THRESH_BINARY_INV);
	cv::findContours(threshImg, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);	// get each rank/suit from the image, one by one


	// show each character from the training image and let the user input which character it is
	for (int i = 0; i < contours.size(); i++)
	{
		if (contourArea(contours.at(i)) > MIN_CONTOUR_AREA)
		{
			cv::Rect boundingRect = cv::boundingRect(contours[i]);
			cv::rectangle(trainingNumbersImg, boundingRect, cv::Scalar(0, 0, 255), 2);

			Mat ROI = threshImg(boundingRect);	// process each segmented rank/suit before saving it and asking for user input
			Mat ROIResized;
			if (outputPreName == "black_suit" || outputPreName == "red_suit")
			{
				cv::resize(ROI, ROIResized, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));
			}
			else
			{
				cv::resize(ROI, ROIResized, cv::Size(RESIZED_TYPE_WIDTH, RESIZED_TYPE_HEIGHT));
			}
			imshow("ROIResized", ROIResized);
			imshow("TrainingsNumbers", trainingNumbersImg);

			int intChar = cv::waitKey(0);	// get user input
			if (intChar == 27)	// if esc key was pressed
			{
				return;
			}
			else if (find(intValidChars.begin(), intValidChars.end(), intChar) != intValidChars.end())	// check if the user input is valid
			{	
				classificationInts.push_back(intChar);	// push the classified rank/suit to the classification list
				Mat imageFloat;
				ROIResized.convertTo(imageFloat, CV_32FC1);	// convert the image to a binary float image
				Mat imageFlattenedFloat = imageFloat.reshape(1, 1);	// reshape the image to one long line (row after row)
				trainingImagesAsFlattenedFloats.push_back(imageFlattenedFloat);	// push the resulting image to the image list
																				// knn requires flattened float images
			}
		}
	}

	std::cout << "training complete" << endl;

	Ptr<ml::KNearest>  kNearest(ml::KNearest::create());
	kNearest->train(trainingImagesAsFlattenedFloats, cv::ml::ROW_SAMPLE, classificationInts);	// train the knn classifier
	kNearest->save("../GameAnalytics/knnData/trained_" + outputPreName + ".yml");	// store the trained classifier in a YML file for future use
}

void ClassifyCard::generateImageVector()
{
	vector<string> rankClassifiersList = { "2", "3", "4", "5", "6", "7", "8", "9", "J", "Q", "K", "A" };
	vector<string> black_suitClassifiersList = { "S", "C" };
	vector<string> red_suitClassifiersList = { "D", "H" };

	for (int i = 0; i < rankClassifiersList.size(); i++)	// get the images from the map to use them for classification using comparison
	{
		Mat src = imread("../GameAnalytics/compareImages/" + rankClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}
		cv::Mat grayImg, threshImg;	// identical processing as the segmented images for improved robustness
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::threshold(grayImg, threshImg, 140, 255, THRESH_BINARY);
		std::pair<classifiers, cv::Mat> pair;
		pair.first = classifiers(char(rankClassifiersList.at(i).at(0)));
		pair.second = threshImg.clone();
		rankImages.push_back(pair);
	}
	for (int i = 0; i < red_suitClassifiersList.size(); i++)
	{
		Mat src = imread("../GameAnalytics/compareImages/" + red_suitClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}

		cv::Mat grayImg, threshImg;
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::threshold(grayImg, threshImg, 140, 255, THRESH_BINARY);
		std::pair<classifiers, cv::Mat> pair;
		pair.first = classifiers(char(red_suitClassifiersList.at(i).at(0)));
		pair.second = threshImg.clone();
		red_suitImages.push_back(pair);
	}
	for (int i = 0; i < black_suitClassifiersList.size(); i++)
	{
		Mat src = imread("../GameAnalytics/compareImages/" + black_suitClassifiersList.at(i) + ".png");
		if (!src.data)	// check for invalid input
		{
			std::cerr << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}

		cv::Mat grayImg, threshImg;
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::threshold(grayImg, threshImg, 140, 255, THRESH_BINARY);
		std::pair<classifiers, cv::Mat> pair;
		pair.first = classifiers(char(black_suitClassifiersList.at(i).at(0)));
		pair.second = threshImg.clone();
		black_suitImages.push_back(pair);
	}
}

/****************************************************
 *	MAIN FUNCTIONS
 ****************************************************/

std::pair<Mat, Mat> ClassifyCard::segmentRankAndSuitFromCard(const Mat & aCard)
{
	// Get the rank and suit from the resized card
	Mat rank(aCard, myRankROI);
	cv::resize(rank, rank, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));	// resize a first time to increase pixeldensity
	Mat suit(aCard, mySuitROI);
	cv::resize(suit, suit, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));	// resize a first time to increase pixeldensity
	std::pair<Mat, Mat> cardCharacteristics = std::make_pair(rank, suit);	// package as a pair of rank and suit for classification
	return cardCharacteristics;
}

std::pair<classifiers, classifiers> ClassifyCard::classifyCard(std::pair<Mat, Mat> cardCharacteristics)
{
	// initialize variables
	int nonZero;
	std::pair<classifiers, classifiers> cardType;
	cv::Mat src, blurredImg, grayImg, threshImg, resizedBlurredImg, resizedThreshImg;
	std::string type = "rank";	// first classify the rank
	std::vector<std::vector<cv::Point> > contours;
	std::vector<cv::Vec4i> hierarchy;
	src = cardCharacteristics.first;

	for (int i = 0; i < 2; i++)
	{	
		// determine color for the suitclassification
		if (type != "rank")	// type == black_suit or red_suit
		{
			cv::Mat3b hsv;
			cv::Mat1b mask1, mask2, mask;
			cvtColor(cardCharacteristics.first, hsv, COLOR_BGR2HSV);
			inRange(hsv, Scalar(0, 70, 50), Scalar(10, 255, 255), mask1);	// red filter for suit classification
			inRange(hsv, Scalar(170, 70, 50), Scalar(180, 255, 255), mask2);
			mask = mask1 | mask2;
			nonZero = cv::countNonZero(mask);
			if (nonZero > 0)	// if there are red pixels in the image, the suit is red (hearts or diamonds)
			{
				type = "red_suit";
			}
		}
		
		// process the src
		cv::cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(5, 5), 0);
		cv::threshold(blurredImg, threshImg, 150, 255, THRESH_BINARY_INV);
		cv::findContours(threshImg, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

 		if (contours.empty())	// no contour visible, so there is no image - extra verification to avoid potentially missed errors
		{
			cardType.first = EMPTY;
			cardType.second = EMPTY;
			return cardType;
		}

		// sort the contours on contourArea
		std::sort(contours.begin(), contours.end(), [](const vector<Point>& c1, const vector<Point>& c2)
			-> bool { return contourArea(c1, false) > contourArea(c2, false); });


		if (type == "rank" && contours.size() > 1 && contourArea(contours.at(1), false) > 30.0)	// multiple contours and the second contour isn't small (noise)
		{
			cardType.first = TEN;
		}
		else
		{
			cv::Mat ROI, resizedROI;
			ROI = threshImg(boundingRect(contours.at(0))); // extract the biggest contour from the image
			(type == "rank") ?	// resize to standard size (necessary for classification)
				cv::resize(ROI, resizedROI, cv::Size(RESIZED_TYPE_WIDTH, RESIZED_TYPE_HEIGHT)) :	// numbers are naturally smaller in width
				cv::resize(ROI, resizedROI, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));	// suits are rather squared, keep this characteristic
			cv::GaussianBlur(resizedROI, resizedBlurredImg, cv::Size(3, 3), 0);	// used for shape analysis
			cv::threshold(resizedBlurredImg, resizedThreshImg, 140, 255, THRESH_BINARY);


			// COMPARISON METHOD
			if (type == "rank")
			{
				cardType.first = rankImages.at(classifyTypeUsingSubtraction(rankImages, resizedThreshImg)).first;
			}
			else
			{
				(type == "red_suit") ? 
					cardType.second = red_suitImages.at(classifyTypeUsingSubtraction(red_suitImages, resizedThreshImg)).first :
					cardType.second = black_suitImages.at(classifyTypeUsingSubtraction(black_suitImages, resizedThreshImg)).first;
			}

			// KNN METHOD
			
			/*if (type == "rank")
			{
				cardType.first = classifyTypeUsingKnn(resizedROI, kNearest_rank);
			}
			else
			{
				(type == "red_suit") ?
					cardType.second = classifyTypeUsingKnn(resizedROI, kNearest_red_suit) :
					cardType.second = classifyTypeUsingKnn(resizedROI, kNearest_black_suit);
			}*/
		}
		type = "black_suit";	// next classify the suit, start from a black_suit, if it's red, this will get detected at the beginning of the second loop
		src = cardCharacteristics.second;
	}
	return cardType;
}

/****************************************************
 *	CLASSIFICATION METHODS
 ****************************************************/

int ClassifyCard::classifyTypeUsingSubtraction(std::vector<std::pair<classifiers, cv::Mat>> &image_list, cv::Mat &resizedROI)
{
	cv::Mat diff;
	int lowestValue = INT_MAX;
	int lowestIndex = INT_MAX;
	int nonZero;
	for (int i = 0; i < image_list.size(); i++)	// absolute comparison between 2 binary images
	{
		cv::compare(image_list.at(i).second, resizedROI, diff, cv::CMP_NE);
		nonZero = cv::countNonZero(diff);
		if (nonZero < lowestValue)	// the lower the amount of nonzero pixels (parts that don't match), the better the classification
		{
			lowestIndex = i;
			lowestValue = nonZero;
		}
	}
	return lowestIndex;
}

classifiers ClassifyCard::classifyTypeUsingKnn(const Mat & image, const Ptr<ml::KNearest> & kNearest)
{
	cv::Mat ROIFloat, ROIFlattenedFloat;
	image.convertTo(ROIFloat, CV_32FC1);	// converts 8 bit int gray image to binary float image
	ROIFlattenedFloat = ROIFloat.reshape(1, 1);	// reshape the image to 1 line (all rows pasted behind each other)
	cv::Mat CurrentChar(0, 0, CV_32F);	// output array char that corresponds to the best match (nearest neighbor)
	kNearest->findNearest(ROIFlattenedFloat, 3, CurrentChar);	// calculate the best match
	return classifiers(char(CurrentChar.at<float>(0, 0)));	// convert the float to a char, and finally to classifiers to find the closest match
}


