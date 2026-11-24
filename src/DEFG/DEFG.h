#pragma once
#include <cstdint>
#include <vector>

// DEFG.h
// Header for DEFG
// Built from scratch and from parts of DEFG.cpp on
// Fri Aug 24, 2001 by CXH

#include "DEFGHost.h"	// DEFG's only link to the program it runs in

// The same types as in WCS's Types.h (repeating an identical typedef is legal), so DEFG needs no host header.
typedef double Point3d[3];
typedef float Point3f[3];

#define DEFG_ENABLE_DIAG

#define DEFG_SPAN_MULT			8
#define DEFG_GAPFILL_HORIZ_WEIGHT 1.0f
#define DEFG_GAPFILL_VERT_WEIGHT 1.0f
#define DEFG_GAPFILL_DIAG_WEIGHT .7071f
#define DEFG_NULL_VALUE			-9999.0f
#define DEFG_NORMAL_BINS		36
#define DEFG_NORMAL_BINMOD		(360 / DEFG_NORMAL_BINS)

#define DEFGPi     3.1415926535898
#define DEFGHalfPi 1.5707963267949
#define DEFGTwoPi  6.2831853071796
#define DEFGOneAndHalfPi 4.7123889803847
#define DEFGPiOver180 1.74532925199433E-002
#define DEFGPiUnder180 5.72957795130823E+001

#define DEFG_SPANBLOCK_QUANTA	10

#define DEFG_ON_GRID			1.0E-6	// cells: a point this close to a row, column or grid node counts as on it
#define DEFG_OPEN_REACH			64		// cells: how far OpenGround looks for open ground...
#define DEFG_OPEN_GROUND		4		// ...and how many free cells in a row count as open ground
#define DEFG_SHORT_EDGE			0.5		// cells: nodes closer than this count as this far apart when the normals are solved
#define DEFG_SLOPE_SWEEPS		400		// at most this many sweeps to settle the normals...
#define DEFG_SLOPES_SETTLED		1.0E-12	// ...which have settled when none changes by more than this. Tight on purpose: at 1.0E-6
										// (half the sweeps) the last digits of the grid depended on the order the points arrived in

#define simplemin(a,b)            (((a) < (b)) ? (a) : (b))
#define simplemax(a,b)            (((a) > (b)) ? (a) : (b))

struct Span;

// a point of a row for the row fill that found its cell of the row buffers taken (DEFG::PutKnot)
struct DEFGKnot
	{
	long Cell;
	double GridX;
	float Z, RowSlope;
	bool operator<(const DEFGKnot &Other) const {return(Cell < Other.Cell);};
	}; // DEFGKnot

/*
	double X, Y, Z;
	signed long int PointNum;
	char Disabled;
	int NumSpans, MaxSpans;
	struct Span **SpanBlock;
*/
#define DEFG_INPOINT_DEF 	double X, Y, Z; signed long int PointNum; char Disabled; int NumSpans, MaxSpans; struct Span **SpanBlock;

struct InputPoint
	{
	DEFG_INPOINT_DEF
	}; // InputPoint

struct SmoothedInputPoint
	{
	DEFG_INPOINT_DEF
	double SlopeX, SlopeY;	// its normal, kept as the slopes of its tangent plane: dZ per grid cell along X and along Y
	}; // InputPoint

// crude non-C++ subclassing
#define DEFG_SPAN_DEF uintptr_t PointA, PointB; char OrigConnect, TrisRef;



struct Span
	{
	DEFG_SPAN_DEF
	}; // Span

struct SmoothedSpan
	{
	DEFG_SPAN_DEF
	int ABSlope; // [0...360]
	Point3f Normal;
	}; // Span


