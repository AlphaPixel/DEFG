// DEFGSupport.cpp
// support code for running DEFG outside of WCS/VNS
// created from scratch and from DEFG.cpp on
// Fri Aug 24, 2001 by CXH

#include <malloc.h>
#include <sys/types.h >
#include <sys/timeb.h>
#include <math.h>
#include "DEFGSupport.h"
#include "DEFG.h"

int LoadPoints(char *InFile, DEFG *DG)
{
FILE *In;
int PointsLoaded = 0;

float A, B, C;

if(In = fopen(InFile, "r"))
	{
	while(fscanf(In, "%f %f %f", &A, &B, &C) != EOF)
		{
		//InPoints[PointsLoaded].X = -A; // if using WCS pos=west longitude
		//InPoints[PointsLoaded].Y = -B; // always, to convert UR positive map form to LR positive array form
		//InPoints[PointsLoaded].Z = C;
		DG->AddPoint(-A, -B, C);
		PointsLoaded++;
		} // while
	fclose(In);
	In = NULL;
	} // if

return(PointsLoaded);
} // LoadPoints

double GetSystemTimeFP(void)
{
double Now = 0.0;
#ifndef __MACH__
struct _timeb TimeBuf;
_ftime(&TimeBuf);
Now = TimeBuf.time + TimeBuf.millitm / 1000.0;
#else // !__MACH__
struct timeval TimeBuf;
gettimeofday( &TimeBuf, NULL );
Now = TimeBuf.tv_sec + TimeBuf.tv_usec / 1000.0;
#endif // __MACH__

return(Now);
} // GetSystemTimeFP

#if defined(WCS_BUILD_VNS) || defined(WCS_BUILD_W6)
// In VNS/W6 builds, MathSupport.h provides these.
  // do nothing; rely on framework headers
#else
  // stand-ins matching WCS Types.h layout
typedef double Point3d[3];
typedef float  Point3f[3];

// --- helpers borrowed from MathSupport.{h,cpp} ---
// FindPosVector(Point3d OP, Point3d EP, Point3d SP)
void FindPosVector(Point3d OP, Point3d EP, Point3d SP)
{
	OP[0] = EP[0] - SP[0];
	OP[1] = EP[1] - SP[1];
	OP[2] = EP[2] - SP[2];
}

// VectorMagnitude(Point3d TP) -> double
double VectorMagnitude(Point3d TP)
{
	return sqrt(TP[0] * TP[0] + TP[1] * TP[1] + TP[2] * TP[2]);
}

// PointDistance(Point3d EP, Point3d SP) -> double
double PointDistance(Point3d EP, Point3d SP)
{
	Point3d TempDist;
	FindPosVector(TempDist, EP, SP);
	return VectorMagnitude(TempDist);
}

// PointDistanceNoSQRT(Point3d EP, Point3d SP) -> double
double PointDistanceNoSQRT(Point3d EP, Point3d SP)
{
	Point3d TempDist;
	FindPosVector(TempDist, EP, SP);
	return (TempDist[0] * TempDist[0] + TempDist[1] * TempDist[1] + TempDist[2] * TempDist[2]);
}


void UnitVector(Point3d UV)
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

void SurfaceNormal(Point3d NP, Point3d FP, Point3d LP)
{

	NP[0] = FP[1] * LP[2] - FP[2] * LP[1];
	NP[1] = FP[2] * LP[0] - FP[0] * LP[2];
	NP[2] = FP[0] * LP[1] - FP[1] * LP[0];

	UnitVector(NP);

} // SurfaceNormal

void NegateVector(Point3d A)
{

	A[0] = -A[0];
	A[1] = -A[1];
	A[2] = -A[2];

} // NegateVector

void AddPoint3d(Point3d A, Point3d Add)
{

	A[0] += Add[0];
	A[1] += Add[1];
	A[2] += Add[2];

} // AddPoint3d

#endif
