#include "Solver.h"

#include <cstring>
#include <iostream>
#include <iomanip>

namespace VF
{
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define min(a, b) (((a) > (b)) ? (b) : (a))
#define SQUARE(a) (a)*(a)

#define MAX_DIM 255U

	Solver::Solver(unsigned int rows, unsigned int columns)
		: m_IsLookupInitialized(false),
		m_Columns(min(columns, MAX_DIM)), m_Rows(min(rows, MAX_DIM)),
		m_uPoint(new unsigned int[m_Columns] {}), m_vPoint(new unsigned int[m_Rows] {}),
		m_uVolt(new unsigned int[m_Columns] {}), m_vVolt(new unsigned int[m_Rows] {}),
		m_Memos(new unsigned char[m_Columns * m_Rows] {}),
		m_MemosTemp(new unsigned char[m_Columns * m_Rows] {}),
		m_PossibilitiesTempIn(new unsigned char[max(m_Columns, m_Rows)] {}),
		m_PossibilitiesTempOut(new unsigned char[max(m_Columns, m_Rows)] {}),
		m_LookupTemp(new MEMO_TYPE[max(m_Columns, m_Rows)] {}),
		m_UserConf(new bool[m_Columns * m_Rows] {}),
		m_LookupCounts(new unsigned int* [max(m_Columns, m_Rows)] {}),
		m_Lookups(new MemoCounts** [max(m_Columns, m_Rows)] {}),
		m_LastUserColumn(-1), m_LastUserRow(-1), m_LastUserMemo(MEMO_1 | MEMO_2 | MEMO_3 | MEMO_VOLT)
	{
		if (columns > MAX_DIM || rows > MAX_DIM)
			std::cout << "Maximum dimensions exceeded. Falling back to a size of " << MAX_DIM << "." << std::endl;

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

		try
		{
			SolveUntilStable();
		}
		catch (int e)
		{
			std::cout << "[ERROR]\tInvalid hints, board not solvable." << std::endl;
			ResetMemos();
			return false;
		}

		return true;
	}

