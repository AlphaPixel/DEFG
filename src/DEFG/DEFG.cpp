// DEFG.cpp
// Decent Extremely Fast Gridder
// Started August 16th, 2000 by Chris 'Xenon' Hanson
// Copyright 2000, Chris 'Xenon' Hanson and 3D Nature, LLC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>
#include <sys/timeb.h> // semi-accurate timing

#include <algorithm>

#include "DEFG.h"
#include "DEFGSpline.h"

// DEFG reaches its host program only through DEFGHost (DEFGHost.h). This one serves when no host is set.
static DEFGHost DefaultHost;

// Define DEFG_LIMIT_INPOINTS (a point count) to slow DoGrid down when it is given more points than that.
#ifdef DEFG_LIMIT_INPOINTS
static int InPointExcess;
static double ProcessAccum = 0;
#endif // DEFG_LIMIT_INPOINTS

static double BeginTime, StartTime, EndTime, FinalTime, ElapsedTime;	//lint -e551

#ifdef DEFG_ENABLE_DIAG
static char StatusOut[1204];
#endif // DEFG_ENABLE_DIAG

// stage timing for the diagnostics
static double DEFGTime(void)
{
return((double)clock() / CLOCKS_PER_SEC);
} // DEFGTime

// Copies of the vector helpers DEFG uses from WCS's MathSupport.cpp, so it needs nothing from there.
static double VectorMagnitude(Point3d TP)
{

return (sqrt(TP[0] * TP[0] + TP[1] * TP[1] + TP[2] * TP[2]));

} // VectorMagnitude

static double PointDistance(Point3d EP, Point3d SP)
{
Point3d TempDist;

TempDist[0] = EP[0] - SP[0];
TempDist[1] = EP[1] - SP[1];
TempDist[2] = EP[2] - SP[2];
return(VectorMagnitude(TempDist));

} // PointDistance

static void UnitVector(Point3d UV)
{
double Len, Invert;

Len = VectorMagnitude(UV);
if (Len > 0.0)
	{
	Invert = 1.0 / Len;
	UV[0] *= Invert;
	UV[1] *= Invert;
	UV[2] *= Invert;
	} // if

} // UnitVector

static void SurfaceNormal(Point3d NP, Point3d FP, Point3d LP)
{

NP[0] = FP[1] * LP[2] - FP[2] * LP[1];
NP[1] = FP[2] * LP[0] - FP[0] * LP[2];
NP[2] = FP[0] * LP[1] - FP[1] * LP[0];

UnitVector(NP);

} // SurfaceNormal

static void AddPoint3d(Point3d A, Point3d Add)
{

A[0] += Add[0];
A[1] += Add[1];
A[2] += Add[2];

} // AddPoint3d

static void NegateVector(Point3d A)
{

A[0] = -A[0];
A[1] = -A[1];
A[2] = -A[2];

} // NegateVector

// Stolen from WCS's MathSupport.cpp
/*===========================================================================*/
//      0
//      +a
// 270__|__+b 90
//      |
//     180
double DEFGfindangle3(double pta, double ptb)
{
double angle;

if (pta == 0.0)
	{
	angle = ptb > 0.0 ? DEFGHalfPi: DEFGOneAndHalfPi;
	} // if
else
	{
	angle = atan(ptb / pta);
	if (pta < 0.0) angle += DEFGPi;
} // else

return angle;

} // DEFGfindangle3

/*===========================================================================*/
/*===========================================================================*/

void DEFG::ClearDist(float Value)
{
int GX, GY;

// clear dist buffer to FLT_MAX
for (GY = 0; GY < DefgGridHeight; GY++)
	{
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
		TempFloat[GY * DefgGridWidth + GX] = Value;
		} // for
	} // for

} // DEFG::ClearDist

/*===========================================================================*/

void DEFG::ClearFinalOutput(void)
{
int GX, GY;

for (GY = 0; GY < DefgGridHeight; GY++)
	{
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
		FinalOutput[GY * DefgGridWidth + GX] = -FLT_MAX;
		} // for
	} // for

} // DEFG::ClearFinalOutput

/*===========================================================================*/

void DEFG::ClearEdgeBuf(void)
{
int GX, GY;

for (GY = 0; GY < DefgGridHeight; GY++)
	{
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
		EdgeBuf[GY * DefgGridWidth + GX] = -FLT_MAX;
		} // for
	} // for

} // DEFG::ClearEdgeBuf

/*===========================================================================*/

void DEFG::ClearXBuffer(void)
{
int GX, GY;

for (GY = 0; GY < DefgGridHeight; GY++)
	{
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
		XCoordBuf[GY * DefgGridWidth + GX] = -DBL_MAX;
		} // for
	} // for

} // DEFG::ClearXBuffer

/*===========================================================================*/

int DEFG::AutoBoundPoints(void)
{
int BoundLoop;

LowX  = LowY  = LowZ  =  DBL_MAX;
HighX = HighY = HighZ = -DBL_MAX;

for (BoundLoop = 0; BoundLoop < LoadedPoints; BoundLoop++)
	{
	if (Smoothing)
		{
		if (LowX  > SmoothedInPoints[BoundLoop].X) LowX  = SmoothedInPoints[BoundLoop].X;
		if (HighX < SmoothedInPoints[BoundLoop].X) HighX = SmoothedInPoints[BoundLoop].X;

		if (LowY  > SmoothedInPoints[BoundLoop].Y) LowY  = SmoothedInPoints[BoundLoop].Y;
		if (HighY < SmoothedInPoints[BoundLoop].Y) HighY = SmoothedInPoints[BoundLoop].Y;

		if (LowZ  > SmoothedInPoints[BoundLoop].Z) LowZ  = SmoothedInPoints[BoundLoop].Z;
		if (HighZ < SmoothedInPoints[BoundLoop].Z) HighZ = SmoothedInPoints[BoundLoop].Z;
		} // if
	else
		{
		if (LowX  > InPoints[BoundLoop].X) LowX  = InPoints[BoundLoop].X;
		if (HighX < InPoints[BoundLoop].X) HighX = InPoints[BoundLoop].X;

		if (LowY  > InPoints[BoundLoop].Y) LowY  = InPoints[BoundLoop].Y;
		if (HighY < InPoints[BoundLoop].Y) HighY = InPoints[BoundLoop].Y;

		if (LowZ  > InPoints[BoundLoop].Z) LowZ  = InPoints[BoundLoop].Z;
		if (HighZ < InPoints[BoundLoop].Z) HighZ = InPoints[BoundLoop].Z;
		} // else
	} // if

XRange = (HighX - LowX);
YRange = (HighY - LowY);

snprintf(StatusOut, sizeof(StatusOut), "Lat:  %f to %f.\n", -HighY, -LowY); PrintStatus(StatusOut);
snprintf(StatusOut, sizeof(StatusOut), "Lon:  %f to %f.\n", -HighX, -LowX); PrintStatus(StatusOut);
snprintf(StatusOut, sizeof(StatusOut), "Elev: %f to %f.\n", HighZ, LowZ); PrintStatus(StatusOut);

return(BoundLoop);

} // DEFG::AutoBoundPoints

/*===========================================================================*/

void DEFG::SetupGridCoordX(int GridSize, double LowCoord, double HighCoord)
{

GridCoordLowX = LowCoord;
GridCoordRangeX = (HighCoord - LowCoord);
GridCoordSizeX = GridSize;
GridCoordSizeXmOneUnderRangeX = (GridCoordRangeX / (GridCoordSizeX - 1));

} // DEFG::SetupGridCoordX

/*===========================================================================*/

void DEFG::SetupGridCoordY(int GridSize, double LowCoord, double HighCoord)
{

GridCoordLowY = LowCoord;
GridCoordRangeY = (HighCoord - LowCoord);
GridCoordSizeY = GridSize;
GridCoordSizeYmOneUnderRangeY = (GridCoordRangeY / (GridCoordSizeY - 1));

} // DEFG::SetupGridCoordY

/*===========================================================================*/

int DEFG::GridFromCoordX(double Coord)
{
double Dif, Frac;
int GridPos;

Dif = (Coord - GridCoordLowX);
Frac = Dif / GridCoordRangeX;

GridPos = (int)((GridCoordSizeX - 1) * Frac);

return(GridPos);

} // DEFG::GridFromCoordX

/*===========================================================================*/

double DEFG::fGridFromCoordX(double Coord)
{
double Dif, Frac, GridPos;

Dif = (Coord - GridCoordLowX);
Frac = Dif / GridCoordRangeX;

GridPos = ((GridCoordSizeX - 1) * Frac);

return(GridPos);

} // DEFG::fGridFromCoordX

/*===========================================================================*/

int DEFG::GridFromCoordY(double Coord)
{
double Dif, Frac;
int GridPos;

Dif = (Coord - GridCoordLowY);
Frac = Dif / GridCoordRangeY;

GridPos = (int)((GridCoordSizeY - 1) * Frac);

return(GridPos);

} // DEFG::GridFromCoordY

/*===========================================================================*/

double DEFG::fGridFromCoordY(double Coord)
{
double Dif, Frac, GridPos;

Dif = (Coord - GridCoordLowY);
Frac = Dif / GridCoordRangeY;

GridPos = ((GridCoordSizeY - 1) * Frac);

return(GridPos);

} // DEFG::fGridFromCoordY

/*===========================================================================*/

int DEFG::GridFromCoord(int GridSize, double LowCoord, double HighCoord, double Coord)
{
double Range, Dif, Frac;
int GridPos;

Range = (HighCoord - LowCoord);
Dif = (Coord - LowCoord);
Frac = Dif / Range;

GridPos = (int)((GridSize - 1) * Frac);
//GridPos = (int)(GridSize * ((Coord - LowCoord) / Range));

return(GridPos);

} // DEFG::GridFromCoord

/*===========================================================================*/

double DEFG::fGridFromCoord(int GridSize, double LowCoord, double HighCoord, double Coord)
{
double Range, Dif, Frac, GridPos;

Range = (HighCoord - LowCoord);
Dif = (Coord - LowCoord);
Frac = Dif / Range;

GridPos = ((GridSize - 1) * Frac);
//GridPos = (int)(GridSize * ((Coord - LowCoord) / Range));

return(GridPos);

} // DEFG::fGridFromCoord

/*===========================================================================*/

double DEFG::CoordFromGrid(int GridSize, double LowCoord, double HighCoord, double Coord)
{
double Range, Result;

Range = (HighCoord - LowCoord);

Result = LowCoord + (Coord * (Range / (GridSize - 1)));

return(Result);

} // DEFG::CoordFromGrid

/*===========================================================================*/

inline double DEFG::CoordFromGridX(double Coord)
{
double Result;

//Result = GridCoordLowX + (Coord * (GridCoordRangeX / (GridCoordSizeX - 1)));
Result = GridCoordLowX + (Coord * GridCoordSizeXmOneUnderRangeX);

return(Result);

} // DEFG::CoordFromGridX

/*===========================================================================*/

inline double DEFG::CoordFromGridY(double Coord)
{
double Result;

//Result = GridCoordLowY + (Coord * (GridCoordRangeY / (GridCoordSizeY - 1)));
Result = GridCoordLowY + (Coord * GridCoordSizeYmOneUnderRangeY);

return(Result);

} // DEFG::CoordFromGridY

/*===========================================================================*/

double DEFG::EvalDistNoSq(double XA, double YA, double XB, double YB)
{
double DX, DY /*, DXs, DYs, DXsDYs */;

DX = (XA - XB);
DY = (YA - YB);

//return(sqrt(DX * DX + DY * DY));
return((DX * DX + DY * DY));

/*DXs = DX * DX;
DYs = DY * DY;
DXsDYs = (DXs + DYs);
return(DXsDYs);
*/

} // DEFG::EvalDistNoSq

/*===========================================================================*/

