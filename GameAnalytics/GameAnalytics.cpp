#include "stdafx.h"
#include "GameAnalytics.h"

int main(int argc, char** argv)
{
	GameAnalytics ga;
}

GameAnalytics::GameAnalytics()
{
	PlayingBoard playingBoard;
	ClassifyCard classifyCard;
	Mat src;
	classifiedCardsFromPlayingBoard.reserve(12);
	hwnd = FindWindow(NULL, L"Microsoft Solitaire Collection - Firefox Developer Edition");
	if (hwnd == NULL)
	{
		std::cout << "Cant find window" << std::endl;
		exit(EXIT_FAILURE);
	}

	//ClicksHooks::Instance().InstallHook();
	int i = 0;
	startOfGame = Clock::now();
	bool endOfGameFlag = false;
	while (key != 27 && !endOfGameFlag)	//key = 27 -> error
	{
		src = waitForStableImage();
		playingBoard.findCardsFromBoardImage(src); // -> average 38ms
			
		switch (playingBoard.getState())
		{
		case outOfMoves:
		{
			handleOutOfMoves();
			endOfGameFlag = true;
			break;
		}
			
		case playing:
			handlePlayingState(playingBoard, classifyCard);
			break;
		default:
			handlePlayingState(playingBoard, classifyCard);
			break;
		}	

		if (i == 1000)
		{
			std::chrono::time_point<std::chrono::steady_clock> test1 = Clock::now();
			std::cout << std::chrono::duration_cast<std::chrono::nanoseconds>(test1 - startOfGame).count() << std::endl;
		}
		i++;
		
	}

	ClicksHooks::Instance().UninstallHook();
}

void GameAnalytics::handleOutOfMoves()
{
	std::cout << "Out of moves" << std::endl;
	std::chrono::time_point<std::chrono::steady_clock> endOfGame = Clock::now();
	std::cout << "Game duration: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endOfGame - startOfGame).count() / 1000000 << " ms" << std::endl;
	Sleep(8000);
	return;
}

void GameAnalytics::handlePlayingState(PlayingBoard &playingBoard, ClassifyCard &classifyCard)
{
	extractedImagesFromPlayingBoard = playingBoard.getCards();
	convertImagesToClassifiedCards(classifyCard);	// -> average d133ms and 550ms

	if (init)
	{
		initializePlayingBoard(classifiedCardsFromPlayingBoard);
		init = false;
	}
	else
	{
		updateBoard(classifiedCardsFromPlayingBoard);
	}

	if (indexOfSelectedCard == -1 && playingBoard.getSelectedCard() != -1)
	{
		//std::cout << "Card at index " << playingBoard.getSelectedCard() << " was just selected!" << std::endl;
		//std::cout << "That is card: " << static_cast<char>(classifiedCardsFromPlayingBoard.at(playingBoard.getSelectedCard()).first) << " "
		//	<< static_cast<char>(classifiedCardsFromPlayingBoard.at(playingBoard.getSelectedCard()).second) << std::endl;
		indexOfSelectedCard = playingBoard.getSelectedCard();
	}
	else if (indexOfSelectedCard != -1 && playingBoard.getSelectedCard() != -1 && indexOfSelectedCard != playingBoard.getSelectedCard())
	{
		char prevSuit = static_cast<char>(classifiedCardsFromPlayingBoard.at(indexOfSelectedCard).second);
		char newSuit = static_cast<char>(classifiedCardsFromPlayingBoard.at(playingBoard.getSelectedCard()).second);
		char prevRank = static_cast<char>(classifiedCardsFromPlayingBoard.at(indexOfSelectedCard).first);
		char newRank = static_cast<char>(classifiedCardsFromPlayingBoard.at(playingBoard.getSelectedCard()).first);
		if (((prevSuit == 'H' || prevSuit == 'D') && (newSuit == 'H' || newSuit == 'D'))
			|| ((prevSuit == 'S' || prevSuit == 'C') && (newSuit == 'S' || newSuit == 'C')))
		{
			std::cout << "Incompatible suit! " << prevSuit << " isn't compatible with " << newSuit << std::endl;
		}
		else
		{
			std::cout << "Incompatible rank! " << prevRank << " isn't compatible with " << newRank << std::endl;
		}
		indexOfSelectedCard = playingBoard.getSelectedCard();
		//std::cout << "Card at index " << indexOfSelectedCard << " is now selected!" << std::endl;
	}
	else if (indexOfSelectedCard != -1 && playingBoard.getSelectedCard() == -1)
	{
		//std::cout << "Card at index " << indexOfSelectedCard << " was just deselected!" << std::endl;
		indexOfSelectedCard = -1;
	}

	return;
}