class DEFG
	{
	private:
/*		unsigned char TempBuf[DEFG_GRID_WIDTH * DEFG_GRID_HEIGHT];
		float TempFloat[DEFG_GRID_WIDTH * DEFG_GRID_HEIGHT], FinalOutput[DEFG_GRID_WIDTH * DEFG_GRID_HEIGHT], EdgeBuf[DEFG_GRID_WIDTH * DEFG_GRID_HEIGHT];
		double XCoordBuf[DEFG_GRID_WIDTH * DEFG_GRID_HEIGHT];
		unsigned long int ID[DEFG_GRID_WIDTH * DEFG_GRID_HEIGHT];
		unsigned char NonInfectious[DEFG_GRID_WIDTH * DEFG_GRID_HEIGHT];
*/
		unsigned char *TempBuf;
		float *TempFloat, *FinalOutput, *EdgeBuf;
		float *SNX;
		double *XCoordBuf;
		uintptr_t *ID;	// holds InputPoint pointers
		unsigned char *NonInfectious;

		long DefgGridWidth, DefgGridHeight;
		long WriteGridWidth, WriteGridHeight;
		long WriteGridOffX, WriteGridOffY;
		long MaxPoints, MaxSpans;
		int Extrapolate;
		bool densify;

		struct InputPoint	*InPoints;
		struct SmoothedInputPoint	*SmoothedInPoints;
		struct Span			*Edges;
		struct SmoothedSpan			*SmoothedEdges;

		double LowX, HighX, LowY, HighY, LowZ, HighZ, XRange, YRange;
		double BoundLowX, BoundHighX, BoundLowY, BoundHighY;
		double XStart, XTerm, YStart, YTerm;

		DEFGHost *Host;	// never NULL: a default host stands in until SetHost is called

		int LoadedPoints, TotalSpans;
		std::vector<DEFGKnot> ExtraKnots;
		float NullVal, SmoothVal;
		double LSmoothFrac, SSmoothFrac;
		char Smoothing;
		double PeakRoom, PitRoom;	// see SetLimits

		double GridCoordRangeX, GridCoordLowX, GridCoordRangeY, GridCoordLowY, GridCoordSizeXmOneUnderRangeX, GridCoordSizeYmOneUnderRangeY;
		int GridCoordSizeX, GridCoordSizeY;
		int DefgGridCells;

		// set by SetupUnifyInfo() prior to tiled gridding
		float *RowUnify, *ColUnify;
		int UnifyNSTileNum, UnifyEWTileNum, UnifyNSTiles, UnifyEWTiles;

	public:
		int Grid(void);
		DEFG();
		~DEFG();

		int InitSizes(int FinalGridWidth, int FinalGridHeight, double HOver, double VOver, int MaxPoints);
		int InitSpans(int NewMaxSpans);
		int AllocRasterBufs(int FullGridWidth, int FullGridHeight);
		void SetExtrap(int NewExtrap) {Extrapolate = NewExtrap;};
		void SetBounds(double NewMinX, double NewMaxX, double NewMinY, double NewMaxY);
		void SetNullVal(float NewNullVal) {NullVal = NewNullVal;};
		void SetSmoothVal(float NewSmoothVal);
		void SetLimits(float PeakRise, float PitDepth);
		void SetDensify(bool NewVal) {densify = NewVal;};
		void SetHost(DEFGHost *NewHost);	// NULL for none. Set it before InitSizes or DoGrid and keep it alive until this object is gone: memory is released through it

		void SetupUnifyInfo(float *SetupRowUnify, float *SetupColUnify, int SetupUnifyNSTileNum, int SetupUnifyEWTileNum, int SetupUnifyNSTiles, int SetupUnifyEWTiles)
		 {RowUnify = SetupRowUnify; ColUnify = SetupColUnify; UnifyNSTileNum = SetupUnifyNSTileNum; UnifyEWTileNum = SetupUnifyEWTileNum; UnifyNSTiles = SetupUnifyNSTiles; UnifyEWTiles = SetupUnifyEWTiles;};

		void ClearDist(float Value);
		void ClearFinalOutput(void);
		void ClearEdgeBuf(void);
		void ClearXBuffer(void);
		int AutoBoundPoints(void);
		void SetupGridCoordX(int GridSize, double LowCoord, double HighCoord);
		void SetupGridCoordY(int GridSize, double LowCoord, double HighCoord);
		int GridFromCoord(int GridSize, double LowCoord, double HighCoord, double Coord);
		double fGridFromCoord(int GridSize, double LowCoord, double HighCoord, double Coord);
		int GridFromCoordX(double Coord);
		int GridFromCoordY(double Coord);
		double fGridFromCoordX(double Coord);
		double fGridFromCoordY(double Coord);
		double GridPosX(double Coord);
		double GridPosY(double Coord);
		double CoordFromGrid(int GridSize, double LowCoord, double HighCoord, double Coord);
		double CoordFromGridX(double Coord);
		double CoordFromGridY(double Coord);
		double EvalDistNoSq(double XA, double YA, double XB, double YB);
		void FlagInterps(void);
		int InfectCellVal(int GX, int GY, int XOff, unsigned long int Idx, float Weight);
		void NormalizeExtrap(void);
		int InfectCell(int GX, int GY, int XOff, unsigned long int Idx, uintptr_t CenterCellID, double CX, double CY, double RX, double RY);
		int AddSpanToNode(InputPoint *AddNode, struct Span *AddSpan);
		int SpanCell(int GX, int GY, int XOff, unsigned long int Idx, InputPoint *CenterCell);
		void PutKnot(long Cell, double GridX, double Z, double RowSlope);
		int OpenGround(int GX, int GY, double &OpenX, double &OpenY);
		void PlotNode(InputPoint *Node);
		int CalcNodeSlopes(void);
		double SmoothCurve(double T, double ZA, double ZB, double MA, double MB, double *Slope = NULL);
		void RasterizeSpanElevPoints(InputPoint *PA, InputPoint *PB);
		void SmoothRasterizeSpanElevPoints(InputPoint *IPA, InputPoint *IPB);
		void RasterizeSpanElevGridHor(int XS, double DXS, int YS, double ZS, int XE, double DXE, float ZE, double SNXS, float SNXE);
		void RasterizeSpanElev(struct Span *RasterSpan);
		void SmoothRasterizeSpanElev(struct SmoothedSpan *RasterSpan);
		bool AddPoint(double X, double Y, double Z);
		bool AddPoints(Point3d &pt1, Point3d &pt2, unsigned long num2add);

		float *GetFinalArray(void) {return(FinalOutput);};
		int GetFinalArrayWidth(void) {return(DefgGridWidth);};
		int GetFinalArrayHeight(void) {return(DefgGridHeight);};
		int SaveDEM(void);	// trims and unifies the finished grid and gives it to the host

		// grids one tile with points from the host (DEFGHost::EnumeratePoints) and saves it (DEFGHost::SaveGrid)
		int DoGrid(const DEFGGridSpec &Spec);


#ifdef DEFG_ENABLE_DIAG
		void DumpPoints(char *FileName);
		void DumpID(char *FileName);
		void DumpTempBuf(char *FileName);
		void DumpDist(char *FileName);
		void DumpElev(char *FileName);
		void DumpEdges(char *FileName);
		void CreateDumpEdgeBuf(char *FileName);
		void CreateDumpTempElev(char *FileName);
		void CreateDumpTempDist(char *FileName);
		void CreateDumpSNX(char *FileName);
		void CheckForDBLMAX(void);
		void PrintStatus(char *Status);
#endif // DEFG_ENABLE_DIAG

	}; // class DEFG

#ifdef DEFG_ENABLE_DIAG
void DoSplineTest(double AX, double AY, double BX, double BY, double CX, double CY, double DX, double DY, char *FN, double Tension = 0.0);
#endif // DEFG_ENABLE_DIAG
