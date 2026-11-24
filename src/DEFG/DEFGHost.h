#pragma once

// DEFGHost.h
// Everything DEFG asks of the program it runs in. DEFG includes no host headers: a host (VNS, a test tool)
// derives from DEFGHost and hands the object to DEFG::SetHost(). Every call has a default, so DEFG also
// runs with a plain DEFGHost, or with none at all (it then uses a default instance of its own).

#include <stddef.h>
#include <stdlib.h>

// Receives the control points from DEFGHost::EnumeratePoints.
class DEFGPointSink
	{
	public:
		virtual void BeginObject(void) = 0;						// the points that follow belong to a new object
		virtual void Point(double X, double Y, double Z) = 0;	// X and Y in grid coordinates
	}; // DEFGPointSink

// The grid DEFG::DoGrid is asked to make.
struct DEFGGridSpec
	{
	int Cols, Rows;							// output nodes
	double XStart, XTerm, YStart, YTerm;	// output extent in grid coordinates
	double HorOverlap, VertOverlap;			// overscan around the extent, percent
	int Extrapolate;						// fill cells the points do not reach
	float NullValue;						// written where there is no data
	float Smoothing;						// 0 = none, 100 = full
	float PeakRise, PitDepth;				// how far smoothing may rise above / drop below the points around a spot, elevation units; 0 = not at all
	}; // DEFGGridSpec

// The finished grid, handed to DEFGHost::SaveGrid.
struct DEFGGridResult
	{
	const float *Elev;						// Cols * Rows values, row by row: Elev[Col + Row * Cols]
	int Cols, Rows;
	double XStart, XTerm, YStart, YTerm;	// grid coordinates of column 0, the last column, row 0 and the last row
	float NullValue;
	}; // DEFGGridResult

class DEFGHost
	{
	public:
		virtual ~DEFGHost() {};

		// Memory. Alloc returns zero-filled memory or NULL. Free is given the size that was allocated.
		virtual void *Alloc(size_t Size) {return(calloc(1, Size));};
		virtual void Free(void *Block, size_t Size) {free(Block);};

		// Progress of one stage of the work. ProgressUpdate returns true when the user cancels.
		virtual void ProgressBegin(const char *Title, unsigned long Steps) {};
		virtual bool ProgressUpdate(unsigned long Step) {return(false);};
		virtual void ProgressEnd(void) {};

		// Messages.
		virtual void LogError(const char *Message) {};
		virtual void StatusText(const char *Message) {};

		// Used by DEFG::DoGrid only.
		// EnumeratePoints sends every control point to Sink, calling Sink.BeginObject() before the first point
		// of each object, and returns false if the user cancelled. DoGrid calls it twice: to count, then to load.
		virtual bool EnumeratePoints(const char *Stage, DEFGPointSink &Sink) {return(true);};
		// DEFG now has its own copy of the points; the host may release its own.
		virtual void PointsLoaded(void) {};
		// Stores the result. Returns 0 on failure.
		virtual int SaveGrid(const DEFGGridResult &Grid) {return(1);};
	}; // DEFGHost