void GameAnalytics::convertImagesToClassifiedCards(ClassifyCard & cc)
{
	classifiedCardsFromPlayingBoard.clear();
	for_each(extractedImagesFromPlayingBoard.begin(), extractedImagesFromPlayingBoard.end(), [&cc, this](cv::Mat mat) {
		if (mat.empty())
		{
			cardType.first = EMPTY;
			cardType.second = EMPTY;
		}
		else
		{
			cardCharacteristics = cc.segmentRankAndSuitFromCard(mat);
			cardType = cc.classifyCard(cardCharacteristics);


			//cardType = cc.classifyCardsWithKnn(cardCharacteristics);

		}
		classifiedCardsFromPlayingBoard.push_back(cardType);
	});
}

cv::Mat GameAnalytics::waitForStableImage()	// -> average 112ms for non-updated screen
{
	Mat src1, graySrc1, graySrc2, diff;
	do {
		src1 = hwnd2mat(hwnd);
		cvtColor(src1, graySrc1, COLOR_BGR2GRAY);
		waitKey(100);
		graySrc2 = hwnd2mat(hwnd);
		cvtColor(graySrc2, graySrc2, COLOR_BGR2GRAY);
		cv::compare(graySrc1, graySrc2, diff, cv::CMP_NE);
	} while (cv::countNonZero(diff) != 0);
	return src1;
}

void GameAnalytics::initializePlayingBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard)
{
	playingBoard.resize(12);
	for (int i = 0; i < 7; i++)
	{
		cardLocation startupLocation;
		if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY || classifiedCardsFromPlayingBoard.at(i).second != EMPTY)
		{
			startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(i));
		}
		startupLocation.unknownCards = i;
		playingBoard.at(i) = startupLocation;
	}
	cardLocation startupLocation;
	if (classifiedCardsFromPlayingBoard.at(7).first != EMPTY || classifiedCardsFromPlayingBoard.at(7).second != EMPTY)
	{
		startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(7));
	}
	startupLocation.unknownCards = 24;
	playingBoard.at(7) = startupLocation;
	for (int i = 8; i < playingBoard.size(); i++)
	{
		cardLocation startupLocation;
		if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY || classifiedCardsFromPlayingBoard.at(i).second != EMPTY)
		{
			startupLocation.knownCards.push_back(classifiedCardsFromPlayingBoard.at(i));
		}
		startupLocation.unknownCards = 0;
		playingBoard.at(i) = startupLocation;
	}
	printPlayingBoardState();
}

