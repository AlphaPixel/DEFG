// DEFGSupport.h
// support for running DEFG outside of WCS/VNS
// created from scratch and from DEFG.cpp on
// Fri Aug 24, 2001 by CXH


#include <stddef.h>
#include <stdio.h>
#include "DEFG.h"

class DEFG;

#define APPMEM_CLEAR 						 1

void *malloc(size_t AllocSize, unsigned long int Flags);
void free(void *Me);
double GetSystemTimeFP(void);


int LoadPoints(char *InFile, DEFG *DG);

#if defined(WCS_BUILD_VNS) || defined(WCS_BUILD_W6)
// In VNS/W6 builds, MathSupport.h provides these.
  // do nothing; rely on framework headers
#else

// --- Minimal helpers borrowed from MathSupport.{h,cpp} ---
// FindPosVector(Point3d OP, Point3d EP, Point3d SP)
void FindPosVector(Point3d OP, Point3d EP, Point3d SP);

// VectorMagnitude(Point3d TP) -> double
double VectorMagnitude(Point3d TP);

// PointDistance(Point3d EP, Point3d SP) -> double
double PointDistance(Point3d EP, Point3d SP);

// PointDistanceNoSQRT(Point3d EP, Point3d SP) -> double
double PointDistanceNoSQRT(Point3d EP, Point3d SP);

void UnitVector(Point3d UV);

void SurfaceNormal(Point3d NP, Point3d FP, Point3d LP);

void NegateVector(Point3d A);

void AddPoint3d(Point3d A, Point3d Add);


#endif