	bool Solver::CreateLookupArrays()
	{
		if (m_LookupCounts && m_Lookups)
		{
			//	Iterate over all possible counts of missing fields
			for (unsigned int i = 0; i < max(m_Columns, m_Rows); i++)
			{
				unsigned int missingFields = i + 1;	//	Count of fields that are unknown
				unsigned int possibilities = SQUARE(missingFields + 1);	//	Count of all different possibilities at this count of unknown fields

				//	Allocate array for arrays of all possibilities at specific count of missing fields
				m_LookupCounts[i] = new unsigned int[possibilities] {};
				m_Lookups[i] = new MemoCounts*[possibilities] {};
				if (m_LookupCounts[i] == nullptr || m_Lookups[i] == nullptr)
					return false;

				unsigned int possIndex = 0;	//	Index into the array of possibilities
				//	Iterate over all possible volt counts
				for (unsigned int voltCount = 0; voltCount <= missingFields; voltCount++)
				{
					unsigned int pointFields = missingFields - voltCount;	// Count of missing fields that have points

					unsigned char* points = new unsigned char[pointFields];	// Array with the points for each field

					//	Iterate over all possible amounts of missing points
					for (unsigned int missingPoints = pointFields * 1; missingPoints <= pointFields * 3; missingPoints++)	// Count of points that are missing across all unknown fields
					{
						unsigned int possibilityCount = min(missingPoints - pointFields * 1, pointFields * 3 - missingPoints) / 2 + 1;	// Count of different possibilities
						m_LookupCounts[i][possIndex] = possibilityCount;
						m_Lookups[i][possIndex] = new MemoCounts[possibilityCount] {};

						for (unsigned int j = 0; j < possibilityCount; j++)
							m_Lookups[i][possIndex][j].memoV = voltCount;

						//	All fields start with value 1, giving the lowest possible total point count.
						std::memset(points, 1, sizeof(*points) * pointFields);

						unsigned int foundPossCount = 0;		// Count of how many different possibilities were already found
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
							MemoCounts possibility { .memo1 = 0, .memo2 = 0, .memo3 = 0, .memoV = (unsigned char)voltCount };	// Possibility of unknown fields
							//	Iterate over all fields to update the point total and take note which individual points are used in this arrangement.
							for (unsigned int j = 0; j < pointFields; j++)
							{
								pointTotal += points[j];
								switch (points[j])
								{
								case 1: possibility.memo1++; break;
								case 2: possibility.memo2++; break;
								case 3: possibility.memo3++; break;
								}
							}

							//	If this point total is too low, continue to the next iteration which will increase the point count by 1.
							if (pointTotal < missingPoints)
								continue;

							//	Having arrived here, we know we are in a configuration with the correct amount of points, so we include the possibility in the lookup.
							//	This is done conservatively, meaning for example if this config no longer includes ones, but the previous one did, they are all kept.
							//	In certain cases this leads to a weaker statement when solving than would be possible, and can be improved with a different lookup design.
							m_Lookups[i][possIndex][foundPossCount] = possibility;
							foundPossCount++;

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
							", lookup:";
						for (unsigned int j = 0; j < possibilityCount; j++)
						{
							for (unsigned int j1 = 0; j1 < m_Lookups[i][possIndex][j].memo1; j1++)
								std::cout << " 1";
							for (unsigned int j2 = 0; j2 < m_Lookups[i][possIndex][j].memo2; j2++)
								std::cout << " 2";
							for (unsigned int j3 = 0; j3 < m_Lookups[i][possIndex][j].memo3; j3++)
								std::cout << " 3";
							for (unsigned int jV = 0; jV < voltCount; jV++)
								std::cout << " V";
							if (j + 1 < possibilityCount)
								std::cout << ",";
						}
						std::cout << std::endl;
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

		//	Repeatedly call SolveAll() and handle the return value.
		while (true)
		{
			switch (SolveAll())
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
					//	A contradiction before even a single button was clicked means the board is not solvable.
					break;
				}
				continue;
			}

			break;
		}
	}

	bool Solver::SolveLookupRecurse(unsigned int level, unsigned int elemCount)
	{
		bool foundLegalSolution = false;

		for (unsigned int j = 0; j < level; j++)
		{
			if(level > 2)
				foundLegalSolution |= SolveLookupRecurse(level - 1, elemCount);
			else
			{
				//	level == 1
				bool isLegal = true;
				for (unsigned int j = 0; j < elemCount; j++)
				{
					if (!(m_PossibilitiesTempIn[j] & m_LookupTemp[j]))
					{
						isLegal = false;
#ifdef _DEBUG
						break;
					}
					else
						isLegal = isLegal;
				}
#else
					}
					std::cout << (
						(m_LookupTemp[j] & MEMO_1) ? " 1" : (
							(m_LookupTemp[j] & MEMO_2) ? " 2" : (
								(m_LookupTemp[j] & MEMO_3) ? " 3" : (
									(m_LookupTemp[j] & MEMO_VOLT) ? " V" : " U"))));
				}
				std::cout << std::endl;
#endif

				if (isLegal)
				{
					for (unsigned int j = 0; j < elemCount; j++)
						m_PossibilitiesTempOut[j] |= m_LookupTemp[j];

					foundLegalSolution = true;
				}
			}

			if (j > 0)
			{
				//	Undo previous iteration's swap
				MEMO_TYPE lastLookup = m_LookupTemp[level - 1];
				m_LookupTemp[level - 1] = m_LookupTemp[level - 1 - j];
				m_LookupTemp[level - 1 - j] = lastLookup;
			}
			if (j + 1 < level)
			{
				//	Swap a new lookup with the last one at this level
				MEMO_TYPE lastLookup = m_LookupTemp[level - 1];
				m_LookupTemp[level - 1] = m_LookupTemp[level - 1 - (j + 1)];
				m_LookupTemp[level - 1 - (j + 1)] = lastLookup;
			}
		}

		return foundLegalSolution;
	}

	Solver::SOLVE_RESULT Solver::Solve(unsigned int index, bool isColumn)
	{
		bool isAnythingChanged = false;

		unsigned int confFieldCount = 0;
		unsigned int confPointCount = 0;
		unsigned int confVoltCount = 0;

		unsigned int poss1Count = 0;
		unsigned int poss2Count = 0;
		unsigned int poss3Count = 0;
		unsigned int possVoltCount = 0;

		//	1.1 Check how much of each type is already confirmed/still possible in this column.
		for (unsigned int i = 0; i < (isColumn ? m_Rows : m_Columns); i++)
		{
			unsigned int memo = m_MemosTemp[isColumn ? (index * m_Rows + i) : (i * m_Columns + index)];

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
					possVoltCount++;

				m_PossibilitiesTempIn[i - confFieldCount] = memo;
				m_PossibilitiesTempOut[i - confFieldCount] = 0;
			}
		}

		unsigned int missingFields = (isColumn ? m_Rows : m_Columns) - confFieldCount;
		if (missingFields == 0)
			return SOLVE_NO_CHANGE;

		int missingPoints = (isColumn ? m_uPoint : m_vPoint)[index] - confPointCount;
		int missingVolts = (isColumn ? m_uVolt : m_vVolt)[index] - confVoltCount;

