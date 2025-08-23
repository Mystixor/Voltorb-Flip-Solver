#include "Solver.h"

#include <cstring>
#include <iostream>
#include <iomanip>

namespace VF
{
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define SQUARE(a) (a)*(a)

	Solver::Solver(unsigned int rows, unsigned int columns)
		: m_IsLookupInitialized(false),
		m_Columns(columns), m_Rows(rows),
		m_uPoint(new unsigned int[columns] {}), m_vPoint(new unsigned int[rows] {}),
		m_uVolt(new unsigned int[columns] {}), m_vVolt(new unsigned int[rows] {}),
		m_Memos(new unsigned char[columns * rows] {}),
		m_MemosTemp(new unsigned char[columns * rows] {}),
		m_UserConf(new bool[columns * rows] {}),
		m_Lookups(new unsigned char* [max(columns, rows)] {}),
		m_LastUserColumn(-1), m_LastUserRow(-1), m_LastUserMemo(MEMO_CONF)
	{
		ResetMemos();

		m_IsLookupInitialized = m_Columns && m_Rows && m_uPoint && m_vPoint && m_uVolt && m_vVolt && m_MemosTemp && m_Lookups
			&& CreateLookupArrays();
	}

	Solver::~Solver()
	{
		for (unsigned int i = 0; i < max(m_Columns, m_Rows); i++)
		{
			delete[] m_Lookups[i];
		}
		delete[] m_Lookups;

		delete[] m_UserConf;

		delete[] m_MemosTemp;

		delete[] m_vVolt;
		delete[] m_uVolt;

		delete[] m_vPoint;
		delete[] m_uPoint;
	}

	unsigned int Solver::GetColumnCount() const
	{
		return m_Columns;
	}

	unsigned int Solver::GetRowCount() const
	{
		return m_Rows;
	}

	unsigned char Solver::GetMemo(unsigned int column, unsigned int row) const
	{
		return m_Memos[column * m_Rows + row];
	}

	unsigned char Solver::SetMemo(unsigned int column, unsigned int row, MEMO_TYPE memo)
	{
		m_Memos[column * m_Rows + row] = memo | MEMO_CONF;
		m_UserConf[column * m_Rows + row] = true;

		m_LastUserColumn = column;
		m_LastUserRow = row;
		m_LastUserMemo = memo;

		SolveUntilStable();

		return m_Memos[column * m_Rows + row];
	}

	unsigned char Solver::UnsetMemo(unsigned int column, unsigned int row)
	{
		m_Memos[column * m_Rows + row] = MEMO_1 | MEMO_2 | MEMO_3 | MEMO_VOLT;
		m_UserConf[column * m_Rows + row] = false;

		ResetMemos();
		SolveUntilStable();

		return m_Memos[column * m_Rows + row];
	}

	bool Solver::IsMemoUserConfirmed(unsigned int column, unsigned int row) const
	{
		return m_UserConf[column * m_Rows + row];
	}

	bool Solver::SetHints(const unsigned int* uPoint, const unsigned int* vPoint, const unsigned int* uVolt, const unsigned int* vVolt)
	{
		for (unsigned int u = 0; u < m_Columns; u++)
		{
			if (uPoint[u] + uVolt[u] < m_Rows || uPoint[u] > (m_Rows - uVolt[u]) * 3)
				return false;
		}
		for (unsigned int v = 0; v < m_Rows; v++)
		{
			if (vPoint[v] + vVolt[v] < m_Columns || vPoint[v] > (m_Columns - vVolt[v]) * 3)
				return false;
		}

		std::memcpy(m_uPoint, uPoint, sizeof(*m_uPoint) * m_Columns);
		std::memcpy(m_vPoint, vPoint, sizeof(*m_vPoint) * m_Rows);

		std::memcpy(m_uVolt, uVolt, sizeof(*m_uVolt) * m_Columns);
		std::memcpy(m_vVolt, vVolt, sizeof(*m_vVolt) * m_Rows);

		ResetMemos();

		SolveUntilStable();

		return true;
	}