void DEFG::FlagInterps(void)
{
int GX, GY;

// use TempBuf to indicate which cells need to be extrapolated
memset(TempBuf, 0, DefgGridCells);
memset(TempFloat, 0, DefgGridCells * sizeof(float));
// we'll make you infectious below when we see you have a valid value.
memset(NonInfectious, 1, DefgGridCells);

for (GY = 0; GY < DefgGridHeight; GY++)
	{
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
		if (FinalOutput[GY * DefgGridWidth + GX] == -FLT_MAX)
			{
			// we need to be interped, we're not infectious until we are interped
			TempBuf[GY * DefgGridWidth + GX] = 1;
			} // if
		else
			{
			// No need to be interped, but we're infectious
			NonInfectious[GY * DefgGridWidth + GX] = 0;
			} // else
		} // for
	} // for

} // DEFG::FlagInterps

/*===========================================================================*/

int DEFG::InfectCellVal(int GX, int GY, int XOff, unsigned long int Idx, float Weight)
{
//double DBoop;
double NewVal;
unsigned long DestCellIdx, SrcCellIdx;
//unsigned long boop = 0;

DestCellIdx = Idx + (GX + XOff);
SrcCellIdx = GY * DefgGridWidth + GX;

// flagged for extrapolation?
if (TempBuf[DestCellIdx])
	{
	NewVal = (Weight * FinalOutput[SrcCellIdx]);
	if (FinalOutput[SrcCellIdx] != 0.0 && (fabs(NewVal) < FLT_MIN))
		{
		NewVal = 0.0;
		} // if
	if (TempFloat[DestCellIdx] == 0.0)
		{
		FinalOutput[DestCellIdx] = (float)NewVal;
		} // if
	else
		{
		FinalOutput[DestCellIdx] += (float)NewVal;
		} // else
	TempFloat[DestCellIdx] += Weight;
	return(1);
	} // if

return(0);

} // DEFG::InfectCellVal

/*===========================================================================*/

void DEFG::NormalizeExtrap(void)
{
int GX, GY, NormGY, NormIdx;
//int Boop;

for (GY = 0; GY < DefgGridHeight; GY++)
	{
	NormGY = GY * DefgGridWidth;
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
/*
		if (GY == 37 && GX == 38)
			Boop = 1;
*/
		NormIdx = NormGY + GX;
		if (TempBuf[NormIdx] && TempFloat[NormIdx] > 0)
			{
			TempBuf[NormIdx] = 0;
			// complete normalizing
			if (TempFloat[NormIdx] != 0.0)
				{
/*
				if (FinalOutput[NormIdx] == -FLT_MAX)
					{
					Boop = 1;
					} // if
*/
				FinalOutput[NormIdx] /= TempFloat[NormIdx];
				TempFloat[NormIdx] = 0.0f;
				} // if
			// we have a valid value, now we become infectious ourselves
			NonInfectious[NormIdx] = 0;
			} // if
		} // for
	} // for

} // DEFG::NormalizeExtrap

/*===========================================================================*/

int DEFG::InfectCell(int GX, int GY, int XOff, unsigned long int Idx, uintptr_t CenterCellID, double CX, double CY, double RX, double RY)
{
InputPoint *OldPoint = NULL;
float Dist, OldDist;
unsigned long int DestCellIdx;

DestCellIdx = Idx + (GX + XOff);

OldDist = TempFloat[DestCellIdx];

if (ID[DestCellIdx] != CenterCellID)
	{
	if (ID[DestCellIdx])
		{
		OldPoint = (InputPoint *)ID[DestCellIdx];
		} // we're reinfecting
	Dist = (float)EvalDistNoSq(CX, CY, RX, RY);
	if (Dist < OldDist)
		{ // infect cell
		TempFloat[DestCellIdx]     = Dist;
		ID[DestCellIdx]            = CenterCellID;
		NonInfectious[DestCellIdx] = 0; // make our infected target infectuous itself
		return(1);
		} // if
	} // if

return(0);

} // DEFG::InfectCell

/*===========================================================================*/

int DEFG::AddSpanToNode(InputPoint *AddNode, struct Span *AddSpan)
{
void *ReBlock;

if (!AddNode->SpanBlock)
	{
	AddNode->SpanBlock = (struct Span **)calloc(1, sizeof(struct Span *) * DEFG_SPANBLOCK_QUANTA);
	AddNode->MaxSpans  = DEFG_SPANBLOCK_QUANTA;
	AddNode->NumSpans  = 0;
	} // if
if (AddNode->SpanBlock)
	{
	// are we out of available slots?
	if (AddNode->NumSpans + 1 == AddNode->MaxSpans)
		{
		if (ReBlock = realloc(AddNode->SpanBlock, sizeof(struct Span *) * (AddNode->MaxSpans + DEFG_SPANBLOCK_QUANTA)))
			{
			AddNode->SpanBlock = (struct Span **)ReBlock;
			AddNode->MaxSpans += DEFG_SPANBLOCK_QUANTA;
			} // if
		} // if
	// are we good to go?
	if (AddNode->NumSpans + 1 != AddNode->MaxSpans)
		{
		// add a span
		AddNode->SpanBlock[AddNode->NumSpans] = AddSpan;
		AddNode->NumSpans ++;
		return(1); // success
		} // if
	} // if

return(0);

} // DEFG::AddSpanToNode

/*===========================================================================*/

int DEFG::SpanCell(int GX, int GY, int XOff, unsigned long int Idx, InputPoint *CenterCell)
{
InputPoint *ANode, *BNode;
struct Span *Moi;
unsigned long DestCellIdx;
uintptr_t CenterCellID, OtherCellID;
signed long SearchSlot;
char FoundA, FoundB;
//Point3d FP /*, LP, NP */;
//int Boop;

DestCellIdx = Idx + (GX + XOff);

CenterCellID = (uintptr_t)CenterCell;
if (ID[DestCellIdx] != CenterCellID)
	{
	OtherCellID  = ID[DestCellIdx];

	if (Smoothing)
		{
		SmoothedEdges[TotalSpans].PointA = simplemin(CenterCellID, OtherCellID);
		SmoothedEdges[TotalSpans].PointB = simplemax(CenterCellID, OtherCellID);
		ANode = (InputPoint *)SmoothedEdges[TotalSpans].PointA;
		BNode = (InputPoint *)SmoothedEdges[TotalSpans].PointB;
		Moi = (struct Span *)&SmoothedEdges[TotalSpans]; // subset of SmoothedSpan
		} // if
	else
		{
		Edges[TotalSpans].PointA = simplemin(CenterCellID, OtherCellID);
		Edges[TotalSpans].PointB = simplemax(CenterCellID, OtherCellID);
		ANode = (InputPoint *)Edges[TotalSpans].PointA;
		BNode = (InputPoint *)Edges[TotalSpans].PointB;

		Moi = &Edges[TotalSpans];
		} // else
	FoundA = FoundB = 0;
	if (ANode->NumSpans && ANode->SpanBlock)
		{
		for (SearchSlot = 0; (SearchSlot < ANode->NumSpans) && (ANode->SpanBlock[SearchSlot]); SearchSlot++)
			{
			if ((ANode->SpanBlock[SearchSlot]->PointA == Moi->PointA) && (ANode->SpanBlock[SearchSlot]->PointB == Moi->PointB))
				{ // we're already linked on this node
				FoundA = 1;
				break;
				} // if
			} // if
		} // if
	if (BNode->NumSpans && BNode->SpanBlock)
		{
		for (SearchSlot = 0; (SearchSlot < BNode->NumSpans) && (BNode->SpanBlock[SearchSlot]); SearchSlot++)
			{
			if ((BNode->SpanBlock[SearchSlot]->PointA == Moi->PointA) && (BNode->SpanBlock[SearchSlot]->PointB == Moi->PointB))
				{ // we're already linked on this node
				FoundB = 1;
				break;
				} // if
			} // if
		} // if
	if (!(FoundA || FoundB))
		{
		AddSpanToNode(ANode, Moi);
		AddSpanToNode(BNode, Moi);
		if (Smoothing)
			{
/*			double fdY, fdX; */
			SmoothedEdges[TotalSpans].OrigConnect = SmoothedEdges[TotalSpans].TrisRef = 0;
/*
			// precalculate slope and normal
			// calculations would get really pissy here if A and B were at the same location
			fdX = fGridFromCoordX(BNode->X) - fGridFromCoordX(ANode->X);
			fdY = fGridFromCoordY(BNode->Y) - fGridFromCoordY(ANode->Y);
			SmoothedEdges[TotalSpans].ABSlope = (int)(DEFGPiUnder180 * DEFGfindangle3(fdY, fdX)); //(atan2(ANode.Y - BNode.Y, ANode.X - BNode.X)
			if (SmoothedEdges[TotalSpans].ABSlope < 0)
				SmoothedEdges[TotalSpans].ABSlope += 360;
			// we make line vector in X & Y pixel coords not original units, since we use it in pixel coords later
			FP[0] = fGridFromCoordX(BNode->X) - fGridFromCoordX(ANode->X);
			FP[1] = fGridFromCoordY(BNode->Y) - fGridFromCoordY(ANode->Y);
			FP[2] = BNode->Z - ANode->Z;
			UnitVector(FP);
*/
/*
			// LP is straight up vector
			LP[0] = 0.0;
			LP[1] = 0.0;
			LP[2] = 1.0;
			// get normal perpendicular to plane containing original line and up vector
			SurfaceNormal(NP, FP, LP);
			// get new "uppish" normal to original line by getting normal
			// of plane comprising original line and the NP vector.
			// We recycle LP as the destination because we don't need it anymore
			SurfaceNormal(LP, FP, NP);
			if (LP[2] < 0.0)
				{
				SmoothedEdges[TotalSpans].Normal[0] = (float)-LP[0];
				SmoothedEdges[TotalSpans].Normal[1] = (float)-LP[1];
				SmoothedEdges[TotalSpans].Normal[2] = (float)-LP[2];
				} // if
			else
				{
				SmoothedEdges[TotalSpans].Normal[0] = (float)LP[0];
				SmoothedEdges[TotalSpans].Normal[1] = (float)LP[1];
				SmoothedEdges[TotalSpans].Normal[2] = (float)LP[2];
				} // else
*/
/*			// record basic normal of line
				SmoothedEdges[TotalSpans].Normal[0] = (float)FP[0];
				SmoothedEdges[TotalSpans].Normal[1] = (float)FP[1];
				SmoothedEdges[TotalSpans].Normal[2] = (float)FP[2];
*/			} // if
		else
			{
			Edges[TotalSpans].OrigConnect = Edges[TotalSpans].TrisRef = 0;
			} // else

		TotalSpans++;
		} // if
	if (FoundA ^ FoundB)
		{
		snprintf(StatusOut, sizeof(StatusOut), "Strange Error: Span mismatch.\n"); PrintStatus(StatusOut);
		} // if
	return(1);
	} // if

return(0);

} // DEFG::SpanCell

/*===========================================================================*/

// The cubic of the smoothing: from ZA at T = 0 to ZB at T = 1 with slopes MA and MB (dZ/dT) at the ends.
// Slope receives dZ/dT at T.
// The end slopes are first held to limits. Without them the curve swings outside its two ends wherever a long edge
// or row span meets a steep slope: pits and streaks across the level ground between two contour lines.
// End slopes between 0 and 3 times the rise keep the curve between its ends. Outside that box each side has one
// meaning: leaving the lower end downward, or reaching the upper end too steeply, dips below the lower end (a pit);
// leaving the lower end too steeply, or reaching the upper end from above, rises over the upper end (a peak).
// PitRoom and PeakRoom widen the box on those sides (SetLimits). With both at 0 a curve whose ends are level stays
// level: a lake stays flat and so does a mesa. Pit room lets the banks' slopes carry on down under a lake, peak room
// lets a hilltop round up over its highest contour, each without the other.
double DEFG::SmoothCurve(double T, double ZA, double ZB, double MA, double MB, double *Slope)
{
double T2 = T * T, T3 = T2 * T, Rise = ZB - ZA;

if (Rise >= 0.0)
	{
	MA = simplemax(-PitRoom, simplemin(MA, 3.0 * Rise + PeakRoom));
	MB = simplemax(-PeakRoom, simplemin(MB, 3.0 * Rise + PitRoom));
	} // if
else
	{
	MA = simplemax(3.0 * Rise - PitRoom, simplemin(MA, PeakRoom));
	MB = simplemax(3.0 * Rise - PeakRoom, simplemin(MB, PitRoom));
	} // else
if (Slope)
	*Slope = (6.0 * T2 - 6.0 * T) * (ZA - ZB) + (3.0 * T2 - 4.0 * T + 1.0) * MA + (3.0 * T2 - 2.0 * T) * MB;

return((2.0 * T3 - 3.0 * T2 + 1.0) * ZA + (3.0 * T2 - 2.0 * T3) * ZB + (T3 - 2.0 * T2 + T) * MA + (T3 - T2) * MB);

} // DEFG::SmoothCurve

