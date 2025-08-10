#include <iostream>


#define SQUARE(a) (a)*(a)


enum MEMO_TYPE : unsigned char
{
	MEMO_1      = 0b00001,	// Field could be a 1
	MEMO_2      = 0b00010,	// Field could be a 2
	MEMO_3      = 0b00100,	// Field could be a 3
	MEMO_VOLT   = 0b01000,	// Field could be a volt
	MEMO_CONF	= 0b10000	// Field is confirmed
};


static void Solve(unsigned int uSize, unsigned int vSize, unsigned int* uPoint, unsigned int* vPoint, unsigned int* uVolt, unsigned int* vVolt, unsigned char* memos)
{
	if (uSize && vSize && uPoint && vPoint && uVolt && vVolt)
	{
		//	1. Create possibility lookup arrays
		unsigned char** lookups = new unsigned char*[std::max(uSize, vSize)];	//	Array of arrays with the points of each field for each count of unknown fields
		for (unsigned int i = 0; i < std::max(uSize, vSize); i++)
		{
			unsigned int missingFields = i + 1;	//	Count of fields that are unknown
			unsigned int possibilities = SQUARE(missingFields + 1);	//	Count of all different possibilities at this count of unknown fields

			lookups[i] = new unsigned char[possibilities];
			memset(lookups[i], 0, sizeof(*lookups[i]) * possibilities);

			unsigned int possIndex = 0;	//	Index into the array of possibilities
			for (unsigned int voltCount = 0; voltCount <= missingFields; voltCount++)
			{
				unsigned int pointFields = missingFields - voltCount;	// Count of missing fields that have points

				unsigned char* points = new unsigned char[pointFields];	// Array with the points of each field

				for (unsigned int missingPoints = pointFields * 1; missingPoints <= pointFields * 3; missingPoints++)	// Count of points that are missing across all unknown fields
				{
					if (voltCount)
						lookups[i][possIndex] |= MEMO_VOLT;

					memset(points, 1, sizeof(*points) * pointFields);

					unsigned int pointTotal = pointFields;	// Count of points across all unknown fields for current attempt at finding valid possibilities
					while (true)
					{
						if (pointTotal < missingPoints)
						{
							for (int j = pointFields - 1; j >= 0; j--)
							{
								if (points[j] == 3)
									continue;
								points[j]++;
								break;
							}
						}

						pointTotal = 0;
						unsigned char possibility = 0;	// Possibility of unknown fields
						for (unsigned int j = 0; j < pointFields; j++)
						{
							pointTotal += points[j];
							possibility |= points[j] == 1 ? MEMO_1 : (points[j] == 2 ? MEMO_2 : MEMO_3);
						}

						if (pointTotal < missingPoints)
							continue;

						if (pointTotal == missingPoints)
						{
							lookups[i][possIndex] |= possibility;

							bool foundNewPossibility = false;
							for (unsigned int indexOffset = 1; indexOffset < pointFields; indexOffset++)	// Offset between indices of fields which will be compared
							{
								for (int j = pointFields - 1; j > indexOffset - 1; j--)	//	Index of field which will be compared against field at this index minus the indexOffset
								{
									if (points[j] > points[j - indexOffset] + 1)
									{
										points[j]--;
										points[j - indexOffset]++;
										foundNewPossibility = true;
										break;
									}
								}
								if (foundNewPossibility)
									break;
							}
							if (!foundNewPossibility)
								break;
						}
					}

					possIndex++;
				}

				delete[] points;
			}
		}


		//	2. Iterate over all columns.
		for (unsigned int u = 0; u < uSize; u++)
		{
			unsigned int confFieldCount = 0;
			unsigned int confPointCount = 0;
			unsigned int confVoltCount = 0;

			unsigned int poss1Count = 0;
			unsigned int poss2Count = 0;
			unsigned int poss3Count = 0;
			unsigned int possVoltCount = 0;

			//	2.1 Check how much of each type is already confirmed/still possible in this column.
			for (unsigned int v = 0; v < vSize; v++)
			{
				unsigned int memo = memos[u * vSize + v];

				if (memo & MEMO_CONF)
				{
					//	2.1a If this field is already confirmed, take note what it is.
					confFieldCount++;

					switch (memo & (MEMO_1 | MEMO_2 | MEMO_3 | MEMO_VOLT))
					{
					case MEMO_1:
						confPointCount += 1;
						break;
					case MEMO_2:
						confPointCount += 2;
						break;
					case MEMO_3:
						confPointCount += 3;
						break;
					case MEMO_VOLT:
						confVoltCount++;
						break;
					}
				}
				else
				{
					//	2.1b If this field is still unconfirmed, take note of what it can still be.
					if (memo & MEMO_1)
						poss1Count++;
					if (memo & MEMO_2)
						poss2Count++;
					if (memo & MEMO_3)
						poss3Count++;
					if (memo & MEMO_VOLT)
						possVoltCount;
				}
			}

			unsigned int missingFields = vSize - confFieldCount;
			if (missingFields == 0)
				continue;

			unsigned int missingPoints = uPoint[u] - confPointCount;
			unsigned int missingVolts = uVolt[u] - confVoltCount;


			//	2.2 Do lookup of possibilities
			unsigned int possIndex = 0;	//	Index into the array of possibilities
			for (unsigned int voltCount = 0; voltCount < missingVolts; voltCount++)
			{
				unsigned int pointFields = missingFields - voltCount;
				possIndex += pointFields * 3 - pointFields * 1 + 1;
			}
			possIndex += missingPoints - (missingFields - missingVolts) * 1;

			unsigned char possibility = lookups[missingFields - 1][possIndex];


			//	2.3 Reevaluate each field in this column.
			for (unsigned int v = 0; v < vSize; v++)
			{
				unsigned char memo = memos[u * vSize + v];
				if (memo & MEMO_CONF)
					continue;

				memo &= possibility;

				//	Check if exactly one type is possible -> confirmed field
				if ((memo ^ MEMO_1) == 0 || (memo ^ MEMO_2) == 0 || (memo ^ MEMO_3) == 0 || (memo ^ MEMO_VOLT) == 0)
				{
					memo |= MEMO_CONF;
				}

				memos[u * vSize + v] = memo;
			}
		}


		//	3. Iterate over all rows
		for (unsigned int v = 0; v < vSize; v++)
		{
			unsigned int confFieldCount = 0;
			unsigned int confPointCount = 0;
			unsigned int confVoltCount = 0;

			unsigned int poss1Count = 0;
			unsigned int poss2Count = 0;
			unsigned int poss3Count = 0;
			unsigned int possVoltCount = 0;

			//	3.1 Check how much of each type is already confirmed/still possible in this row.
			for (unsigned int u = 0; u < uSize; u++)
			{
				unsigned int memo = memos[u * vSize + v];

				if (memo & MEMO_CONF)
				{
					//	3.1a If this field is already confirmed, take note what it is.
					confFieldCount++;

					switch (memo & (MEMO_1 | MEMO_2 | MEMO_3 | MEMO_VOLT))
					{
					case MEMO_1:
						confPointCount += 1;
						break;
					case MEMO_2:
						confPointCount += 2;
						break;
					case MEMO_3:
						confPointCount += 3;
						break;
					case MEMO_VOLT:
						confVoltCount++;
						break;
					}
				}
				else
				{
					//	3.1b If this field is still unconfirmed, take note of what it can still be.
					if (memo & MEMO_1)
						poss1Count++;
					if (memo & MEMO_2)
						poss2Count++;
					if (memo & MEMO_3)
						poss3Count++;
					if (memo & MEMO_VOLT)
						possVoltCount;
				}
			}

			unsigned int missingFields = uSize - confFieldCount;
			if (missingFields == 0)
				continue;

			unsigned int missingPoints = vPoint[v] - confPointCount;
			unsigned int missingVolts = vVolt[v] - confVoltCount;


			//	3.2 Do lookup of possibilities
			unsigned int possIndex = 0;	//	Index into the array of possibilities
			for (unsigned int voltCount = 0; voltCount < missingVolts; voltCount++)
			{
				unsigned int pointFields = missingFields - voltCount;
				possIndex += pointFields * 3 - pointFields * 1 + 1;
			}
			possIndex += missingPoints - (missingFields - missingVolts) * 1;

			unsigned char possibility = lookups[missingFields - 1][possIndex];


			//	3.3 Reevaluate each field in this row.
			for (unsigned int u = 0; u < uSize; u++)
			{
				unsigned char memo = memos[u * vSize + v];
				if (memo & MEMO_CONF)
					continue;

				memo &= possibility;

				//	Check if exactly one type is possible -> confirmed field
				if ((memo ^ MEMO_1) == 0 || (memo ^ MEMO_2) == 0 || (memo ^ MEMO_3) == 0 || (memo ^ MEMO_VOLT) == 0)
				{
					memo |= MEMO_CONF;
				}

				memos[u * vSize + v] = memo;
			}
		}


		//	4. Draw field
		std::cout << "\n\n";
		for (unsigned int v = 0; v < vSize; v++)
		{
			for (unsigned int u = 1; u < uSize; u++)
				std::cout << "               |||";
			std::cout << "\n";
			for (unsigned int u = 0; u < uSize; u++)
			{
				std::cout
					<< ((memos[u * vSize + v] & MEMO_1) ? "   1" : "    ")
					<< ((memos[u * vSize + v] & MEMO_2) ? " 2" : "  ")
					<< ((memos[u * vSize + v] & MEMO_3) ? " 3" : "  ")
					<< ((memos[u * vSize + v] & MEMO_VOLT) ? " V" : "  ")
					<< ((memos[u * vSize + v] & MEMO_CONF) ? " C" : "  ");
				if(u < uSize - 1)
					std::cout << "   |||";
			}
			std::cout << "\n";
			for(unsigned int u = 1; u < uSize; u++)
				std::cout << "               |||";
			std::cout << "\n";
			for (unsigned int u = 0; u < uSize - 1; u++)
				if (v < vSize - 1)
					std::cout << "||||||||||||||||||";
			if(v < vSize - 1)
				std:: cout << "|||||||||||||||\n";
		}
		std::cout << "\n\n";


		//	5. Clean-up
		for (unsigned int i = 0; i < std::max(uSize, vSize); i++)
			delete[] lookups[i];
		delete[] lookups;
	}
}

