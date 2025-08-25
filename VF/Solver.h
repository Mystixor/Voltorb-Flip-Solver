#pragma once

namespace VF
{
	class Solver
	{
	public:
		enum MEMO_TYPE : unsigned char
		{
			MEMO_1 = 0b00001,		// Field could be a 1
			MEMO_2 = 0b00010,		// Field could be a 2
			MEMO_3 = 0b00100,		// Field could be a 3
			MEMO_VOLT = 0b01000,	// Field could be a volt
			MEMO_CONF = 0b10000		// Field is confirmed
		};

		Solver(unsigned int rows, unsigned int columns);

		~Solver();

		unsigned int GetColumnCount() const;
		unsigned int GetRowCount() const;

		//	Gets a memo.
		unsigned char GetMemo(unsigned int column, unsigned int row) const;

		//	Sets and confirms a memo, returns the new memo
		unsigned char SetMemo(unsigned int column, unsigned int row, MEMO_TYPE memo);

		//	Unsets a memo and solves the board again, return the new memo
		unsigned char UnsetMemo(unsigned int column, unsigned int row);

		bool IsMemoUserConfirmed(unsigned int column, unsigned int row) const;

		//	Sets the hints on the sides of the board, returns whether the hints are legal.
		bool SetHints(const unsigned int* uPoint, const unsigned int* vPoint, const unsigned int* uVolt, const unsigned int* vVolt);

		void PrintBoard() const;

	private:
		enum SOLVE_RESULT
		{
			SOLVE_NO_CHANGE,		//	There was no change on the board.
			SOLVE_CHANGED,			//	There was a change on the board.
			SOLVE_CONTRADICTION		//	There was a contradiction on the board.
		};

		//	Creates the arrays holding lookup information for which possibilities of values exist for different row/column lengths, available points/volts, known point-/volt-fields. Returns whether the arrays were created successfully.
		bool CreateLookupArrays();

		//	Reset all memos that are not user-confirmed.
		void ResetMemos();

		//	Repeatedly call Solve() until no more changes are possible, or a contradiction is encountered.
		void SolveUntilStable();

		bool SolveLookupRecurse(unsigned int level, unsigned int elemCount);

		SOLVE_RESULT Solve(unsigned int index, bool isColumn);

		//	Does one solving iteration over all temporary rows and columns, returns whether anything was changed.
		SOLVE_RESULT SolveAll();

		//	Commit the current temporary solution as the new legal solution.
		void CommitTemporarySolution();

		bool m_IsLookupInitialized;

		unsigned int const m_Columns;
		unsigned int const m_Rows;

		unsigned int* const m_uPoint;
		unsigned int* const m_vPoint;
		unsigned int* const m_uVolt;
		unsigned int* const m_vVolt;

		//  Column-major matrix of the memos of the playing field
		unsigned char* const m_Memos;

		//  Column-major matrix of the memos of the playing field during a solver loop, may be resetted in case of contradiction
		unsigned char* const m_MemosTemp;

		unsigned char* const m_PossibilitiesTempIn;
		unsigned char* const m_PossibilitiesTempOut;

		MEMO_TYPE* const m_LookupTemp;

		//	Column-major matrix showing which fields are user-confirmed and should not be reset by ResetMemos()
		bool* const m_UserConf;

		//	Array of arrays (for columns/rows with specific points and volts left) with the counts of the available combinations
		unsigned int** const m_LookupCounts;

		struct MemoCounts
		{
			unsigned char memo1;
			unsigned char memo2;
			unsigned char memo3;
			unsigned char memoV;
		};

		//	Array of arrays (for columns/rows with specific points and volts left) of arrays with the available combinations
		MemoCounts*** const m_Lookups;

		unsigned int m_LastUserColumn;
		unsigned int m_LastUserRow;
		unsigned char m_LastUserMemo;
	};
}