/*===========================================================================*/

// Smoothing, one edge of the triangulation:
// - a cubic from A to B whose slope at each end is that of the end's tangent plane (its normal), sliced by the
//   vertical plane of the edge;
// - rasterized where it crosses each row: a point for the row fill (PutKnot) with its X position, its elevation
//   and the surface slope along the row (dZ/dX per cell):
//   along the edge the surface follows the cubic, across the edge its slope runs linearly from A's to B's.
// Everything is in grid cells, with the ends where the points really are.
void DEFG::SmoothRasterizeSpanElevPoints(InputPoint *IPA, InputPoint *IPB)
{
double AX, AY, BX, BY, LineLen, UX, UY, AlongA, AlongB, CrossA, CrossB, Cross, LenFrac, XPos, ZPos, DZ;
SmoothedInputPoint *PA, *PB;
int IY, LastY, LoopStep;

PA = (SmoothedInputPoint *)IPA;
PB = (SmoothedInputPoint *)IPB;

AX = GridPosX(PA->X);
AY = GridPosY(PA->Y);
BX = GridPosX(PB->X);
BY = GridPosY(PB->Y);
if (AY == BY)
	return;	// it lies along the rows and crosses none: the row fill makes the same curve from its two ends

LineLen = sqrt((BX - AX) * (BX - AX) + (BY - AY) * (BY - AY));
// unit vector along the edge; (-UY, UX) is the unit vector across it
UX = (BX - AX) / LineLen;
UY = (BY - AY) / LineLen;
AlongA = PA->SlopeX * UX + PA->SlopeY * UY;
CrossA = PA->SlopeY * UX - PA->SlopeX * UY;
AlongB = PB->SlopeX * UX + PB->SlopeY * UY;
CrossB = PB->SlopeY * UX - PB->SlopeX * UY;

// the rows it crosses, from A's end to B's
if (AY < BY) {IY = (int)ceil(AY); LastY = (int)floor(BY); LoopStep = 1;}
else {IY = (int)floor(AY); LastY = (int)ceil(BY); LoopStep = -1;}
for (; (LastY - IY) * LoopStep >= 0; IY += LoopStep)
	{
	LenFrac = (IY - AY) / (BY - AY);
	XPos = AX + (LenFrac * (BX - AX));
	if (fabs(XPos - floor(XPos + 0.5)) < DEFG_ON_GRID)
		XPos = floor(XPos + 0.5);
	ZPos = SmoothCurve(LenFrac, PA->Z, PB->Z, AlongA * LineLen, AlongB * LineLen, &DZ);
	// the smoothing amount mixes the cubic with the straight edge
	ZPos = SSmoothFrac * ZPos + LSmoothFrac * (PA->Z + (LenFrac * (PB->Z - PA->Z)));
	Cross = CrossA + LenFrac * (CrossB - CrossA);

	if (fabs(ZPos) < FLT_MIN)
		{
		ZPos = 0.0;
		} // if
	// the slope along the row: the X part of (slope along the edge) * (UX, UY) + (slope across it) * (-UY, UX)
	PutKnot((long)IY * DefgGridWidth + (long)floor(XPos), XPos, ZPos, (DZ / LineLen) * UX - Cross * UY);
	} // for

} // SmoothRasterizeSpanElevPoints

/*===========================================================================*/

// One edge of the triangulation, rasterized where it crosses each row: a point for the row fill (PutKnot) with its
// X position and elevation. The ends are where the points really are.
void DEFG::RasterizeSpanElevPoints(InputPoint *PA, InputPoint *PB)
{
double AX, AY, BX, BY, LenFrac, XPos;
int IY, LastY, LoopStep;

AX = GridPosX(PA->X);
AY = GridPosY(PA->Y);
BX = GridPosX(PB->X);
BY = GridPosY(PB->Y);
if (AY == BY)
	return;	// it lies along the rows and crosses none

// the rows it crosses, from A's end to B's
if (AY < BY) {IY = (int)ceil(AY); LastY = (int)floor(BY); LoopStep = 1;}
else {IY = (int)floor(AY); LastY = (int)ceil(BY); LoopStep = -1;}
for (; (LastY - IY) * LoopStep >= 0; IY += LoopStep)
	{
	LenFrac = (IY - AY) / (BY - AY);
	XPos = AX + (LenFrac * (BX - AX));
	if (fabs(XPos - floor(XPos + 0.5)) < DEFG_ON_GRID)
		XPos = floor(XPos + 0.5);
	PutKnot((long)IY * DefgGridWidth + (long)floor(XPos), XPos, PA->Z + (LenFrac * (PB->Z - PA->Z)), 0.0);
	} // for

} // DEFG::RasterizeSpanElevPoints

/*===========================================================================*/

void DEFG::RasterizeSpanElevGridHor(int XS, double DXS, int YS, double ZS, int XE, double DXE, float ZE, double SNXS, float SNXE)
{
double DeltaX, DeltaZ, ZPos, LZPos, LenFrac, XPos, CellLen = 0.0;
int LoopCoord, LoopStep;

DeltaX = DXS - DXE;
DeltaZ = ZS - ZE;

if (XS < XE) LoopStep = 1;
else LoopStep = -1;
if (Smoothing)
	{
	// Smoothing, the row fill: the cubic again, now along the row from one edge to the next, with the row
	// slopes the edge pass left in SNX. Those are per cell, so the span is measured in cells (start to end).
	CellLen = fGridFromCoordX(DXE) - fGridFromCoordX(DXS);
	} // if smoothing

LoopCoord = XS;
while(1)	//lint !e716
	{
	XPos = CoordFromGridX((double)LoopCoord);
	LenFrac = ((DXS - XPos) / DeltaX);
	if (LenFrac >= 0.0 && LenFrac <= 1.0)
		{
		// we need linear value either way
		LZPos = ZS - (LenFrac * DeltaZ);
		if (Smoothing)
			{
			// the smoothing amount mixes the cubic with the straight line
			ZPos = SSmoothFrac * SmoothCurve(LenFrac, ZS, ZE, SNXS * CellLen, SNXE * CellLen) + LSmoothFrac * LZPos;
			} // if
		else
			{
			ZPos = LZPos;
			} // else
		if (fabs(ZPos) < FLT_MIN)
			{
			ZPos = 0.0;
			} // if
		FinalOutput[YS * DefgGridWidth + LoopCoord] = (float)ZPos;
		} // if
	if (LoopCoord == XE) break; // before writing final pixel. Test before loop increment
	LoopCoord += LoopStep;
	} // for

} // DEFG::RasterizeSpanElevGridHor

/*===========================================================================*/

void DEFG::RasterizeSpanElev(struct Span *RasterSpan)
{
InputPoint *PA, *PB;

PA = (InputPoint *)RasterSpan->PointA;
PB = (InputPoint *)RasterSpan->PointB;

RasterizeSpanElevPoints(PA, PB);
} // DEFG::RasterizeSpanElev

/*===========================================================================*/

void DEFG::SmoothRasterizeSpanElev(struct SmoothedSpan *RasterSpan)
{
InputPoint *PA, *PB;

PA = (InputPoint *)RasterSpan->PointA;
PB = (InputPoint *)RasterSpan->PointB;

SmoothRasterizeSpanElevPoints(PA, PB);
} // DEFG::SmoothRasterizeSpanElev

/*===========================================================================*/

// Which way the open ground lies from a cell, judged from nothing but which cells hold points (TempFloat > 0 while
// the points are being merged). Sixteen rays are walked from the cell until each meets a cell with a point or the
// edge of the grid; each ray's free run times its direction adds to (OpenX, OpenY). Returns 0 if no ray runs
// DEFG_OPEN_GROUND cells free or the runs cancel out: then there is no open ground to speak of on any side.
int DEFG::OpenGround(int GX, int GY, double &OpenX, double &OpenY)
{
static const double Ray[16][2] = {
	{1.0, 0.0}, {0.9238795325, 0.3826834324}, {0.7071067812, 0.7071067812}, {0.3826834324, 0.9238795325},
	{0.0, 1.0}, {-0.3826834324, 0.9238795325}, {-0.7071067812, 0.7071067812}, {-0.9238795325, 0.3826834324},
	{-1.0, 0.0}, {-0.9238795325, -0.3826834324}, {-0.7071067812, -0.7071067812}, {-0.3826834324, -0.9238795325},
	{0.0, -1.0}, {0.3826834324, -0.9238795325}, {0.7071067812, -0.7071067812}, {0.9238795325, -0.3826834324}};
int Dir, Step, Run, Longest = 0, X, Y;

OpenX = OpenY = 0.0;
for (Dir = 0; Dir < 16; Dir++)
	{
	Run = 0;
	for (Step = 1; Step <= DEFG_OPEN_REACH; Step++)
		{
		X = (int)floor(GX + Step * Ray[Dir][0] + 0.5);
		Y = (int)floor(GY + Step * Ray[Dir][1] + 0.5);
		if (X < 0 || Y < 0 || X >= DefgGridWidth || Y >= DefgGridHeight || TempFloat[Y * DefgGridWidth + X] > 0.0f)
			break;
		Run = Step;
		} // for
	OpenX += Run * Ray[Dir][0];
	OpenY += Run * Ray[Dir][1];
	if (Run > Longest)
		Longest = Run;
	} // for

return(Longest >= DEFG_OPEN_GROUND && sqrt(OpenX * OpenX + OpenY * OpenY) >= DEFG_OPEN_GROUND);

} // DEFG::OpenGround

/*===========================================================================*/

// Where a coordinate falls on the grid, in cells. Points are used where they are, not moved to a cell. One within a
// millionth of a cell of a row, a column or a grid node is put exactly on it, so that points given on the grid stay there.
double DEFG::GridPosX(double Coord)
{
double Pos = fGridFromCoordX(Coord), Near = floor(Pos + 0.5);

return(fabs(Pos - Near) < DEFG_ON_GRID ? Near: Pos);

} // DEFG::GridPosX

/*===========================================================================*/

double DEFG::GridPosY(double Coord)
{
double Pos = fGridFromCoordY(Coord), Near = floor(Pos + 0.5);

return(fabs(Pos - Near) < DEFG_ON_GRID ? Near: Pos);

} // DEFG::GridPosY

/*===========================================================================*/

// One point of a row for the row fill: where an edge crosses the row or a node lies on it, with its elevation and,
// when smoothing, the surface slope along the row. The row buffers hold one such point per cell; any others in the
// same cell go to ExtraKnots, because the fill needs the leftmost to end the span that comes in and the rightmost to
// start the next. Losing one of two used to run a span to the wrong point: a streak one row high.
void DEFG::PutKnot(long Cell, double GridX, double Z, double RowSlope)
{
DEFGKnot Extra;

if (EdgeBuf[Cell] != -FLT_MAX && XCoordBuf[Cell] != CoordFromGridX(GridX))
	{
	Extra.Cell = Cell;
	Extra.GridX = GridX;
	Extra.Z = (float)Z;
	Extra.RowSlope = (float)RowSlope;
	ExtraKnots.push_back(Extra);
	return;
	} // if
EdgeBuf[Cell] = (float)Z;
XCoordBuf[Cell] = CoordFromGridX(GridX);
if (Smoothing)
	SNX[Cell] = (float)RowSlope;

} // DEFG::PutKnot

/*===========================================================================*/

