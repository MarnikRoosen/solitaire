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

std::pair<classifiers, classifiers> ClassifyCard::classifyRankAndSuitOfCard(std::pair<Mat, Mat> cardCharacteristics)
{
	std::pair<classifiers, classifiers> cardType;
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
		Mat grayImg, blurredImg, threshImg, threshImgCopy;
		cvtColor(src, grayImg, COLOR_BGR2GRAY);
		cv::GaussianBlur(grayImg, blurredImg, Size(1, 1), 0);
		threshold(blurredImg, threshImg, 130, 255, THRESH_BINARY_INV);
		threshImgCopy = threshImg.clone();


		vector<vector<Point> > contours;
		vector<Vec4i> hierarchy;

		cv::findContours(threshImgCopy, contours, hierarchy, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
		// Sort and remove objects that are too small
		std::sort(contours.begin(), contours.end(), [](const vector<Point>& c1, const vector<Point>& c2) -> bool { return contourArea(c1, false) > contourArea(c2, false); });
		auto new_end = std::remove_if(contours.begin(), contours.end(), [](const std::vector<cv::Point>& c1) -> bool { return (contourArea(c1, false) < 30.0); });
		contours.erase(new_end, contours.end());
		if (type == "rank" && contours.size() > 1)
		{
			cardType.first = TEN;
		}
		else
		{
			cv::Mat ROI = threshImg(boundingRect(contours[0]));
			cv::Mat ROIResized;

			if (type == "suit")
			{
				cv::resize(ROI, ROIResized, cv::Size(RESIZED_TYPE_HEIGHT, RESIZED_TYPE_HEIGHT));
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
			if (type == "suit")
			{
				cardType.second = classifiers(char(int(fltCurrentChar)));
			}
			else
			{
				cardType.first = classifiers(char(int(fltCurrentChar)));
			}

		}
		type = "suit";
		src = cardCharacteristics.second;
	}
	return cardType;	
}

std::pair<Mat, Mat> ClassifyCard::segmentRankAndSuitFromCard(Mat aCard)
{
	Mat card = aCard.clone();
	if (card.size() != standardCardSize)
	{
		resize(card, card, standardCardSize);
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

void ClassifyCard::getTrainedData(String type, cv::Mat& class_ints, cv::Mat& train_images)
{
	Mat classificationInts, trainingImagesAsFlattenedFloats;
	FileStorage fsClassifications(type + "_classifications.xml", FileStorage::READ);

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