		if (missingPoints < 0 ||
			missingVolts < 0 ||
			missingPoints > (missingFields - missingVolts) * 3 ||
			missingPoints < (missingFields - missingVolts) * 1
		)
			return SOLVE_CONTRADICTION;


		//	1.2 Do lookup of possibilities
		int possIndex = 0;	//	Index into the array of possibilities
		for (unsigned int voltCount = 0; voltCount < missingVolts; voltCount++)
		{
			unsigned int pointFields = missingFields - voltCount;
			possIndex += pointFields * 3 - pointFields * 1 + 1;
		}
		possIndex += missingPoints - (missingFields - missingVolts) * 1;

		if (possIndex < 0)
			return SOLVE_CONTRADICTION;

		unsigned int lookupCount = m_LookupCounts[missingFields - 1][possIndex];	// Count of different possibilities in this lookup

		bool foundLegalSolution = false;
		for (unsigned int lookupIdx = 0; lookupIdx < lookupCount; lookupIdx++)
		{
			MemoCounts memoCounts = m_Lookups[missingFields - 1][possIndex][lookupIdx];
			for (unsigned int onesIdx = 0; onesIdx < memoCounts.memo1; onesIdx++)
				m_LookupTemp[onesIdx] = MEMO_1;
			for (unsigned int twosIdx = memoCounts.memo1; twosIdx < memoCounts.memo1 + memoCounts.memo2; twosIdx++)
				m_LookupTemp[twosIdx] = MEMO_2;
			for (unsigned int threesIdx = memoCounts.memo1 + memoCounts.memo2; threesIdx < memoCounts.memo1 + memoCounts.memo2 + memoCounts.memo3; threesIdx++)
				m_LookupTemp[threesIdx] = MEMO_3;
			for (unsigned int voltsIdx = memoCounts.memo1 + memoCounts.memo2 + memoCounts.memo3; voltsIdx < memoCounts.memo1 + memoCounts.memo2 + memoCounts.memo3 + memoCounts.memoV; voltsIdx++)
				m_LookupTemp[voltsIdx] = MEMO_VOLT;

			foundLegalSolution |= SolveLookupRecurse(missingFields, missingFields);
		}
		if (!foundLegalSolution)
			return SOLVE_CONTRADICTION;


		//	1.3 Reevaluate each field in this column.
		unsigned int confMemos = 0;
		for (unsigned int i = 0; i < (isColumn ? m_Rows : m_Columns); i++)
		{
			unsigned char memo = m_MemosTemp[isColumn ? (index * m_Rows + i) : (i * m_Columns + index)];
			if (memo & MEMO_CONF)
			{
				confMemos++;
				continue;
			}

			memo &= m_PossibilitiesTempOut[i - confMemos];

			if (!(memo & (MEMO_1 | MEMO_2 | MEMO_3 | MEMO_VOLT)))
				return SOLVE_CONTRADICTION;

			//	Check if exactly one type is possible -> confirmed field
			if ((memo ^ MEMO_1) == 0 || (memo ^ MEMO_2) == 0 || (memo ^ MEMO_3) == 0 || (memo ^ MEMO_VOLT) == 0)
				memo |= MEMO_CONF;

			if (memo != m_MemosTemp[isColumn ? (index * m_Rows + i) : (i * m_Columns + index)])
			{
				m_MemosTemp[isColumn ? (index * m_Rows + i) : (i * m_Columns + index)] = memo;
				isAnythingChanged = true;
			}
		}

#ifdef _DEBUG
		PrintBoard();
#endif

		return isAnythingChanged ? SOLVE_CHANGED : SOLVE_NO_CHANGE;
	}

	Solver::SOLVE_RESULT Solver::SolveAll()
	{
		bool isAnythingChanged = false;

		//	1. Iterate over all columns.
		for (unsigned int u = 0; u < m_Columns; u++)
		{
			switch (Solve(u, true))
			{
			case SOLVE_NO_CHANGE:
				break;
			case SOLVE_CHANGED:
				isAnythingChanged = true;
				break;
			case SOLVE_CONTRADICTION:
				return SOLVE_CONTRADICTION;
			}
		}

		//	2. Iterate over all rows
		for (unsigned int v = 0; v < m_Rows; v++)
		{
			switch (Solve(v, false))
			{
			case SOLVE_NO_CHANGE:
				break;
			case SOLVE_CHANGED:
				isAnythingChanged = true;
				break;
			case SOLVE_CONTRADICTION:
				return SOLVE_CONTRADICTION;
			}
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