// A node that lies on a row is a point of that row for the row fill, and one on a grid node is that cell's elevation.
// A node between rows reaches the grid only through its edges.
void DEFG::PlotNode(InputPoint *Node)
{
double NX = GridPosX(Node->X), NY = GridPosY(Node->Y);
long Cell;

if (NY != floor(NY) || NX < 0.0 || NY < 0.0 || NX > DefgGridWidth - 1 || NY > DefgGridHeight - 1)
	return;
Cell = (long)NY * DefgGridWidth + (long)floor(NX);
PutKnot(Cell, NX, Node->Z, Smoothing ? ((SmoothedInputPoint *)Node)->SlopeX: 0.0);
if (NX == floor(NX))
	FinalOutput[Cell] = (float)Node->Z;

} // DEFG::PlotNode

/*===========================================================================*/

// Smoothing: gives every node its normal, kept as the slopes of its tangent plane.
// They are the slopes that bend the curves along all the edges least (Nielson 1983, Renka 1984). Along one edge of
// length L the bending of the cubic between two nodes is least when this node's slope along the edge is 1.5 times the
// edge's own slope less half the neighbour's; each node gets the slopes that best meet that on all its edges at once,
// weighted 1 / L^3, and the sweep over the nodes is repeated until the slopes settle. Returns 0 if cancelled.
int DEFG::CalcNodeSlopes(void)
{
SmoothedInputPoint *Node, *Other;
double NX, NY, EX, EY, L2, L3, Pull, AXX, AXY, AYY, BX, BY, Det, NewX, NewY, Change, Worst;
int Sweep, Loop, Slot;

for (Sweep = 0; Sweep < DEFG_SLOPE_SWEEPS; Sweep++)
	{
	if (Host->ProgressUpdate(Sweep))
		return(0);
	Worst = 0.0;
	for (Loop = 0; Loop < LoadedPoints; Loop++)
		{
		Node = &SmoothedInPoints[Loop];
		if (Node->Disabled)
			continue;
		NX = GridPosX(Node->X);
		NY = GridPosY(Node->Y);
		AXX = AXY = AYY = BX = BY = NewX = NewY = 0.0;
		for (Slot = 0; Slot < Node->NumSpans; Slot++)
			{
			Other = (SmoothedInputPoint *)(Node->SpanBlock[Slot]->PointA == (uintptr_t)Node ? Node->SpanBlock[Slot]->PointB: Node->SpanBlock[Slot]->PointA);
			EX = GridPosX(Other->X) - NX;
			EY = GridPosY(Other->Y) - NY;
			L2 = EX * EX + EY * EY;
			if (L2 == 0.0)
				continue;
			if (L2 < DEFG_SHORT_EDGE * DEFG_SHORT_EDGE)
				{ // the grid cannot resolve a slope between nodes this close: they count as DEFG_SHORT_EDGE apart
				EX *= DEFG_SHORT_EDGE / sqrt(L2);
				EY *= DEFG_SHORT_EDGE / sqrt(L2);
				L2 = DEFG_SHORT_EDGE * DEFG_SHORT_EDGE;
				} // if
			L3 = L2 * sqrt(L2);
			Pull = (3.0 * (Other->Z - Node->Z) - (Other->SlopeX * EX + Other->SlopeY * EY)) / (2.0 * L3);
			AXX += EX * EX / L3;
			AXY += EX * EY / L3;
			AYY += EY * EY / L3;
			BX += Pull * EX;
			BY += Pull * EY;
			} // for
		Det = AXX * AYY - AXY * AXY;
		if (Det > 1.0E-9 * (AXX + AYY) * (AXX + AYY))
			{
			NewX = (BX * AYY - BY * AXY) / Det;
			NewY = (BY * AXX - BX * AXY) / Det;
			} // if
		else if ((Det = BX * (AXX * BX + AXY * BY) + BY * (AXY * BX + AYY * BY)) > 0.0)
			{ // its neighbours all lie along one line: it gets the slope along that line and none across it
			NewX = BX * (BX * BX + BY * BY) / Det;
			NewY = BY * (BX * BX + BY * BY) / Det;
			} // else if
		Change = simplemax(fabs(NewX - Node->SlopeX), fabs(NewY - Node->SlopeY)) / simplemax(1.0, simplemax(fabs(NewX), fabs(NewY)));
		if (Change > Worst)
			Worst = Change;
		Node->SlopeX = NewX;
		Node->SlopeY = NewY;
		} // for
	if (Worst < DEFG_SLOPES_SETTLED)
		break;
	} // for

return(1);

} // DEFG::CalcNodeSlopes

/*===========================================================================*/

int DEFG::Grid(void)
{
double CX, CY, RY, RX;
int Abort = 0, Loop, GX, GY, XOff, YOff;
size_t NextExtra;
//int SpanSearchA, SpanSearchB;
unsigned long int GYm1Idx, GY0Idx, GYp1Idx, GYIdx, Counter, Iterations, DisabledPoints = 0, UsablePoints = 0;
uintptr_t CenterCellID;
InputPoint *WorkPoint, *MergePoint;
double OpenX, OpenY, Toward;
long MergeCell;
int PointDisabled;
//int Boop;


//snprintf(StatusOut, sizeof(StatusOut), "Beginning processing.\n"); PrintStatus(StatusOut);

if (!AllocRasterBufs(DefgGridWidth, DefgGridHeight))
	return(0);


BeginTime = StartTime = DEFGTime();

SetupGridCoordX(DefgGridWidth,  LowX, HighX);
SetupGridCoordY(DefgGridHeight, LowY, HighY);

memset(ID, 0, DefgGridCells * sizeof(uintptr_t));
ClearDist(0.0f);
ClearEdgeBuf();
ClearXBuffer();
// <<<>>> remove when done debugging
memset(TempBuf, 0, DefgGridCells);

// Points that share a cell become one node. Where two contour lines pass through one cell, averaging their points
// puts a node between the lines. On the shore of a lake that node stands above its neighbours, and long sloping
// triangles run from it across the level water; the rim of a mesa has the same trouble upside down. So a cell
// holding points of more than one elevation keeps the points of one elevation only: the one whose points lie
// furthest toward the open ground, which is the line the open ground is bounded by. Nothing is known about the
// lines themselves; the open ground is found from which cells hold points at all (OpenGround). Where there is no
// open ground near, the cell's points are averaged as before: that is every cell of closely packed points.
memset(NonInfectious, 0, DefgGridCells);
for (Loop = 0; Loop < LoadedPoints; Loop++)
	{ // which cells hold points (TempFloat counts them), and which hold more than one elevation (NonInfectious)
	MergePoint = Smoothing ? (InputPoint *)&SmoothedInPoints[Loop]: &InPoints[Loop];
	MergeCell = GridFromCoordY(MergePoint->Y) * DefgGridWidth + GridFromCoordX(MergePoint->X);
	if (TempFloat[MergeCell] == 0.0f)
		EdgeBuf[MergeCell] = (float)MergePoint->Z;
	else if (EdgeBuf[MergeCell] != (float)MergePoint->Z && ! NonInfectious[MergeCell])
		{
		NonInfectious[MergeCell] = 1;
		XCoordBuf[MergeCell] = -DBL_MAX;
		} // else if
	TempFloat[MergeCell] += 1.0f;
	} // for
for (Loop = 0; Loop < LoadedPoints; Loop++)
	{ // in those cells: the elevation of the point furthest toward the open ground goes into EdgeBuf
	MergePoint = Smoothing ? (InputPoint *)&SmoothedInPoints[Loop]: &InPoints[Loop];
	MergeCell = GridFromCoordY(MergePoint->Y) * DefgGridWidth + GridFromCoordX(MergePoint->X);
	if (NonInfectious[MergeCell] != 1)
		continue;
	if (! OpenGround(GridFromCoordX(MergePoint->X), GridFromCoordY(MergePoint->Y), OpenX, OpenY))
		{
		NonInfectious[MergeCell] = 2;	// no open ground: average
		continue;
		} // if
	Toward = fGridFromCoordX(MergePoint->X) * OpenX + fGridFromCoordY(MergePoint->Y) * OpenY;
	if (Toward > XCoordBuf[MergeCell])
		{
		XCoordBuf[MergeCell] = Toward;
		EdgeBuf[MergeCell] = (float)MergePoint->Z;
		} // if
	} // for
ClearDist(0.0f);

//snprintf(StatusOut, sizeof(StatusOut), "Merging points.\n"); PrintStatus(StatusOut);
Host->ProgressBegin("Merging Points", LoadedPoints);
for (Loop = 0; Loop < LoadedPoints; Loop++)
	{
	if (Host->ProgressUpdate(Loop))
		{
		Abort = 1;
		break;
		} // if
	MergePoint = Smoothing ? (InputPoint *)&SmoothedInPoints[Loop]: &InPoints[Loop];
	MergeCell = GridFromCoordY(MergePoint->Y) * DefgGridWidth + GridFromCoordX(MergePoint->X);
	if (NonInfectious[MergeCell] == 1 && EdgeBuf[MergeCell] != (float)MergePoint->Z)
		{ // another line's point in a cell that keeps the line facing the open ground
		MergePoint->Disabled = 1;
		DisabledPoints++;
		continue;
		} // if
	if (Smoothing)
		{
		GX = GridFromCoordX(SmoothedInPoints[Loop].X);
		GY = GridFromCoordY(SmoothedInPoints[Loop].Y);
		} // if
	else
		{
		GX = GridFromCoordX(InPoints[Loop].X);
		GY = GridFromCoordY(InPoints[Loop].Y);
		} // else
	// point averaging here.
	if (ID[GY * DefgGridWidth + GX] == 0)
		{
		if (Smoothing)
			{
			ID[GY * DefgGridWidth + GX] = (uintptr_t)&SmoothedInPoints[Loop];
			} // if
		else
			{
			ID[GY * DefgGridWidth + GX] = (uintptr_t)&InPoints[Loop];
			} // else
		TempFloat[GY * DefgGridWidth + GX] = 1.0f;
		} // if
	else
		{
		WorkPoint = (InputPoint *)ID[GY * DefgGridWidth + GX];
		if (Smoothing)
			{
			WorkPoint->X += SmoothedInPoints[Loop].X;
			WorkPoint->Y += SmoothedInPoints[Loop].Y;
			WorkPoint->Z += SmoothedInPoints[Loop].Z;
			SmoothedInPoints[Loop].Disabled = 1;
			} // if
		else
			{
			WorkPoint->X += InPoints[Loop].X;
			WorkPoint->Y += InPoints[Loop].Y;
			WorkPoint->Z += InPoints[Loop].Z;
			InPoints[Loop].Disabled = 1;
			} // else
		TempFloat[GY * DefgGridWidth + GX] += 1.0f;
		DisabledPoints++;
		} // else
	} // for


//snprintf(StatusOut, sizeof(StatusOut), "Normalizing merged points.\n"); PrintStatus(StatusOut);
Host->ProgressEnd();
if (Abort) return(0);
Host->ProgressBegin("Normalizing Merged Points", DefgGridHeight);
// normalize to complete averaging
for (GY = 0; GY < DefgGridHeight; GY++)
	{
	if (Host->ProgressUpdate(GY))
		{
		Abort = 1;
		break;
		} // if
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
		if (TempFloat[GY * DefgGridWidth + GX] > 1.0)
			{
			WorkPoint = (InputPoint *)ID[GY * DefgGridWidth + GX];
			WorkPoint->X /= TempFloat[GY * DefgGridWidth + GX];
			WorkPoint->Y /= TempFloat[GY * DefgGridWidth + GX];
			WorkPoint->Z /= TempFloat[GY * DefgGridWidth + GX];
			} // if
		} // for
	} // for


ClearEdgeBuf();	// it held the kept elevations during the merge

// re-rasterize with conflict-resolved pointset only
memset(ID,  0, DefgGridCells * sizeof(uintptr_t));
memset(NonInfectious, 0, DefgGridCells);

ClearDist(FLT_MAX);


// Plot cardinal points

//DumpPoints("c:\\tmp\\merged.xyz");