void GameAnalytics::updateBoard(const std::vector<std::pair<classifiers, classifiers>> & classifiedCardsFromPlayingBoard)
{	
	int changedIndex1 = -1, changedIndex2 = -1;
	findChangedCardLocations(classifiedCardsFromPlayingBoard, changedIndex1, changedIndex2);

	// only one card location changed = shuffled deck
	if (changedIndex1 == 7 && changedIndex2 == -1)
	{
		updateDeck(changedIndex1, classifiedCardsFromPlayingBoard);
		return;
	}


	// card move from deck to board
	if (changedIndex1 == 7 || changedIndex2 == 7)
	{
		int tempIndex;
		changedIndex1 == 7 ? (tempIndex = changedIndex2) : (tempIndex = changedIndex1);
		auto result = std::find(
			playingBoard.at(7).knownCards.begin(),
			playingBoard.at(7).knownCards.end(),
			classifiedCardsFromPlayingBoard.at(tempIndex));
		if (result != playingBoard.at(7).knownCards.end())
		{
			playingBoard.at(7).knownCards.erase(result);
			playingBoard.at(tempIndex).knownCards.push_back(classifiedCardsFromPlayingBoard.at(tempIndex));
			printPlayingBoardState();
			return;
		}
	}

	// card move between tableau and foundations
	if (changedIndex1 != -1 && changedIndex2 != -1)
	{
		if (cardMoveBetweenTableauAndFoundations(changedIndex1, classifiedCardsFromPlayingBoard, changedIndex2))
		{
			printPlayingBoardState();
			return;
		}
	}

	// error with previously detected card
	if (changedIndex1 != -1 && changedIndex1 != 7 && changedIndex2 == -1)
	{
		std::cout << "Error with previous board state! Card at index " << changedIndex1 << " should have been: " <<
			static_cast<char>(classifiedCardsFromPlayingBoard.at(changedIndex1).first) << static_cast<char>(classifiedCardsFromPlayingBoard.at(changedIndex1).second) << std::endl;
		if (!playingBoard.at(changedIndex1).knownCards.empty())
		{
			playingBoard.at(changedIndex1).knownCards.pop_back();
		}
		else if (classifiedCardsFromPlayingBoard.at(changedIndex1).first == EMPTY)
		{
			return;
		}
		else
		{
			--playingBoard.at(changedIndex1).unknownCards;
		}
		playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
		printPlayingBoardState();
	}
}

bool GameAnalytics::cardMoveBetweenTableauAndFoundations(int changedIndex1, const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int changedIndex2)
{
	auto inList1 = std::find(playingBoard.at(changedIndex1).knownCards.begin(), playingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
	auto inList2 = std::find(playingBoard.at(changedIndex2).knownCards.begin(), playingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
	if (inList1 != playingBoard.at(changedIndex1).knownCards.end() && inList2 == playingBoard.at(changedIndex2).knownCards.end())
	{
		inList1++;
		playingBoard.at(changedIndex2).knownCards.insert(
			playingBoard.at(changedIndex2).knownCards.end(),
			inList1,
			playingBoard.at(changedIndex1).knownCards.end());
		playingBoard.at(changedIndex1).knownCards.erase(
			inList1,
			playingBoard.at(changedIndex1).knownCards.end());
		return true;
	}
	if (inList1 == playingBoard.at(changedIndex1).knownCards.end() && inList2 != playingBoard.at(changedIndex2).knownCards.end())
	{
		inList2++;
		playingBoard.at(changedIndex1).knownCards.insert(
			playingBoard.at(changedIndex1).knownCards.end(),
			inList2,
			playingBoard.at(changedIndex2).knownCards.end());
		playingBoard.at(changedIndex2).knownCards.erase(
			inList2,
			playingBoard.at(changedIndex2).knownCards.end());
		return true;
	}
	if (inList1 == playingBoard.at(changedIndex1).knownCards.end() && inList2 == playingBoard.at(changedIndex2).knownCards.end())
	{
		auto inList1 = std::find(playingBoard.at(changedIndex1).knownCards.begin(), playingBoard.at(changedIndex1).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex2));
		auto inList2 = std::find(playingBoard.at(changedIndex2).knownCards.begin(), playingBoard.at(changedIndex2).knownCards.end(), classifiedCardsFromPlayingBoard.at(changedIndex1));
		if (inList1 != playingBoard.at(changedIndex1).knownCards.end() && inList2 == playingBoard.at(changedIndex2).knownCards.end())
		{
			playingBoard.at(changedIndex2).knownCards.insert(
				playingBoard.at(changedIndex2).knownCards.end(),
				playingBoard.at(changedIndex1).knownCards.begin(),
				playingBoard.at(changedIndex1).knownCards.end());
			playingBoard.at(changedIndex1).knownCards.clear();
			if (classifiedCardsFromPlayingBoard.at(changedIndex1).first != EMPTY)
			{
				playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
				--playingBoard.at(changedIndex1).unknownCards;
			}
			return true;
		}
		if (inList1 == playingBoard.at(changedIndex1).knownCards.end() && inList2 != playingBoard.at(changedIndex2).knownCards.end())
		{
			playingBoard.at(changedIndex1).knownCards.insert(
				playingBoard.at(changedIndex1).knownCards.end(),
				playingBoard.at(changedIndex2).knownCards.begin(),
				playingBoard.at(changedIndex2).knownCards.end());
			playingBoard.at(changedIndex2).knownCards.clear();
			if (classifiedCardsFromPlayingBoard.at(changedIndex2).first != EMPTY)
			{
				playingBoard.at(changedIndex2).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex2));
				--playingBoard.at(changedIndex2).unknownCards;
			}
			return true;
		}
	}
	return false;
}