int main()
{
	unsigned int uPoints[5]{ 4, 4, 7, 4, 5 };
	unsigned int vPoints[5]{ 5, 5, 4, 3, 7 };
	unsigned int uVolts[5] { 1, 2, 1, 1, 2 };
	unsigned int vVolts[5] { 1, 2, 2, 2, 0 };

	//  First u=0 -> v=[0,vSize), then u=1 -> v=[0,vSize), up to u=uSize-1 -> v=[0,vSize)
	unsigned char* memos = new unsigned char[5 * 5];
	memset(memos, MEMO_1 | MEMO_2 | MEMO_3 | MEMO_VOLT, sizeof(*memos) * 5 * 5);

	Solve(5, 5, uPoints, vPoints, uVolts, vVolts, memos);

	memos[0 * 5 + 4] = MEMO_1 | MEMO_CONF;
	memos[1 * 5 + 4] = MEMO_2 | MEMO_CONF;
	memos[2 * 5 + 4] = MEMO_1 | MEMO_CONF;
	memos[3 * 5 + 4] = MEMO_1 | MEMO_CONF;
	memos[4 * 5 + 4] = MEMO_2 | MEMO_CONF;

	Solve(5, 5, uPoints, vPoints, uVolts, vVolts, memos);

	delete[] memos;
}