//snprintf(StatusOut, sizeof(StatusOut), "Processing merged points.\n"); PrintStatus(StatusOut);
Host->ProgressEnd();
if (Abort) return(0);
Host->ProgressBegin("Processing Merged Points", LoadedPoints);
for (Loop = 0; Loop < LoadedPoints; Loop++)
	{
/*
	if (Loop == 10435)
		{
		Boop = 1;
		} // if
*/
	if (Host->ProgressUpdate(Loop))
		{
		Abort = 1;
		break;
		} // if
#ifdef DEFG_LIMIT_INPOINTS
	if (InPointExcess > 0)
		{
		for (int ProcessWorkCounter = 0; ProcessWorkCounter < InPointExcess; ProcessWorkCounter++)
			{
			ProcessAccum += sqrt((double)(ProcessWorkCounter & 0xff));
			ProcessAccum = fmod(ProcessAccum, 100.0);
			} // for
		} // if
#endif // DEFG_LIMIT_INPOINTS

	if (Smoothing)
		{
		PointDisabled = SmoothedInPoints[Loop].Disabled;
		} // if
	else
		{
		PointDisabled = InPoints[Loop].Disabled;
		} // else
	if (!PointDisabled)
		{
		UsablePoints++;
		if (Smoothing)
			{
			GX = GridFromCoordX(SmoothedInPoints[Loop].X);
			GY = GridFromCoordY(SmoothedInPoints[Loop].Y);
			ID[GY * DefgGridWidth + GX] = (uintptr_t)&SmoothedInPoints[Loop];
			} // if
		else
			{
			GX = GridFromCoordX(InPoints[Loop].X);
			GY = GridFromCoordY(InPoints[Loop].Y);
			ID[GY * DefgGridWidth + GX] = (uintptr_t)&InPoints[Loop];
			} // else
		TempFloat[GY * DefgGridWidth + GX] = 0.0f;
		} // if
	} // for


// allocate span storage now that we have a better idea of how many
// points (and therefore spans) we really need space for

if (!InitSpans(UsablePoints * DEFG_SPAN_MULT)) return(0);

//DumpID("c:\\tmp\\startdump.raw");
//CreateDumpEdgeBuf("c:\\tmp\\CardinalPoints.raw");

//snprintf(StatusOut, sizeof(StatusOut), "Processing network.\n"); PrintStatus(StatusOut);

// iterate convolution-kernal trick

Host->ProgressEnd();
if (Abort) return(0);
Host->ProgressBegin("Processing Network", 1 + (int)sqrt((double)(DefgGridWidth * DefgGridWidth + DefgGridHeight * DefgGridHeight))); //lint !e790
Iterations = 0;
do
	{
	if (Host->ProgressUpdate(Iterations))
		{
		Abort = 1;
		break;
		} // if
	Counter = 0;
	for (GY = 0; GY < DefgGridHeight; GY++)
		{
		GYm1Idx = (GY - 1) * DefgGridWidth;
		GY0Idx  = (GY + 0) * DefgGridWidth;
		GYp1Idx = (GY + 1) * DefgGridWidth;
		for (GX = 0; GX < DefgGridWidth; GX++)
			{
#ifdef DEFG_LIMIT_INPOINTS
	if (InPointExcess > 0)
		{
		for (int ProcessWorkCounter = 0; ProcessWorkCounter < InPointExcess; ProcessWorkCounter++)
			{
			ProcessAccum += sqrt((double)(ProcessWorkCounter & 0xff));
			ProcessAccum = fmod(ProcessAccum, 100.0);
			} // for
		} // if
#endif // DEFG_LIMIT_INPOINTS

			// <<<>>> OPT
			//unsigned long int GY0IdxPlusGX;
			//GY0IdxPlusGX = GY0Idx + GX;
			//if ((CenterCellID = ID[GY0IdxPlusGX]) && (!NonInfectious[GY0IdxPlusGX])) // empty cells can't spread virally
			if ((CenterCellID = ID[GY0Idx + GX]) && (!NonInfectious[GY0Idx + GX])) // empty cells can't spread virally
				{
				WorkPoint = (InputPoint *)CenterCellID;
				CX = WorkPoint->X;
				CY = WorkPoint->Y;
				if (GY > 0)
					{ // look up one line
					YOff  = -1;
					GYIdx = GYm1Idx;
					RY    = CoordFromGridY((double)(GY + YOff));
					if (GX > 0)
						{ // look left
						XOff      = -1;
						RX        = CoordFromGridX((double)(GX + XOff));
						Counter  += InfectCell(GX, GY, XOff, GYIdx, CenterCellID, CX, CY, RX, RY);
						} // if
					if (GX < DefgGridWidth - 1)
						{ // look right
						XOff      = 1;
						RX        = CoordFromGridX((double)(GX + XOff));
						Counter  += InfectCell(GX, GY, XOff, GYIdx, CenterCellID, CX, CY, RX, RY);
						} // if
					// always look above on your column
					if (1) // preserve consistant indent
						{
						XOff      = 0;
						RX        = CoordFromGridX((double)(GX + XOff));
						Counter  += InfectCell(GX, GY, XOff, GYIdx, CenterCellID, CX, CY, RX, RY);
						} // if
					} // if
				if (GY < DefgGridHeight - 1)
					{ // look down one line
					YOff  = 1;
					GYIdx = GYp1Idx;
					RY    = CoordFromGridY((double)(GY + YOff));
					if (GX > 0)
						{ // look left
						XOff      = -1;
						RX        = CoordFromGridX((double)(GX + XOff));
						Counter  += InfectCell(GX, GY, XOff, GYIdx, CenterCellID, CX, CY, RX, RY);
						} // if
					if (GX < DefgGridWidth - 1)
						{ // look right
						XOff      = 1;
						RX        = CoordFromGridX((double)(GX + XOff));
						Counter  += InfectCell(GX, GY, XOff, GYIdx, CenterCellID, CX, CY, RX, RY);
						} // if
					// always look below on your column
					if (1) // preserve consistant indent
						{
						XOff      = 0;
						RX        = CoordFromGridX((double)(GX + XOff));
						Counter  += InfectCell(GX, GY, XOff, GYIdx, CenterCellID, CX, CY, RX, RY);
						} // if
					} // if
				if (1) // preserve consistant indent
					{ // always try left/right on your own line
					YOff  = 0;
					GYIdx = GY0Idx;
					RY    = CoordFromGridY((double)(GY + YOff));
					if (GX > 0)
						{ // look left
						XOff      = -1;
						RX        = CoordFromGridX((double)(GX + XOff));
						Counter  += InfectCell(GX, GY, XOff, GYIdx, CenterCellID, CX, CY, RX, RY);
						} // if
					if (GX < DefgGridWidth - 1)
						{ // look right
						XOff      = 1;
						RX        = CoordFromGridX((double)(GX + XOff));
						Counter  += InfectCell(GX, GY, XOff, GYIdx, CenterCellID, CX, CY, RX, RY);
						} // if
					} // if
				NonInfectious[GY0Idx + GX] = 1;
				// <<<>>> OPT
				//NonInfectious[GY0IdxPlusGX] = 1;
				} // if
			} // for
		} // for
	Iterations++;

	//DumpID("c:\\tmp\\dump.raw");

	} while (Counter);


EndTime = DEFGTime();
ElapsedTime = (EndTime - StartTime);

//snprintf(StatusOut, sizeof(StatusOut), "Total Points processed: %d.\n", (LoadedPoints - DisabledPoints)); PrintStatus(StatusOut);
//snprintf(StatusOut, sizeof(StatusOut), "Total Iterations: %d.\n", Iterations); PrintStatus(StatusOut);
//snprintf(StatusOut, sizeof(StatusOut), "Elapsed NN computation time: %f seconds.\n", ElapsedTime); PrintStatus(StatusOut);

//DumpID("c:\\tmp\\ID.raw");
//CreateDumpTempDist("c:\\tmp\\dist.raw");


// Create Span Lists

//snprintf(StatusOut, sizeof(StatusOut), "Spanning Cells...\n"); PrintStatus(StatusOut);
Host->ProgressEnd();
if (Abort) return(0);
Host->ProgressBegin("Spanning Cells", DefgGridHeight);

StartTime = DEFGTime();

for (GY = 0; GY < DefgGridHeight; GY++)
	{
	if (Host->ProgressUpdate(GY))
		{
		Abort = 1;
		break;
		} // if

#ifdef DEFG_LIMIT_INPOINTS
	if (InPointExcess > 0)
		{
		for (int ProcessWorkCounter = 0; ProcessWorkCounter < InPointExcess; ProcessWorkCounter++)
			{
			ProcessAccum += sqrt((double)(ProcessWorkCounter & 0xff));
			ProcessAccum = fmod(ProcessAccum, 100.0);
			} // for
		} // if
#endif // DEFG_LIMIT_INPOINTS

	GY0Idx  = (GY + 0) * DefgGridWidth;
	GYp1Idx = (GY + 1) * DefgGridWidth;
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
		CenterCellID = ID[GY0Idx + GX];
		WorkPoint = (InputPoint *)CenterCellID;
		GYIdx = GY0Idx;
		if (GX < DefgGridWidth - 1)
			{ // UL to UR
			SpanCell(GX, GY, 1, GYIdx, WorkPoint);
			} // else if
		if (GY < DefgGridHeight - 1)
			{
			GYIdx = GYp1Idx;
			// UL to LL
			SpanCell(GX, GY, 0, GYIdx, WorkPoint);
			if (GX < DefgGridWidth - 1)
				{
				// Check UL to LR
				if (!SpanCell(GX, GY, 1, GYIdx, WorkPoint))
					{ // need to ensure we don't do double diagonal crossover
					// Check LL to UR (note: we reassign GYIdx and WorkPoint/CenterCellID!)
					GYIdx        = GY0Idx;
					CenterCellID = ID[GYp1Idx + GX];
					WorkPoint    = (InputPoint *)CenterCellID;
					SpanCell(GX, GY, 1, GYIdx, WorkPoint);
					} // if
				} // if
			} // else if
		} // for
	} // for

EndTime = DEFGTime();
ElapsedTime = (EndTime - StartTime);

//snprintf(StatusOut, sizeof(StatusOut), "Total Spans created: %d.\n", TotalSpans); PrintStatus(StatusOut);
//snprintf(StatusOut, sizeof(StatusOut), "Elapsed Span computation time: %f seconds.\n", ElapsedTime); PrintStatus(StatusOut);

//DumpEdges(DEFG_OUTFILE);


//CreateDumpTempElev("c:\\tmp\\A.raw");


//snprintf(StatusOut, sizeof(StatusOut), "Computing Span and Node normals.\n"); PrintStatus(StatusOut);

// ID buf now becomes FinalOutput from now on
ClearFinalOutput();

if (Smoothing)
	{
	Host->ProgressEnd();
	if (Abort) return(0);
	Host->ProgressBegin("Smoothing Nodes", DEFG_SLOPE_SWEEPS);
	if (!CalcNodeSlopes())
		Abort = 1;
	} // if

// the nodes and the edges become points of the rows...
ExtraKnots.clear();
Host->ProgressEnd();
if (Abort) return(0);
Host->ProgressBegin(Smoothing ? "Rasterizing Smoothed Spans": "Rasterizing Spans", TotalSpans);
for (Loop = 0; Loop < LoadedPoints; Loop++)
	{
	if (Smoothing)
		{
		if (!SmoothedInPoints[Loop].Disabled)
			PlotNode((InputPoint *)&SmoothedInPoints[Loop]);
		} // if
	else if (!InPoints[Loop].Disabled)
		{
		PlotNode(&InPoints[Loop]);
		} // else if
	} // for
for (Loop = 0; Loop < TotalSpans; Loop++)
	{
	if (Host->ProgressUpdate(Loop))
		{
		Abort = 1;
		break;
		} // if
	if (Smoothing)
		{
		SmoothRasterizeSpanElev(&SmoothedEdges[Loop]);
		} // if
	else
		{
		RasterizeSpanElev(&Edges[Loop]);
		} // else
	} // for