void GameAnalytics::findChangedCardLocations(const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard, int &changedIndex1, int &changedIndex2)
{
	for (int i = 0; i < playingBoard.size(); i++)
	{
		if (playingBoard.at(i).knownCards.empty())	// adding card to an empty location
		{
			if (classifiedCardsFromPlayingBoard.at(i).first != EMPTY)
			{
				changedIndex1 == -1 ? (changedIndex1 = i) : (changedIndex2 = i);
			}
		}
		else if (i == 7)
		{
			if (playingBoard.at(7).knownCards.back() != classifiedCardsFromPlayingBoard.at(7) && classifiedCardsFromPlayingBoard.at(7).first != EMPTY)
			{
				changedIndex1 == -1 ? (changedIndex1 = 7) : (changedIndex2 = 7);
			}
		}
		else
		{
			if (playingBoard.at(i).knownCards.back() != classifiedCardsFromPlayingBoard.at(i))
			{
				changedIndex1 == -1 ? (changedIndex1 = i) : (changedIndex2 = i);
			}
		}
		if (changedIndex2 != -1) { return; }
	}
}

void GameAnalytics::updateDeck(int changedIndex1, const std::vector<std::pair<classifiers, classifiers>> &classifiedCardsFromPlayingBoard)
{
	auto result = std::find(
		playingBoard.at(changedIndex1).knownCards.begin(),
		playingBoard.at(changedIndex1).knownCards.end(),
		classifiedCardsFromPlayingBoard.at(changedIndex1));
	if (result == playingBoard.at(changedIndex1).knownCards.end() && classifiedCardsFromPlayingBoard.at(changedIndex1).first != EMPTY)
	{
		playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
		--playingBoard.at(changedIndex1).unknownCards;
		printPlayingBoardState();
	}
	else
	{
		playingBoard.at(changedIndex1).knownCards.erase(result);
		playingBoard.at(changedIndex1).knownCards.push_back(classifiedCardsFromPlayingBoard.at(changedIndex1));
		printPlayingBoardState();
	}
}