	bool Solver::CreateLookupArrays()
	{
		if (m_Lookups)
		{
			//	Iterate over all possible counts of missing fields
			for (unsigned int i = 0; i < max(m_Columns, m_Rows); i++)
			{
				unsigned int missingFields = i + 1;	//	Count of fields that are unknown
				unsigned int possibilities = SQUARE(missingFields + 1);	//	Count of all different possibilities at this count of unknown fields

				//	Allocate array for all possibilities at specific count of missing fields
				m_Lookups[i] = new unsigned char[possibilities];
				if (m_Lookups[i] == nullptr)
					return false;
				std::memset(m_Lookups[i], 0, sizeof(*m_Lookups[i]) * possibilities);

				unsigned int possIndex = 0;	//	Index into the array of possibilities
				//	Iterate over all possible volt counts
				for (unsigned int voltCount = 0; voltCount <= missingFields; voltCount++)
				{
					unsigned int pointFields = missingFields - voltCount;	// Count of missing fields that have points

					unsigned char* points = new unsigned char[pointFields];	// Array with the points for each field

					//	Iterate over all possible amounts of missing points
					for (unsigned int missingPoints = pointFields * 1; missingPoints <= pointFields * 3; missingPoints++)	// Count of points that are missing across all unknown fields
					{
						if (voltCount)
							m_Lookups[i][possIndex] |= MEMO_VOLT;

						//	All fields start with value 1, giving the lowest possible total point count.
						std::memset(points, 1, sizeof(*points) * pointFields);

#ifdef _DEBUG
						unsigned int possibilityCount = 0;	// Count of different possibilities that were found, useful only for debugging.
#endif
						unsigned int pointTotal = pointFields;	// Count of points across all unknown fields for current attempt at finding valid possibilities
						//	Iterate over all possible point distributions and take note of those possibilities that yield the correct amount of missing points.
						while (true)
						{
							if (pointTotal < missingPoints)
							{
								//	If the point total is less than the amount of missing points, increase the first field from the back that has less than 3 points.
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
							//	Iterate over all fields to update the point total and take note which individual points are used in this arrangement.
							for (unsigned int j = 0; j < pointFields; j++)
							{
								pointTotal += points[j];
								possibility |= points[j] == 1 ? MEMO_1 : (points[j] == 2 ? MEMO_2 : MEMO_3);
							}

							//	If this point total is too low, continue to the next iteration which will increase the point count by 1.
							if (pointTotal < missingPoints)
								continue;

							//	Having arrived here, we know we are in a configuration with the correct amount of points, so we include the possibility in the lookup.
							//	This is done conservatively, meaning for example if this config no longer includes ones, but the previous one did, they are all kept.
							//	In certain cases this leads to a weaker statement when solving than would be possible, and can be improved with a different lookup design.
							m_Lookups[i][possIndex] |= possibility;
#ifdef _DEBUG
							possibilityCount++;
#endif

							//	Search for another possibility
							bool foundNewPossibility = false;	//	Whether a new possibility was found this iteration.

							//	We iterate over all offsets between pairs of 2 fields starting with all pairs of adjacent fields,
							//	in search for a pair of fields that are more than 1 point apart, to redistribute the points,
							//	i.e. (MEMO_1, MEMO_3) --> (MEMO_2, MEMO_2).
							for (unsigned int indexOffset = 1; indexOffset < pointFields; indexOffset++)	// Offset between indices of fields which will be compared
							{
								//	Since the lowest field values are in the front, we start with pairs where the larger field is in the back.
								for (int j = pointFields - 1; j > indexOffset - 1; j--)	//	Index of field which will be compared against field at this index minus the indexOffset
								{
									//	If the field further back is larger by more than 1, decrement its points and increment the other's.
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

#ifdef _DEBUG
						std::cout <<
							"missingFields: " << std::setw(3) << missingFields <<
							", voltCount: " << std::setw(3) << voltCount <<
							", missingPoints: " << std::setw(3) << missingPoints <<
							", possibilityCount: " << std::setw(3) << possibilityCount <<
							", lookup: " <<
							(m_Lookups[i][possIndex] & MEMO_1 ? "1" : " ") <<
							(m_Lookups[i][possIndex] & MEMO_2 ? " 2" : "  ") <<
							(m_Lookups[i][possIndex] & MEMO_3 ? " 3" : "  ") <<
							std::endl;
#endif

						possIndex++;
					}

					delete[] points;
				}
			}

			return true;
		}

		return false;
	}

	void Solver::ResetMemos()
	{
		for (unsigned int u = 0; u < m_Columns; u++)
		{
			for (unsigned int v = 0; v < m_Rows; v++)
			{
				if (!m_UserConf[u * m_Rows + v])
				{
					m_Memos[u * m_Rows + v] = MEMO_1 | MEMO_2 | MEMO_3 | MEMO_VOLT;
					m_MemosTemp[u * m_Rows + v] = MEMO_1 | MEMO_2 | MEMO_3 | MEMO_VOLT;
				}
			}
		}
	}

	void Solver::SolveUntilStable()
	{
		//	Copy the current legal solution into the temporary solution storage.
		memcpy(m_MemosTemp, m_Memos, sizeof(*m_MemosTemp) * m_Columns * m_Rows);

		//	Repeatedly call Solve() and handle the return value.
		while (true)
		{
			switch (Solve())
			{
			case SOLVE_NO_CHANGE:
				CommitTemporarySolution();
				break;
			case SOLVE_CHANGED:
				continue;
			case SOLVE_CONTRADICTION:
				if (m_LastUserColumn != -1 && m_LastUserRow != -1 && m_LastUserMemo != MEMO_CONF)
				{
					//	Reset the memos to what they were before, minus the contradictory option selected by the user.
					memcpy(m_MemosTemp, m_Memos, sizeof(*m_MemosTemp) * m_Columns * m_Rows);
					m_MemosTemp[m_LastUserColumn * m_Rows + m_LastUserRow] = (MEMO_1 | MEMO_2 | MEMO_3 | MEMO_VOLT) ^ m_LastUserMemo;
					m_UserConf[m_LastUserColumn * m_Rows + m_LastUserRow] = false;
				}
				else
				{
					//	A contradiction before even a single button was clicked should not be possible.
					exit(0);
				}
				continue;
			}

			break;
		}
	}

	Solver::SOLVE_RESULT Solver::Solve()
	{
		bool isAnythingChanged = false;

		//	1. Iterate over all columns.
		for (unsigned int u = 0; u < m_Columns; u++)
		{
			unsigned int confFieldCount = 0;
			unsigned int confPointCount = 0;
			unsigned int confVoltCount = 0;

			unsigned int poss1Count = 0;
			unsigned int poss2Count = 0;
			unsigned int poss3Count = 0;
			unsigned int possVoltCount = 0;

			//	1.1 Check how much of each type is already confirmed/still possible in this column.
			for (unsigned int v = 0; v < m_Rows; v++)
			{
				unsigned int memo = m_MemosTemp[u * m_Rows + v];

				if (memo & MEMO_CONF)
				{
					//	1.1a If this field is already confirmed, take note what it is.
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
					//	1.1b If this field is still unconfirmed, take note of what it can still be.
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

			unsigned int missingFields = m_Rows - confFieldCount;
			if (missingFields == 0)
				continue;

			int missingPoints = m_uPoint[u] - confPointCount;
			int missingVolts = m_uVolt[u] - confVoltCount;

			if (missingPoints < 0 || missingVolts < 0)
				return SOLVE_CONTRADICTION;


			//	1.2 Do lookup of possibilities
			unsigned int possIndex = 0;	//	Index into the array of possibilities
			for (unsigned int voltCount = 0; voltCount < missingVolts; voltCount++)
			{
				unsigned int pointFields = missingFields - voltCount;
				possIndex += pointFields * 3 - pointFields * 1 + 1;
			}
			possIndex += missingPoints - (missingFields - missingVolts) * 1;

			unsigned char possibility = m_Lookups[missingFields - 1][possIndex];


			//	1.3 Reevaluate each field in this column.
			for (unsigned int v = 0; v < m_Rows; v++)
			{
				unsigned char memo = m_MemosTemp[u * m_Rows + v];
				if (memo & MEMO_CONF)
					continue;

				memo &= possibility;

				//	Check if exactly one type is possible -> confirmed field
				if ((memo ^ MEMO_1) == 0 || (memo ^ MEMO_2) == 0 || (memo ^ MEMO_3) == 0 || (memo ^ MEMO_VOLT) == 0)
				{
					memo |= MEMO_CONF;
				}

				if (memo != m_MemosTemp[u * m_Rows + v])
				{
					m_MemosTemp[u * m_Rows + v] = memo;
					isAnythingChanged = true;
				}
			}

#ifdef _DEBUG
			PrintBoard();
#endif
		}


		//	2. Iterate over all rows
		for (unsigned int v = 0; v < m_Rows; v++)
		{
			unsigned int confFieldCount = 0;
			unsigned int confPointCount = 0;
			unsigned int confVoltCount = 0;

			unsigned int poss1Count = 0;
			unsigned int poss2Count = 0;
			unsigned int poss3Count = 0;
			unsigned int possVoltCount = 0;

			//	2.1 Check how much of each type is already confirmed/still possible in this row.
			for (unsigned int u = 0; u < m_Columns; u++)
			{
				unsigned int memo = m_MemosTemp[u * m_Rows + v];

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

			unsigned int missingFields = m_Columns - confFieldCount;
			if (missingFields == 0)
				continue;

			int missingPoints = m_vPoint[v] - confPointCount;
			int missingVolts = m_vVolt[v] - confVoltCount;

			if (missingPoints < 0 || missingVolts < 0)
				return SOLVE_CONTRADICTION;


			//	2.2 Do lookup of possibilities
			unsigned int possIndex = 0;	//	Index into the array of possibilities
			for (unsigned int voltCount = 0; voltCount < missingVolts; voltCount++)
			{
				unsigned int pointFields = missingFields - voltCount;
				possIndex += pointFields * 3 - pointFields * 1 + 1;
			}
			possIndex += missingPoints - (missingFields - missingVolts) * 1;

			unsigned char possibility = m_Lookups[missingFields - 1][possIndex];


			//	2.3 Reevaluate each field in this row.
			for (unsigned int u = 0; u < m_Columns; u++)
			{
				unsigned char memo = m_MemosTemp[u * m_Rows + v];
				if (memo & MEMO_CONF)
					continue;

				memo &= possibility;

				//	Check if exactly one type is possible -> confirmed field
				if ((memo ^ MEMO_1) == 0 || (memo ^ MEMO_2) == 0 || (memo ^ MEMO_3) == 0 || (memo ^ MEMO_VOLT) == 0)
				{
					memo |= MEMO_CONF;
				}

				if (memo != m_MemosTemp[u * m_Rows + v])
				{
					m_MemosTemp[u * m_Rows + v] = memo;
					isAnythingChanged = true;
				}
			}

#ifdef _DEBUG
			PrintBoard();
#endif
		}

		return isAnythingChanged ? SOLVE_CHANGED : SOLVE_NO_CHANGE;
	}

	void Solver::CommitTemporarySolution()
	{
		if(m_Columns && m_Rows)
			memcpy(m_Memos, m_MemosTemp, sizeof(*m_Memos) * m_Columns * m_Rows);
	}

	void Solver::PrintBoard() const
	{
		std::cout << "\n\n";
		for (unsigned int v = 0; v < m_Rows; v++)
		{
			for (unsigned int u = 1; u < m_Columns; u++)
				std::cout << "               |||";
			std::cout << "\n";
			for (unsigned int u = 0; u < m_Columns; u++)
			{
				std::cout
					<< ((m_MemosTemp[u * m_Rows + v] & MEMO_1) ? "   1" : "    ")
					<< ((m_MemosTemp[u * m_Rows + v] & MEMO_2) ? " 2" : "  ")
					<< ((m_MemosTemp[u * m_Rows + v] & MEMO_3) ? " 3" : "  ")
					<< ((m_MemosTemp[u * m_Rows + v] & MEMO_VOLT) ? " V" : "  ")
					<< ((m_MemosTemp[u * m_Rows + v] & MEMO_CONF) ? " C" : "  ");
				if (u < m_Columns - 1)
					std::cout << "   |||";
			}
			std::cout << "\n";
			for (unsigned int u = 1; u < m_Columns; u++)
				std::cout << "               |||";
			std::cout << "\n";
			for (unsigned int u = 0; u < m_Columns - 1; u++)
				if (v < m_Rows - 1)
					std::cout << "||||||||||||||||||";
			if (v < m_Rows - 1)
				std::cout << "|||||||||||||||\n";
		}
		std::cout << "\n\n";
	}
}