//DumpElev("c:\\tmp\\LinearElevSpan.raw");
//CreateDumpEdgeBuf("c:\\tmp\\LinearElevSpanChar.raw");

// ...and each row is filled from one of those points to the next
std::sort(ExtraKnots.begin(), ExtraKnots.end());
NextExtra = 0;
Host->ProgressEnd();
if (Abort) return(0);
Host->ProgressBegin(Smoothing ? "Smooth Polygon Filling": "Polygon Filling", DefgGridHeight);

for (GY = 0; GY < DefgGridHeight; GY++)
	{
	double SNXS = 0.0, InX, OutX, InGridX, OutGridX, InZ, OutZ, InSlope, OutSlope;
	if (Host->ProgressUpdate(GY))
		{
		Abort = 1;
		break;
		} // if
	int FoundPointA = 0, TPAX, TPAY;
	double TPAZ, DTPAX;
	GY0Idx  = GY * DefgGridWidth;
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
		if (EdgeBuf[GY0Idx + GX] != -FLT_MAX)
			{
			// the span coming in ends at this cell's leftmost point, the next one starts from its rightmost
			InX = OutX = XCoordBuf[GY0Idx + GX];
			InGridX = OutGridX = fGridFromCoordX(InX);
			InZ = OutZ = EdgeBuf[GY0Idx + GX];
			InSlope = OutSlope = Smoothing ? SNX[GY0Idx + GX]: 0.0;
			for (; NextExtra < ExtraKnots.size() && ExtraKnots[NextExtra].Cell <= (long)(GY0Idx + GX); NextExtra++)
				{
				if (ExtraKnots[NextExtra].Cell != (long)(GY0Idx + GX))
					continue;
				if (ExtraKnots[NextExtra].GridX < InGridX)
					{
					InGridX = ExtraKnots[NextExtra].GridX;
					InX = CoordFromGridX(InGridX);
					InZ = ExtraKnots[NextExtra].Z;
					InSlope = ExtraKnots[NextExtra].RowSlope;
					} // if
				if (ExtraKnots[NextExtra].GridX > OutGridX)
					{
					OutGridX = ExtraKnots[NextExtra].GridX;
					OutX = CoordFromGridX(OutGridX);
					OutZ = ExtraKnots[NextExtra].Z;
					OutSlope = ExtraKnots[NextExtra].RowSlope;
					} // if
				} // for
			if (FoundPointA && GX - TPAX) //lint !e530 // don't scanfill if there's no space between.
				{
				RasterizeSpanElevGridHor(TPAX, DTPAX, TPAY, TPAZ, GX, InX, (float)InZ, SNXS, (float)InSlope); //lint !e530
				} // if
			if (fabs(InGridX - GX) < DEFG_ON_GRID)
				FinalOutput[GY0Idx + GX] = (float)InZ;	// a point exactly on the cell is its elevation, span or no span
			// transfer current location into TPA for next operation
			TPAX = GX;
			DTPAX = OutX;
			TPAY = GY;
			TPAZ = OutZ;
			SNXS = OutSlope;
			FoundPointA = 1;
			} // if
		} // for
	} // for
std::vector<DEFGKnot>().swap(ExtraKnots);

//CreateDumpSNX("c:\\tmp\\SNX.raw");


// for efficiency we can dump our set of points now
if (Smoothing)
	{
	if (SmoothedInPoints)	Host->Free(SmoothedInPoints, MaxPoints * sizeof(struct SmoothedInputPoint));
	SmoothedInPoints = NULL;
	} // if
else
	{
	if (InPoints)	Host->Free(InPoints, MaxPoints * sizeof(struct InputPoint));
	InPoints = NULL;
	} // else

// for stingyness we can dump the edges list at this point
if (Edges && MaxSpans)			Host->Free(Edges, MaxSpans * sizeof(struct Span));
Edges = NULL;
if (SmoothedEdges && MaxSpans)	Host->Free(SmoothedEdges, MaxSpans * sizeof(struct SmoothedSpan));
SmoothedEdges = NULL;



if (Extrapolate)
	{
	//snprintf(StatusOut, sizeof(StatusOut), "Post gap-filling and extrapolating.\n"); PrintStatus(StatusOut);
Host->ProgressEnd();
if (Abort) return(0);
Host->ProgressBegin("Extrapolating", DefgGridHeight + DefgGridWidth);

	FlagInterps();

	Iterations = 0;
	do
		{
		if (Host->ProgressUpdate(Iterations))
			{
			Abort = 1;
			break;
			} // if
		Counter = 0;
/*
		if (Iterations == 52)
			{
			Boop = 1;
			} // if
*/
		for (GY = 0; GY < DefgGridHeight; GY++)
			{
			GYm1Idx = (GY - 1) * DefgGridWidth;
			GY0Idx  = (GY + 0) * DefgGridWidth;
			GYp1Idx = (GY + 1) * DefgGridWidth;
			for (GX = 0; GX < DefgGridWidth; GX++)
				{
				if ((!NonInfectious[GY0Idx + GX]) && (FinalOutput[GY0Idx + GX] != -FLT_MAX))
					{
					if (GY > 0)
						{ // look up one line
						YOff  = -1;
						GYIdx = GYm1Idx;
						if (GX > 0)
							{ // look left
							XOff      = -1;
							Counter  += InfectCellVal(GX, GY, XOff, GYIdx, DEFG_GAPFILL_DIAG_WEIGHT);
							} // if
						if (GX < DefgGridWidth - 1)
							{ // look right
							XOff      = 1;
							Counter  += InfectCellVal(GX, GY, XOff, GYIdx, DEFG_GAPFILL_DIAG_WEIGHT);
							} // if
						// always look above on your column
						if (1) // preserve consistant indent
							{
							XOff      = 0;
							Counter  += InfectCellVal(GX, GY, XOff, GYIdx, DEFG_GAPFILL_VERT_WEIGHT);
							} // if
						} // if
					if (GY < DefgGridHeight - 1)
						{ // look down one line
						YOff  = 1;
						GYIdx = GYp1Idx;
						if (GX > 0)
							{ // look left
							XOff      = -1;
							Counter  += InfectCellVal(GX, GY, XOff, GYIdx, DEFG_GAPFILL_DIAG_WEIGHT);
							} // if
						if (GX < DefgGridWidth - 1)
							{ // look right
							XOff      = 1;
							Counter  += InfectCellVal(GX, GY, XOff, GYIdx, DEFG_GAPFILL_DIAG_WEIGHT);
							} // if
						// always look below on your column
						if (1) // preserve consistant indent
							{
							XOff      = 0;
							Counter  += InfectCellVal(GX, GY, XOff, GYIdx, DEFG_GAPFILL_VERT_WEIGHT);
							} // if
						} // if
					if (1) // preserve consistant indent
						{ // always try left/right on your own line
						YOff  = 0;
						GYIdx = GY0Idx;
						if (GX > 0)
							{ // look left
							XOff      = -1;
							Counter  += InfectCellVal(GX, GY, XOff, GYIdx, DEFG_GAPFILL_HORIZ_WEIGHT);
							} // if
						if (GX < DefgGridWidth - 1)
							{ // look right
							XOff      = 1;
							Counter  += InfectCellVal(GX, GY, XOff, GYIdx, DEFG_GAPFILL_HORIZ_WEIGHT);
							} // if
						} // if
					NonInfectious[GY0Idx + GX] = 1;
					} // if
				} // for
			} // for
		NormalizeExtrap();
		Iterations++;
		} while (Counter);
	//snprintf(StatusOut, sizeof(StatusOut), "Finished gap-filling, %d passes.\n", Iterations); PrintStatus(StatusOut);
	} // if

//snprintf(StatusOut, sizeof(StatusOut), "Writing NullValue.\n"); PrintStatus(StatusOut);
Host->ProgressEnd();
if (Abort) return(0);
Host->ProgressBegin("Nulling", DefgGridHeight);


// convert -FLT_MAX values to Null value
for (GY = 0; GY < DefgGridHeight; GY++)
	{
	if (Host->ProgressUpdate(GY))
		{
		Abort = 1;
		break;
		} // if
	GY0Idx  = (GY + 0) * DefgGridWidth;
	for (GX = 0; GX < DefgGridWidth; GX++)
		{
		if (FinalOutput[GY0Idx + GX] == -FLT_MAX)
			{
			FinalOutput[GY0Idx + GX] = NullVal;
			} // if
/*
		if ((FinalOutput[GY0Idx + GX] != 0.0) && (fabs(FinalOutput[GY0Idx + GX]) < .0001)) // one-tenth of a mm is minimum epsilon value to appease OpenGL
			{
			FinalOutput[GY0Idx + GX] = 0.0f;
			} // if
*/
		} // for
	} // for

FinalTime = DEFGTime();

//CreateDumpTempElev("c:\\tmp\\Filled.raw");
//DumpElev("c:\\tmp\\Final.raw");

ElapsedTime = (FinalTime - BeginTime);

Host->ProgressEnd();
if (Abort)
	return(0);

//snprintf(StatusOut, sizeof(StatusOut), "Total Elapsed Time: %f seconds.\n", ElapsedTime); PrintStatus(StatusOut);

return(1);

} // DEFG::Grid

/*===========================================================================*/

DEFG::DEFG()
{
Host = &DefaultHost;
densify = false;
Extrapolate = 1;	// until SetExtrap or DoGrid says otherwise
RowUnify = ColUnify = NULL;	// no tile-edge unification until SetupUnifyInfo
UnifyNSTileNum = UnifyEWTileNum = UnifyNSTiles = UnifyEWTiles = 0;
TempBuf = NULL;
TempFloat = FinalOutput = EdgeBuf = NULL;
SNX = NULL;
XCoordBuf = NULL;
ID = NULL;
NonInfectious = NULL;

InPoints = NULL;
SmoothedInPoints = NULL;
Edges = NULL;
SmoothedEdges = NULL;

LoadedPoints = TotalSpans = 0;

MaxPoints = MaxSpans = 0;

NullVal = -9999.0f;
SmoothVal = 0.0f;
Smoothing = 0;
PeakRoom = PitRoom = 0.0;

// very permissive bounds
BoundLowX  = -DBL_MAX;
BoundHighX =  DBL_MAX;
BoundLowY  = -DBL_MAX;
BoundHighY =  DBL_MAX;

} // DEFG::DEFG

/*===========================================================================*/

int DEFG::AllocRasterBufs(int FullGridWidth, int FullGridHeight)
{

// <<<>>> TempBuf only needs to be used when extrapolate is on
if (!(TempBuf = (unsigned char *)Host->Alloc(FullGridWidth * FullGridHeight * sizeof(unsigned char))))
	return(0);
if (!(TempFloat = (float *)Host->Alloc(FullGridWidth * FullGridHeight * sizeof(float))))
	return(0);
if (!(FinalOutput = (float *)Host->Alloc(FullGridWidth * FullGridHeight * sizeof(uintptr_t))))	// sized for the ID grid, which shares this block
	return(0);
if (!(EdgeBuf = (float *)Host->Alloc(FullGridWidth * FullGridHeight * sizeof(float))))
	return(0);
if (!(XCoordBuf = (double *)Host->Alloc(FullGridWidth * FullGridHeight * sizeof(double))))
	return(0);
// ID buf now occupies same block as FinalOutput as they are never used at the same time
/*if (!(ID = (unsigned long int *)Host->Alloc(FullGridWidth * FullGridHeight * sizeof(unsigned long int))))
	return(0);*/
if (!(NonInfectious = (unsigned char *)Host->Alloc(FullGridWidth * FullGridHeight * sizeof(unsigned char))))
	return(0);

ID = (uintptr_t *)FinalOutput;

// SNX is a temporary masquerade of TempFloat
SNX = TempFloat;

return(1);

} // DEFG::AllocRasterBufs

/*===========================================================================*/

