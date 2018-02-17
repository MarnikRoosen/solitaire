// Screen capture source: https://stackoverflow.com/questions/26888605/opencv-capturing-desktop-screen-live
// Mouse events source: https://opencv-srf.blogspot.be/2011/11/mouse-events.html
// Suit-rank detection and training knn algorithm based on: https://github.com/MicrocontrollersAndMore/OpenCV_3_KNN_Character_Recognition_Cpp

#include "stdafx.h"
#include "ClassifyCard.h"

ClassifyCard::ClassifyCard()
{
	standardCardSize.width = 133;
	standardCardSize.height = 177;
}

void ClassifyCard::getTrainedData(String type, cv::Mat& class_ints, cv::Mat& train_images)
{
	Mat classificationInts;      // read in classification data
	FileStorage fsClassifications(type + "_classifications.xml", FileStorage::READ);
	if (fsClassifications.isOpened() == false) {
		std::cout << "error, unable to open training classifications file, trying to generate it\n\n";
		Mat trainingImg = imread("../GameAnalytics/trainingImages/" + type + "TrainingImg.png");
		if (!trainingImg.data)	// check for invalid input
		{
			cout << "Could not open or find the image" << std::endl;
			exit(EXIT_FAILURE);
		}
		generateTrainingData(trainingImg, type);		// If training data hasn't been generated yet
		FileStorage fsClassifications(type + "_classifications.xml", FileStorage::READ);
	}
	if (fsClassifications.isOpened() == false)
	{
		cout << "error, unable to open training classification file again, exiting program" << std::endl;
		exit(EXIT_FAILURE);
	}

	fsClassifications[type + "_classifications"] >> classificationInts;
	fsClassifications.release();
	class_ints = classificationInts;

	Mat trainingImagesAsFlattenedFloats;	// read in trained images
	FileStorage fsTrainingImages(type + "_images.xml", FileStorage::READ);
	if (fsTrainingImages.isOpened() == false) {
		cout << "error, unable to open training images file, exiting program" << std::endl;
		exit(EXIT_FAILURE);
	}
	fsTrainingImages[type + "_images"] >> trainingImagesAsFlattenedFloats;
	fsTrainingImages.release();
	train_images = trainingImagesAsFlattenedFloats;
}

void ClassifyCard::classifyRankAndSuitOfCard(std::pair<Mat, Mat> cardCharacteristics)
{
	String type = "rank";
	Mat src = cardCharacteristics.first;
	for (int i = 0; i < 2; i++)
	{
		Mat classificationInts;      // read in classification data
		Mat trainingImagesAsFlattenedFloats;	// read in trained images
		getTrainedData(type, classificationInts, trainingImagesAsFlattenedFloats);

		Ptr<ml::KNearest>  kNearest(ml::KNearest::create());
		kNearest->train(trainingImagesAsFlattenedFloats, cv::ml::ROW_SAMPLE, classificationInts);

		// process the src
		String card;
		Mat grayImg, blurredImg, threshImg, threshImgCopy;
		cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		threshold(blurredImg, threshImg, 130, 255, THRESH_BINARY_INV);
		threshImgCopy = threshImg.clone();

		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;

		cv::findContours(threshImgCopy, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

		if (type == "rank" && contours.size() > 1)
		{
			card = "10";
		}
		else
		{
			cv::Mat ROI = threshImg(boundingRect(contours[0]));
			cv::Mat ROIResized;

			if (type == "suit")
			{
				cv::resize(ROI, ROIResized, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT), 0, 0, CV_INTER_LINEAR);
			}
			else
			{
				cv::resize(ROI, ROIResized, cv::Size(RESIZED_TYPE_WIDTH, RESIZED_TYPE_HEIGHT));
			}
			Mat ROIFloat;
			ROIResized.convertTo(ROIFloat, CV_32FC1);
			Mat ROIFlattenedFloat = ROIFloat.reshape(1, 1);
			Mat CurrentChar(0, 0, CV_32F);

			kNearest->findNearest(ROIFlattenedFloat, 1, CurrentChar);
			float fltCurrentChar = (float)CurrentChar.at<float>(0, 0);
			card += convertCharToCardName( char(int(fltCurrentChar)) );
		}
		std::cout << card;
		type = "suit";
		src = cardCharacteristics.second;
	}
	
}