void GameAnalytics::printPlayingBoardState()
{
	std::cout << "Deck: ";
	if (playingBoard.at(7).knownCards.empty())
	{
		std::cout << "// ";
	}
	for (int i = 0; i < playingBoard.at(7).knownCards.size(); i++)
	{
		std::cout << static_cast<char>(playingBoard.at(7).knownCards.at(i).first) << static_cast<char>(playingBoard.at(7).knownCards.at(i).second) << " ";
	}
	std::cout << "     Hidden cards = " << playingBoard.at(7).unknownCards << std::endl;

	std::cout << "Solved cards: " << std::endl;
	for (int i = 8; i < playingBoard.size(); i++)
	{
		std::cout << "   Position " << i - 8 << ": ";
		if (playingBoard.at(i).knownCards.empty())
		{
			std::cout << "// ";
		}
		for (int j = 0; j < playingBoard.at(i).knownCards.size(); j++)
		{
			std::cout << static_cast<char>(playingBoard.at(i).knownCards.at(j).first) << static_cast<char>(playingBoard.at(i).knownCards.at(j).second) << " ";
		}
		std::cout << "     Hidden cards = " << playingBoard.at(i).unknownCards << std::endl;
	}

	std::cout << "Bottom cards: " << std::endl;
	for (int i = 0; i < 7; i++)
	{
		std::cout << "   Position " << i << ": ";
		if (playingBoard.at(i).knownCards.empty())
		{
			std::cout << "// ";
		}
		for (int j = 0; j < playingBoard.at(i).knownCards.size(); j++)
		{
			std::cout << static_cast<char>(playingBoard.at(i).knownCards.at(j).first) << static_cast<char>(playingBoard.at(i).knownCards.at(j).second) << " ";
		}
		std::cout << "     Hidden cards = " << playingBoard.at(i).unknownCards << std::endl;
	}

	auto averageThinkTime2 = Clock::now();
	averageThinkDurations.push_back(std::chrono::duration_cast<std::chrono::milliseconds>(averageThinkTime2 - startOfGame).count());
	startOfGame = Clock::now();
	std::cout << "Last move thinktime = " << averageThinkDurations.back() << "ms" << std::endl;
	std::cout << "Average thinktime = " << std::accumulate(averageThinkDurations.begin(), averageThinkDurations.end(), 0) / averageThinkDurations.size() << "ms" << std::endl;
	std::cout << "Amount of moves = " << averageThinkDurations.size() << " moves" << std::endl;
	std::cout << std::endl;
}

Mat GameAnalytics::hwnd2mat(const HWND & hwnd)	//Mat = n-dimensional dense array class, HWND = handle for desktop window
{
	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
	HDC hwindowDC, hwindowCompatibleDC;

	int height, width, srcheight, srcwidth;
	HBITMAP hbwindow;
	Mat src;
	BITMAPINFOHEADER  bi;

	hwindowDC = GetDC(hwnd);	//get device context
	hwindowCompatibleDC = CreateCompatibleDC(hwindowDC);	//creates a compatible memory device context for the device
	SetStretchBltMode(hwindowCompatibleDC, COLORONCOLOR);	//set bitmap stretching mode, color on color deletes all eliminated lines of pixels
	RECT windowsize;    // get the height and width of the screen
	GetClientRect(hwnd, &windowsize);	//get coordinates of clients window
	srcheight = windowsize.bottom;
	srcwidth = windowsize.right;
	//srcheight = 768;
	//srcwidth = 1366;
	height = srcheight * 1;  //possibility to resize the client window screen
	width = srcwidth * 1;
	src.create(height, width, CV_8UC4);	//creates matrix with a given height and width, CV_ 8 unsigned 4 (color)channels

										// create a bitmap
	hbwindow = CreateCompatibleBitmap(hwindowDC, width, height);
	bi.biSize = sizeof(BITMAPINFOHEADER);    //http://msdn.microsoft.com/en-us/library/windows/window/dd183402%28v=vs.85%29.aspx
	bi.biWidth = width;
	bi.biHeight = -height;  //origin of the source is the top left corner, height is 'negative'
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bi.biCompression = BI_RGB;
	bi.biSizeImage = 0;
	bi.biXPelsPerMeter = 0;
	bi.biYPelsPerMeter = 0;
	bi.biClrUsed = 0;
	bi.biClrImportant = 0;

	// use the previously created device context with the bitmap
	SelectObject(hwindowCompatibleDC, hbwindow);
	//PrintWindow(hwnd, hwindowCompatibleDC, PW_CLIENTONLY);

	// copy from the window device context to the bitmap device context
	StretchBlt(hwindowCompatibleDC, 0, 0, width, height, hwindowDC, 0, 0, srcwidth, srcheight, SRCCOPY); //change SRCCOPY to NOTSRCCOPY for wacky colors !
	GetDIBits(hwindowCompatibleDC, hbwindow, 0, height, src.data, (BITMAPINFO *)&bi, DIB_RGB_COLORS);  //copy from hwindowCompatibleDC to hbwindow

																									   // avoid memory leak
	DeleteObject(hbwindow);
	DeleteDC(hwindowCompatibleDC);
	ReleaseDC(hwnd, hwindowDC);

	return src;
}