int DEFG::InitSizes(int FinalGridWidth, int FinalGridHeight, double HOver, double VOver, int NewMaxPoints)
{
int FullGridHeight, FullGridWidth;
int ExtraGridHeight, ExtraGridWidth;
int InitSmoothing = 0;

if (NewMaxPoints == 0)
	{
	Host->LogError("No points selected.");
	return(0);
	} // if

if (SmoothVal > 0.0) InitSmoothing = 1;

WriteGridWidth  = FinalGridWidth;
WriteGridHeight = FinalGridHeight;

ExtraGridWidth  = (int)((double)FinalGridWidth  * (HOver / 100.0));
ExtraGridHeight = (int)((double)FinalGridHeight * (VOver / 100.0));

FullGridWidth  = ExtraGridWidth  + ExtraGridWidth  + FinalGridWidth;
FullGridHeight = ExtraGridHeight + ExtraGridHeight + FinalGridHeight;

WriteGridOffX = ExtraGridWidth;
WriteGridOffY = ExtraGridHeight;

// defer raster alloc until gridding begins because if we're not tiled,
// we might be able to free the initial control point array by then
/*if (!AllocRasterBufs(FullGridWidth, FullGridHeight))
	return(0);
*/

if (InitSmoothing)
	{
	if (!(SmoothedInPoints = (struct SmoothedInputPoint *)Host->Alloc(NewMaxPoints * sizeof(struct SmoothedInputPoint))))
		return(0);
	} // if
else
	{
	if (!(InPoints = (struct InputPoint *)Host->Alloc(NewMaxPoints * sizeof(struct InputPoint))))
		return(0);
	} // else


MaxPoints = NewMaxPoints;
DefgGridWidth = FullGridWidth;
DefgGridHeight = FullGridHeight;
DefgGridCells = DefgGridWidth * DefgGridHeight;

LoadedPoints = 0;

return(1);

} // DEFG::InitSizes

/*===========================================================================*/

int DEFG::InitSpans(int NewMaxSpans)
{

if (Smoothing)
	{
	if (!(SmoothedEdges = (struct SmoothedSpan *)Host->Alloc(NewMaxSpans * sizeof(struct SmoothedSpan))))
		return(0);
	} // if
else
	{
	if (!(Edges = (struct Span *)Host->Alloc(NewMaxSpans * sizeof(struct Span))))
		return(0);
	} // else

MaxSpans = NewMaxSpans;

return(1);

} // DEFG::InitSpans

/*===========================================================================*/

DEFG::~DEFG()
{

if (TempBuf)		Host->Free(TempBuf, DefgGridCells * sizeof(unsigned char));
if (TempFloat)		Host->Free(TempFloat, DefgGridCells * sizeof(float));
if (FinalOutput)	Host->Free(FinalOutput, DefgGridCells * sizeof(uintptr_t));
if (EdgeBuf)		Host->Free(EdgeBuf, DefgGridCells * sizeof(float));
if (XCoordBuf)		Host->Free(XCoordBuf, DefgGridCells * sizeof(double));
// ID buf now occupies same block as FinalOutput as they are never used at the same time
//if (ID)			Host->Free(ID, DefgGridCells * sizeof(unsigned long int));
if (NonInfectious)	Host->Free(NonInfectious, DefgGridCells * sizeof(unsigned char));

// SNX is a masquerades of TempFloat and doesn't get freed

if (InPoints)			Host->Free(InPoints, MaxPoints * sizeof(struct InputPoint));
if (SmoothedInPoints)	Host->Free(SmoothedInPoints, MaxPoints * sizeof(struct SmoothedInputPoint));
if (Edges)				Host->Free(Edges, MaxSpans * sizeof(struct Span));
if (SmoothedEdges)		Host->Free(SmoothedEdges, MaxSpans * sizeof(struct SmoothedSpan));

InPoints = NULL;
SmoothedInPoints = NULL;
Edges = NULL;
SmoothedEdges = NULL;

MaxPoints = MaxSpans = 0;

TempBuf = NULL;
TempFloat = FinalOutput = EdgeBuf = NULL;
XCoordBuf = NULL;
ID = NULL;
NonInfectious = NULL;

} // DEFG::~DEFG

/*===========================================================================*/

bool DEFG::AddPoint(double X, double Y, double Z)
{
bool rVal = false;

if (X >= BoundLowX && X <= BoundHighX && Y >= BoundLowY && Y <= BoundHighY)
	{
	if (Smoothing)
		{
		SmoothedInPoints[LoadedPoints].X = X;
		SmoothedInPoints[LoadedPoints].Y = Y;
		SmoothedInPoints[LoadedPoints].Z = Z;
		SmoothedInPoints[LoadedPoints].PointNum = LoadedPoints;
		SmoothedInPoints[LoadedPoints].NumSpans = SmoothedInPoints[LoadedPoints].MaxSpans = 0;
		SmoothedInPoints[LoadedPoints].SpanBlock = NULL;
		SmoothedInPoints[LoadedPoints].SlopeX = SmoothedInPoints[LoadedPoints].SlopeY = 0.0;
		} // if
	else
		{
		InPoints[LoadedPoints].X = X;
		InPoints[LoadedPoints].Y = Y;
		InPoints[LoadedPoints].Z = Z;
		InPoints[LoadedPoints].PointNum = LoadedPoints;
		InPoints[LoadedPoints].NumSpans = InPoints[LoadedPoints].MaxSpans = 0;
		InPoints[LoadedPoints].SpanBlock = NULL;
		} // if
	rVal = true;
	LoadedPoints++;
	} // if

return(rVal);

} // DEFG::AddPoint

/*===========================================================================*/

// adds pt1 and num2add interpolated points but NOT pt2
bool DEFG::AddPoints(Point3d &pt1, Point3d &pt2, unsigned long num2add)
{
bool rVal = false;

if (pt1[0] >= BoundLowX && pt1[0] <= BoundHighX && pt1[1] >= BoundLowY && pt1[1] <= BoundHighY)
	{
	if (Smoothing)
		{
		SmoothedInPoints[LoadedPoints].X = pt1[0];
		SmoothedInPoints[LoadedPoints].Y = pt1[1];
		SmoothedInPoints[LoadedPoints].Z = pt1[2];
		SmoothedInPoints[LoadedPoints].PointNum = LoadedPoints;
		SmoothedInPoints[LoadedPoints].NumSpans = SmoothedInPoints[LoadedPoints].MaxSpans = 0;
		SmoothedInPoints[LoadedPoints].SpanBlock = NULL;
		SmoothedInPoints[LoadedPoints].SlopeX = SmoothedInPoints[LoadedPoints].SlopeY = 0.0;
		} // if
	else
		{
		InPoints[LoadedPoints].X = pt1[0];
		InPoints[LoadedPoints].Y = pt1[1];
		InPoints[LoadedPoints].Z = pt1[2];
		InPoints[LoadedPoints].PointNum = LoadedPoints;
		InPoints[LoadedPoints].NumSpans = InPoints[LoadedPoints].MaxSpans = 0;
		InPoints[LoadedPoints].SpanBlock = NULL;
		} // if
	LoadedPoints++;
	} // if

// now add num2add interpolated points
double CurX = pt1[0];
double CurY = pt1[1];
double CurZ = pt1[2];
double DeltaX = (pt2[0] - pt1[0]) / (num2add + 1);
double DeltaY = (pt2[1] - pt1[1]) / (num2add + 1);
double DeltaZ = (pt2[2] - pt1[2]) / (num2add + 1);

for(unsigned long AddCount = 0; AddCount < num2add; AddCount++)
	{
	CurX += DeltaX;
	CurY += DeltaY;
	CurZ += DeltaZ;

	if (CurX >= BoundLowX && CurX <= BoundHighX && CurY >= BoundLowY && CurY <= BoundHighY)
		{
		if (Smoothing)
			{
			SmoothedInPoints[LoadedPoints].X = CurX;
			SmoothedInPoints[LoadedPoints].Y = CurY;
			SmoothedInPoints[LoadedPoints].Z = CurZ;
			SmoothedInPoints[LoadedPoints].PointNum = LoadedPoints;
			SmoothedInPoints[LoadedPoints].NumSpans = SmoothedInPoints[LoadedPoints].MaxSpans = 0;
			SmoothedInPoints[LoadedPoints].SpanBlock = NULL;
			SmoothedInPoints[LoadedPoints].SlopeX = SmoothedInPoints[LoadedPoints].SlopeY = 0.0;
			} // if
		else
			{
			InPoints[LoadedPoints].X = CurX;
			InPoints[LoadedPoints].Y = CurY;
			InPoints[LoadedPoints].Z = CurZ;
			InPoints[LoadedPoints].PointNum = LoadedPoints;
			InPoints[LoadedPoints].NumSpans = InPoints[LoadedPoints].MaxSpans = 0;
			InPoints[LoadedPoints].SpanBlock = NULL;
			} // if
		LoadedPoints++;
		} // if
	} // for

return(true);

} // DEFG::AddPoints

/*===========================================================================*/

void DEFG::SetBounds(double NewMinX, double NewMaxX, double NewMinY, double NewMaxY)
{

BoundLowX = simplemin(NewMinX, NewMaxX);
BoundHighX = simplemax(NewMinX, NewMaxX);
BoundLowY = simplemin(NewMinY, NewMaxY);
BoundHighY = simplemax(NewMinY, NewMaxY);

XRange = (BoundHighX - BoundLowX);
YRange = (BoundHighY - BoundLowY);

//snprintf(StatusOut, sizeof(StatusOut), "Lat:  %f to %f.\n", -HighY, -LowY); PrintStatus(StatusOut);
//snprintf(StatusOut, sizeof(StatusOut), "Lon:  %f to %f.\n", -HighX, -LowX); PrintStatus(StatusOut);
//snprintf(StatusOut, sizeof(StatusOut), "Elev: %f to %f.\n", HighZ, LowZ); PrintStatus(StatusOut);

} // DEFG::SetBounds

/*===========================================================================*/

// One pass over the host's control points for DoGrid: counts the points that fall in the tile, or adds them
// to the gridder. Densify adds interpolated points between consecutive points of one object.
class DEFGTilePoints: public DEFGPointSink
	{
	public:
		DEFG *AddTo;	// NULL to count only
		bool densify, first_point;
		int SomeOverscan, InitMaxPoints, NOOSMaxPoints;
		unsigned long adding;
		double minx, maxx, miny, maxy, XStart, XTerm, YStart, YTerm;
		double cell_x, cell_y, last_x, last_y, last_z;

		virtual void BeginObject(void) {first_point = true; adding = 0;};
		virtual void Point(double VX, double VY, double VZ);
	}; // DEFGTilePoints

void DEFGTilePoints::Point(double VX, double VY, double VZ)
{
double add_x, add_y, delta_x, delta_y;

if (densify)
	{
	if (! first_point)
		{
		add_y = add_x = 0;
		adding = 0;
		delta_x = fabs(VX - last_x);
		delta_y = fabs(VY - last_y);
		if (delta_x > cell_x)
			{
			add_x = ceil(delta_x / cell_x) - 1.0;
			} // if
		if (delta_y > cell_y)
			{
			add_y = ceil(delta_y / cell_y) - 1.0;
			} // if
		adding += simplemax((unsigned long)add_x, (unsigned long)add_y);
		} // if
	else
		{
		first_point = false;
		} // else
	last_x = VX;
	last_y = VY;
	last_z = VZ;
	} // if

if (VX >= minx && VX <= maxx && VY >= miny && VY <= maxy)
	{
	if (! AddTo)
		{
		InitMaxPoints++;
		InitMaxPoints += adding;
		// if overscan, count points within non-overscan region
		if (SomeOverscan && VX >= XStart && VX <= XTerm && VY >= YStart && VY <= YTerm)
			{
			NOOSMaxPoints++;
			NOOSMaxPoints += adding;
			} // if
		} // if
	else if (densify && adding)
		{
		Point3d thisPt, lastPt;

		thisPt[0] = VX;
		thisPt[1] = VY;
		thisPt[2] = VZ;
		lastPt[0] = last_x;
		lastPt[1] = last_y;
		lastPt[2] = last_z;
		AddTo->AddPoints(thisPt, lastPt, adding);
		} // else if
	else
		{
		AddTo->AddPoint(VX, VY, VZ);
		} // else
	} // if values in grid region

} // DEFGTilePoints::Point