String ClassifyCard::convertCharToCardName(char aName)
{
	if (aName == 'C')
	{
		return " of clubs\n";
	}
	else if (aName == 'H')
	{
		return " of hearts\n";
	}
	else if (aName == 'D')
	{
		return " of diamonds\n";
	}
	else if (aName == 'S')
	{
		return " of spades\n";
	}
	if (aName == 'K')
	{
		return "King";
	}
	else if (aName == 'Q')
	{
		return "Queen";
	}
	else if (aName == 'J')
	{
		return "Jack";
	}
	else if (aName == 'A')
	{
		return "Ace";
	}
	else
	{
		stringstream ss;
		string s;
		char c = aName;
		ss << c;
		ss >> s;
		return s;
	}
}

std::pair<Mat, Mat> ClassifyCard::segmentRankAndSuitFromCard(Mat aCard)
{
	Mat card = aCard;
	if (card.size() != standardCardSize)
	{
		resize(aCard, aCard, standardCardSize);
	}

	// Get the rank and suit from the resized card
	Rect myRankROI(2, 4, 25, 23);
	Mat croppedRankRef(card, myRankROI);
	Mat rank;
	croppedRankRef.copyTo(rank);	// Copy the data into new matrix

	Rect mySuitROI(6, 27, 17, 18);
	Mat croppedSuitRef(card, mySuitROI);
	Mat suit;
	croppedSuitRef.copyTo(suit);	// Copy the data into new matrix

	std::pair<Mat, Mat> cardCharacteristics = std::make_pair(rank, suit);
	return cardCharacteristics;
}

Mat ClassifyCard::detectCardFromImage(String cardName)
{
	String filename = cardName;	// load testimage
	Mat src = imread("../GameAnalytics/testImages/" + filename);
	if (!src.data)	// check for invalid input
	{
		cout << "Could not open or find the image" << std::endl;
		exit(EXIT_FAILURE);
	}

	imshow("original image", src);
	Mat grayImg, blurredImg, threshImg;
	vector<vector<Point>> contours;
	vector<Vec4i> hierarchy;

	cvtColor(src, grayImg, COLOR_BGR2GRAY);	// convert the image to gray
	GaussianBlur(grayImg, blurredImg, Size(1, 1), 1000);	// apply gaussian blur to improve card detection
	threshold(blurredImg, threshImg, 130, 255, THRESH_BINARY);	// threshold the image to keep only brighter regions (cards are white)										
	findContours(threshImg, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE, Point(0, 0));	// find all the contours using the thresholded image

	// find the largest contour
	int largest_area = 0;
	int largest_contour_index = 0;
	Rect bounding_rect;

	for (int i = 0; i < contours.size(); i++) // Iterate through each contour. 
	{
		int a = contourArea(contours[i], false);  // Find the area of contour
		if (a > largest_area) {
			largest_area = a;
			largest_contour_index = i;                // Store the index of largest contour
			bounding_rect = boundingRect(contours[i]); // Find the bounding rectangle for biggest contour
		}
	}

	// Get only the top card
	Mat card = Mat(src, bounding_rect).clone();
	Size cardSize = card.size();
	double topCardHeight = cardSize.width * 1.33;
	Rect myROI(0, cardSize.height - topCardHeight, cardSize.width, topCardHeight);
	Mat croppedRef(card, myROI);
	Mat topCard;
	croppedRef.copyTo(topCard);	// Copy the data into new matrix
	
	imshow("topCard", topCard);
	waitKey(0);
	return topCard;
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
			if (outputPreName == "suit")
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