/*===========================================================================*/

// Grids one tile: gets the control points from the host (twice: to count them, then to load the ones in the
// tile and its overscan), grids them and hands the result to the host.
int DEFG::DoGrid(const DEFGGridSpec &Spec)
{
double LapX, LapY, Horilap = 0.0, Vertlap = 0.0;
DEFGTilePoints Tile;

// x & y nodes are already adjusted for overlap
XStart = Spec.XStart;
XTerm  = Spec.XTerm;
YStart = Spec.YStart;
YTerm  = Spec.YTerm;

Tile.AddTo = NULL;
Tile.densify = densify;
Tile.SomeOverscan = 0;
Tile.InitMaxPoints = Tile.NOOSMaxPoints = 0;
Tile.cell_x = Tile.cell_y = Tile.last_x = Tile.last_y = Tile.last_z = 0.0;
Tile.BeginObject();
if (densify)
	{
	Tile.cell_x = (XTerm - XStart) / (Spec.Cols - 1);
	Tile.cell_y = (YTerm - YStart) / (Spec.Rows - 1);
	} // if

Horilap = Spec.HorOverlap;
Vertlap = Spec.VertOverlap;

LapX = fabs(Horilap * (XTerm - XStart) / 100.0);
LapY = fabs(Vertlap * (YTerm - YStart) / 100.0);

if (Horilap != 0.0 || Vertlap != 0.0)
	{
	Tile.SomeOverscan = 1;
	} // if

Tile.XStart = XStart;
Tile.XTerm = XTerm;
Tile.YStart = YStart;
Tile.YTerm = YTerm;
LowX  = Tile.minx = XStart - LapX;
HighX = Tile.maxx = XTerm + LapX;
LowY  = Tile.miny = YStart - LapY;
HighY = Tile.maxy = YTerm + LapY;

// enumerate total points
if (! Host->EnumeratePoints("Counting Points in Tile", Tile))
	{
	return(0);
	} // if
#ifdef DEFG_LIMIT_INPOINTS
InPointExcess = Tile.InitMaxPoints > DEFG_LIMIT_INPOINTS ? Tile.InitMaxPoints - DEFG_LIMIT_INPOINTS: 0;
#endif // DEFG_LIMIT_INPOINTS

// would default overscan values be ineffective here?
if (Tile.SomeOverscan && Tile.InitMaxPoints == Tile.NOOSMaxPoints && Vertlap == 100.0 && Horilap == 100.0)
	{ // disable overscan
	// reset all values internally to non-overscan versions
	Vertlap = Horilap = 0.0;
	LapX = LapY = 0.0;
	LowX  = Tile.minx = XStart - LapX;
	HighX = Tile.maxx = XTerm + LapX;
	LowY  = Tile.miny = YStart - LapY;
	HighY = Tile.maxy = YTerm + LapY;
	} // if

SetExtrap(Spec.Extrapolate);
SetNullVal(Spec.NullValue);
// must set smooth val before initsizes so we can allocate smoothing data structures
SetSmoothVal(Spec.Smoothing);
SetLimits(Spec.PeakRise, Spec.PitDepth);
if (InitSizes(Spec.Cols, Spec.Rows, Horilap, Vertlap, Tile.InitMaxPoints))
	{
	SetBounds(Tile.minx, Tile.maxx, Tile.miny, Tile.maxy);
	// transfer points to DEFG internal
	Tile.AddTo = this;
	if (! Host->EnumeratePoints("Clipping Points in Tile", Tile))
		{
		return(0);
		} // if

	// we've made our own internal copy of the points
	Host->PointsLoaded();

	if (Grid())
		{
		return(SaveDEM());
		} // if
	} // if

return(0);

} // DEFG::DoGrid

/*===========================================================================*/

// Trims the overscan off the finished grid, makes shared tile edges agree and hands the result to the host.
int DEFG::SaveDEM(void)
{
DEFGGridResult Result;
long x, y, SubsetIdx;

// need to subset grid in-place?
if (WriteGridOffX || WriteGridOffY)
	{
	SubsetIdx = 0;
	for (y = WriteGridOffY; y < DefgGridHeight - WriteGridOffY; y++)
		{
		for (x = WriteGridOffX; x < DefgGridWidth - WriteGridOffX; x++)
			{
			FinalOutput[SubsetIdx++] = FinalOutput[x + y * DefgGridWidth];
			} // for
		} // for
	} // if

if (RowUnify && ColUnify)
	{ // need to store and restore certain samples for edge unification
	float *OutputStrip;
	unsigned long int FullNumRows, FullNumCols;
	
	FullNumRows = (UnifyNSTiles * (WriteGridHeight - 1)) + 1;
	FullNumCols = (UnifyEWTiles * (WriteGridWidth - 1)) + 1;

	// first we write candidate value into edge buffers if none exist there currently
	// after this, we'll come back and read values back, picking up the value we wrote
	// if we were the ones that put them there, or that previous tiles wrote if we didn't
	// put them there.


	// top edge into RowUnify (add 0 for top edge, 1 for bottom edge)
	OutputStrip = &RowUnify[(FullNumCols * (UnifyNSTileNum + 0)) + (UnifyEWTileNum * (WriteGridWidth - 1))];
	y = 0; // top row
	for (x = 0; x < WriteGridWidth; x++)
		{
		if (OutputStrip[x] == -FLT_MAX)
			{
			OutputStrip[x] = FinalOutput[x + y * WriteGridWidth];
			} // if
		else
			{
			// if there's a valid value there already, leave it alone, we'll read it later for unification
			} // else
		} // for

	// bottom edge into RowUnify (add 0 for top edge, 1 for bottom edge)
	OutputStrip = &RowUnify[(FullNumCols * (UnifyNSTileNum + 1)) + (UnifyEWTileNum * (WriteGridWidth - 1))];
	y = WriteGridHeight - 1; // bottom row
	for (x = 0; x < WriteGridWidth; x++)
		{
		if (OutputStrip[x] == -FLT_MAX)
			{
			OutputStrip[x] = FinalOutput[x + y * WriteGridWidth];
			} // if
		else
			{
			// if there's a valid value there already, leave it alone, we'll read it later for unification
			} // else
		} // for

	// left edge into ColUnify (add 0 for left edge, 1 for right edge)
	OutputStrip = &ColUnify[(FullNumRows * (UnifyEWTileNum + 0)) + (UnifyNSTileNum * (WriteGridHeight - 1))];
	x = 0; // leftmost col
	for (y = 0; y < WriteGridHeight; y++)
		{
		if (OutputStrip[y] == -FLT_MAX)
			{
			OutputStrip[y] = FinalOutput[x + y * WriteGridWidth];
			} // if
		else
			{
			// if there's a valid value there already, leave it alone, we'll read it later for unification
			} // else
		} // for

	// right edge into ColUnify (add 0 for left edge, 1 for right edge)
	OutputStrip = &ColUnify[(FullNumRows * (UnifyEWTileNum + 1)) + (UnifyNSTileNum * (WriteGridHeight - 1))];
	x = WriteGridWidth - 1; // rightmost col
	for (y = 0; y < WriteGridHeight; y++)
		{
		if (OutputStrip[y] == -FLT_MAX)
			{
			OutputStrip[y] = FinalOutput[x + y * WriteGridWidth];
			} // if
		else
			{
			// if there's a valid value there already, leave it alone, we'll read it later for unification
			} // else
		} // for


	// Ok, now we turn around an do reads from the edge coherency buffers, reading back any slot that
	// has a valid (non -FLT_MAX) values. Someone else might have just put those values there, in which
	// case we're conforming our edge to theirs (first guy always wins). Or, we might have just put those
	// values there ourselves, in which case nothing changes and the next guys conform to our edge (we're
	// first, we win).

	// top edge from RowUnify (add 0 for top edge, 1 for bottom edge)

	OutputStrip = &RowUnify[(FullNumCols * (UnifyNSTileNum + 0)) + (UnifyEWTileNum * (WriteGridWidth - 1))];
	y = 0; // top row
	for (x = 0; x < WriteGridWidth; x++)
		{
		if (OutputStrip[x] != -FLT_MAX)
			{
			FinalOutput[x + y * WriteGridWidth] = OutputStrip[x];
			} // if
		else
			{
			// if there's a valid value there already, leave it alone, we'll read it later for unification
			} // else
		} // for

	// bottom edge from RowUnify (add 0 for top edge, 1 for bottom edge)
	OutputStrip = &RowUnify[(FullNumCols * (UnifyNSTileNum + 1)) + (UnifyEWTileNum * (WriteGridWidth - 1))];
	y = WriteGridHeight - 1; // bottom row
	for (x = 0; x < WriteGridWidth; x++)
		{
		if (OutputStrip[x] != -FLT_MAX)
			{
			FinalOutput[x + y * WriteGridWidth] = OutputStrip[x];
			} // if
		else
			{
			// if there's a valid value there already, leave it alone, we'll read it later for unification
			} // else
		} // for

	// left edge from ColUnify (add 0 for left edge, 1 for right edge)
	OutputStrip = &ColUnify[(FullNumRows * (UnifyEWTileNum + 0)) + (UnifyNSTileNum * (WriteGridHeight - 1))];
	x = 0; // leftmost col
	for (y = 0; y < WriteGridHeight; y++)
		{
		if (OutputStrip[y] != -FLT_MAX)
			{
			FinalOutput[x + y * WriteGridWidth] = OutputStrip[y];
			} // if
		else
			{
			// if there's a valid value there already, leave it alone, we'll read it later for unification
			} // else
		} // for


	// right edge from ColUnify (add 0 for left edge, 1 for right edge)
	OutputStrip = &ColUnify[(FullNumRows * (UnifyEWTileNum + 1)) + (UnifyNSTileNum * (WriteGridHeight - 1))];
	x = WriteGridWidth - 1; // rightmost col
	for (y = 0; y < WriteGridHeight; y++)
		{
		if (OutputStrip[y] != -FLT_MAX)
			{
			FinalOutput[x + y * WriteGridWidth] = OutputStrip[y];
			} // if
		else
			{
			// if there's a valid value there already, leave it alone, we'll read it later for unification
			} // else
		} // for

	} // if

Result.Elev = FinalOutput;
Result.Cols = WriteGridWidth;
Result.Rows = WriteGridHeight;
Result.XStart = XStart;
Result.XTerm = XTerm;
Result.YStart = YStart;
Result.YTerm = YTerm;
Result.NullValue = NullVal;

return(Host->SaveGrid(Result));

} // DEFG::SaveDEM

/*===========================================================================*/

void DEFG::SetHost(DEFGHost *NewHost)
{

Host = NewHost ? NewHost: &DefaultHost;

} // DEFG::SetHost

/*===========================================================================*/

void DEFG::SetSmoothVal(float NewSmoothVal)
{

SmoothVal = NewSmoothVal;

if (SmoothVal < 0.0)
	SmoothVal = 0.0f;
if (SmoothVal > 100.0)
	SmoothVal = 100.0f;

if (SmoothVal > 0.0)
	Smoothing = 1;
else
	Smoothing = 0;

SSmoothFrac = SmoothVal / 100.0;
LSmoothFrac = 1.0 - SSmoothFrac;

} // DEFG::SetSmoothVal

/*===========================================================================*/

// How far the smoothed surface may go outside the points around a spot, in elevation units: above them (a peak) and
// below them (a pit). 0 keeps it between them; a very large value sets that side free.
// A curve leaves its two ends by at most a quarter of the room its end slopes are given. The surface is made of two
// curves on top of each other, one along an edge and one along a row between edges, so each gets room for half the
// limit: 2 times it.
void DEFG::SetLimits(float PeakRise, float PitDepth)
{

PeakRoom = 2.0 * (PeakRise > 0.0f ? PeakRise: 0.0f);
PitRoom = 2.0 * (PitDepth > 0.0f ? PitDepth: 0.0f);

} // DEFG::SetLimits
