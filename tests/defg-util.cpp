// defg-util.cpp
// DEFG's test driver. It is main.cpp of VNS's copy of DEFG (WCS-VNS/DEFG); keep the two the same below this
// comment. It needs only src/DEFG, and CMake builds it as defg-util (the program names itself defg).
//
//   defg <in.xyz> <out.raw>   grids the points of a text file ("x y z" per line) over their own extent into
//                             1000 x 1000 cells and writes the grid as raw floats. No host is involved.
//   defg --selftest           runs DoGrid the way VNS's Terrain Gridder does, with this program as the host
//                             (DEFGHost.h), and checks the result against the hashes VNS's own test expects
//                             (WCS-VNS/tests/TestTerraGridder.cpp): the same points must give the same grid.
//                             Then checks the smoothing against surfaces whose true shape is known.

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "DEFG.h"

// what the smoothing self-test asks for (see SelfTestSmoothing)
#define HILLS_LIMITED	0.5		// rms error at full smoothing with the limits at zero, as a share of the unsmoothed error
#define HILLS_FREE		0.25	// the same without limits
#define LIMIT_SHOWS		0.05	// lifting a limit must let the grid past the points by at least this

static int LoadPoints(const char *InFile, DEFG *DG)
{
FILE *In;
int PointsLoaded = 0;
float A, B, C;

if (In = fopen(InFile, "r"))
	{
	while (fscanf(In, "%f %f %f", &A, &B, &C) == 3)
		{
		DG->AddPoint(-A, -B, C);	// X: WCS longitude is positive west. Y: map north-up to array row-down.
		PointsLoaded++;
		} // while
	fclose(In);
	} // if

return(PointsLoaded);
} // LoadPoints

// A host for the self-test: it supplies the control points of TestTerraGridder.cpp and hashes the grid it is
// given the way that test hashes the DEM VNS saves (columns, rows, then the elevations column by column).
class SelfTestHost: public DEFGHost
	{
	public:
		uint32_t Hash;
		int Stages, CancelAtStage;	// CancelAtStage 0 = never

		SelfTestHost() {Hash = 0; Stages = CancelAtStage = 0;};
		virtual void ProgressBegin(const char *Title, unsigned long Steps) {Stages++;};
		virtual bool ProgressUpdate(unsigned long Step) {return(CancelAtStage && Stages >= CancelAtStage);};
		virtual void LogError(const char *Message) {printf("DEFG error: %s\n", Message);};
		virtual bool EnumeratePoints(const char *Stage, DEFGPointSink &Sink);
		virtual int SaveGrid(const DEFGGridResult &Grid);
	}; // SelfTestHost

bool SelfTestHost::EnumeratePoints(const char *Stage, DEFGPointSink &Sink)
{
static const int Seed[2] = {1, 400}, Count[2] = {90, 70};
int Obj, Ct, N;

for (Obj = 0; Obj < 2; Obj++)
	{
	Sink.BeginObject();
	for (Ct = 0; Ct < Count[Obj]; Ct++)
		{
		N = Seed[Obj] + Ct;
		// latitude 40.0 to 40.1 N, longitude 105.0 to 105.1 W; X is negated longitude, as VNS passes geographic points
		Sink.Point(-(105.0 + 0.1 * (((N * 73) % 256) / 256.0)), 40.0 + 0.1 * (((N * 151) % 256) / 256.0),
			(float)(1000 + ((N * 37) % 101) * 5));
		} // for
	} // for

return(true);
} // SelfTestHost::EnumeratePoints

int SelfTestHost::SaveGrid(const DEFGGridResult &Grid)
{
uint32_t Entries[2];
const unsigned char *Byte;
int Col, Row;
size_t Ct;

Hash = 2166136261u;	// FNV-1a
Entries[0] = (uint32_t)Grid.Cols;
Entries[1] = (uint32_t)Grid.Rows;
for (Byte = (const unsigned char *)Entries, Ct = 0; Ct < sizeof(Entries); Ct++)
	Hash = (Hash ^ Byte[Ct]) * 16777619u;
for (Col = 0; Col < Grid.Cols; Col++)
	{
	for (Row = 0; Row < Grid.Rows; Row++)
		{
		for (Byte = (const unsigned char *)&Grid.Elev[Col + Row * Grid.Cols], Ct = 0; Ct < sizeof(float); Ct++)
			Hash = (Hash ^ Byte[Ct]) * 16777619u;
		} // for
	} // for

return(1);
} // SelfTestHost::SaveGrid

// one tile of 41 x 33 nodes over the test area, set up with the arithmetic of TerraGridder::MakeGrid
// CancelAtStage makes the host cancel in that progress stage: DoGrid must then fail without saving anything
static bool SelfTestTile(const char *Name, double Overlap, float Smoothing, int Extrapolate, uint32_t Expect, int CancelAtStage = 0)
{
SelfTestHost Host;
DEFG Gridder;
DEFGGridSpec Spec;
double West = -105.1, East = -105.0, South = 40.0, North = 40.1;
int Result;

Spec.Cols = 41;
Spec.Rows = 33;
Spec.XStart = West;
Spec.XTerm = West + (East - West);
Spec.YStart = South;
Spec.YTerm = South + (North - South);
Spec.HorOverlap = Spec.VertOverlap = Overlap;
Spec.Extrapolate = Extrapolate;
Spec.NullValue = -9999.0f;
Spec.Smoothing = Smoothing;
Spec.PeakRise = Spec.PitDepth = 0.0f;
Host.CancelAtStage = CancelAtStage;
Gridder.SetHost(&Host);
Result = Gridder.DoGrid(Spec);
if (CancelAtStage)
	{
	printf("%s %-9s cancelled in stage %d: DoGrid returned %d, grid %s\n", ! Result && ! Host.Hash ? "PASS": "FAIL", Name,
		Host.Stages, Result, Host.Hash ? "saved": "not saved");
	return(! Result && ! Host.Hash);
	} // if
printf("%s %-9s hash 0x%08X, VNS gives 0x%08X (%d progress stages)\n", Result && Host.Hash == Expect ? "PASS": "FAIL",
	Name, (unsigned)Host.Hash, (unsigned)Expect, Host.Stages);

return(Result && Host.Hash == Expect);
} // SelfTestTile

// ---- the gridding and the smoothing, checked against surfaces whose true shape is known ----
// 81 points on a 65 x 65 grid over 0..64. On whole cells a plane comes back exact with smoothing off.

static double PlaneDX, PlaneDY;
static double PlaneSurface(double X, double Y) {return(100.0 + PlaneDX * X + PlaneDY * Y);}
static double HillSurface(double X, double Y) {return(100.0 + 20.0 * sin(X / 10.0) * cos(Y / 13.0) + 0.3 * X);}
// level ground at 100 from X = 16 to X = 48, with a bank rising 1.25 per cell on either side: a lake
static double FloorSurface(double X, double Y) {return(100.0 + 1.25 * (X < 16.0 ? 16.0 - X: X > 48.0 ? X - 48.0: 0.0));}
// the same upside down: a mesa
static double MesaSurface(double X, double Y) {return(200.0 - FloorSurface(X, Y));}

static const float Free = 1.0E9f;	// a limit this large is no limit

class SurfaceHost: public DEFGHost
	{
	public:
		double (*Surface)(double X, double Y);
		bool EmptyFloor;	// no points over the level ground of FloorSurface, and none off their column
		bool OffCells;		// the inner points lie between the cells, not on them
		bool TwinShore;		// with EmptyFloor: the next contour line passes through the same cells as the shore
		float Elev[65 * 65];
		double Low, High;	// the lowest and the highest point given

		virtual bool EnumeratePoints(const char *Stage, DEFGPointSink &Sink);
		virtual int SaveGrid(const DEFGGridResult &Grid);
		bool Grid(float Smoothing, float PeakRise = 0.0f, float PitDepth = 0.0f);
		void Level(double &Rise, double &Drop);
		void Error(double &Max, double &Rms);
		void Outside(double &Over, double &Under);
		int Empty(void);
	}; // SurfaceHost

bool SurfaceHost::EnumeratePoints(const char *Stage, DEFGPointSink &Sink)
{
int I, J;
double X, Y, Z;

Low = 1.0E300;
High = -1.0E300;
Sink.BeginObject();
for (J = 0; J <= 8; J++)
	{
	for (I = 0; I <= 8; I++)
		{
		if (EmptyFloor && I > 2 && I < 6)
			continue;
		if (EmptyFloor && TwinShore && (I == 2 || I == 6))
			{ // the shore line 0.4 of a cell inside its column, and the next line 5 up the bank (or down it) 0.4 further out
			Y = 8 * J + ((J > 0 && J < 8) ? ((I * 11 + J * 5) % 5) - 2: 0);
			Z = Surface(32.0, 32.0);
			Sink.Point(I == 2 ? 16.6: 47.4, Y, Z);
			Sink.Point(I == 2 ? 16.2: 47.8, Y, Z + (Surface(0.0, 0.0) > Z ? 5.0: -5.0));
			if (Z < Low) Low = Z - 5.0;
			if (Z > High) High = Z + 5.0;
			continue;
			} // if
		// a lattice 8 cells apart, each inner point moved up to 2 cells off it
		X = 8 * I + ((I > 0 && I < 8 && ! EmptyFloor) ? ((I * 7 + J * 13) % 5) - 2: 0);
		Y = 8 * J + ((J > 0 && J < 8) ? ((I * 11 + J * 5) % 5) - 2: 0);
		if (OffCells && I > 0 && I < 8 && J > 0 && J < 8)
			{ // and then by up to 0.45 of a cell more
			X += ((I * 37 + J * 11) % 10) / 10.0 - 0.45;
			Y += ((I * 13 + J * 29) % 10) / 10.0 - 0.45;
			} // if
		Z = Surface(X, Y);
		if (Z < Low) Low = Z;
		if (Z > High) High = Z;
		Sink.Point(X, Y, Z);
		} // for
	} // for

return(true);
} // SurfaceHost::EnumeratePoints

int SurfaceHost::SaveGrid(const DEFGGridResult &Grid)
{
if (Grid.Cols != 65 || Grid.Rows != 65)
	return(0);
memcpy(Elev, Grid.Elev, sizeof(Elev));
return(1);
} // SurfaceHost::SaveGrid

bool SurfaceHost::Grid(float Smoothing, float PeakRise, float PitDepth)
{
DEFG Gridder;
DEFGGridSpec Spec;

Spec.Cols = Spec.Rows = 65;
Spec.XStart = Spec.YStart = 0.0;
Spec.XTerm = Spec.YTerm = 64.0;
Spec.HorOverlap = Spec.VertOverlap = 0.0;
Spec.Extrapolate = 0;
Spec.NullValue = -9999.0f;
Spec.Smoothing = Smoothing;
Spec.PeakRise = PeakRise;
Spec.PitDepth = PitDepth;
Gridder.SetHost(this);
return(Gridder.DoGrid(Spec) != 0);
} // SurfaceHost::Grid

// the largest and the root-mean-square distance of the grid from the true surface
void SurfaceHost::Error(double &Max, double &Rms)
{
double Off, Sum = 0.0;
int Col, Row, Cells = 0;

Max = 0.0;
for (Row = 0; Row < 65; Row++)
	{
	for (Col = 0; Col < 65; Col++)
		{
		if (Elev[Col + Row * 65] == -9999.0f)
			continue;
		Off = fabs(Elev[Col + Row * 65] - Surface(Col, Row));
		if (Off > Max)
			Max = Off;
		Sum += Off * Off;
		Cells++;
		} // for
	} // for
Rms = Cells ? sqrt(Sum / Cells): 0.0;
} // SurfaceHost::Error

// how far the level ground of FloorSurface or MesaSurface (measured from X = 18 to 46) has risen above and dropped below its level
void SurfaceHost::Level(double &Rise, double &Drop)
{
double Flat = Surface(32.0, 32.0);
int Ct;

Rise = Drop = 0.0;
for (Ct = 0; Ct < 65 * 65; Ct++)
	{
	if (Ct % 65 < 18 || Ct % 65 > 46 || Elev[Ct] == -9999.0f)
		continue;
	if (Elev[Ct] - Flat > Rise) Rise = Elev[Ct] - Flat;
	if (Flat - Elev[Ct] > Drop) Drop = Flat - Elev[Ct];
	} // for
} // SurfaceHost::Level

// how far the grid goes above the highest point given and below the lowest
void SurfaceHost::Outside(double &Over, double &Under)
{
int Ct;

Over = Under = 0.0;
for (Ct = 0; Ct < 65 * 65; Ct++)
	{
	if (Elev[Ct] == -9999.0f)
		continue;
	if (Elev[Ct] - High > Over) Over = Elev[Ct] - High;
	if (Low - Elev[Ct] > Under) Under = Low - Elev[Ct];
	} // for
} // SurfaceHost::Outside

// the cells that got no elevation
int SurfaceHost::Empty(void)
{
int Ct, Cells = 0;

for (Ct = 0; Ct < 65 * 65; Ct++)
	Cells += Elev[Ct] == -9999.0f;
return(Cells);
} // SurfaceHost::Empty

static bool SelfTestSmoothing(void)
{
static const double Slope[4][2] = {{0.5, 0.5}, {0.5, -0.5}, {0.5, 0.0}, {0.0, 0.5}};
static const float Amount[3] = {0.0f, 50.0f, 100.0f};
static SurfaceHost Host;
double Max, Rms[3], RmsFree, OffRms[2], Over[3], Under[3], Rise[4], Drop[4], Worst = 0.0;
int Ct, Holes = 0;
bool Ok = true, Pass = true;

Host.EmptyFloor = Host.OffCells = Host.TwinShore = false;

// A plane must come back as the same plane whichever way it slopes, with the limits at zero and without limits:
// the node normals, the slopes taken from them and the units and signs of both passes all have to be right for that.
// The points reach every edge of the grid, so every cell must get an elevation.
Host.Surface = PlaneSurface;
for (Ct = 0; Ct < 8; Ct++)
	{
	PlaneDX = Slope[Ct / 2][0];
	PlaneDY = Slope[Ct / 2][1];
	Pass = Host.Grid(100.0f, Ct & 1 ? Free: 0.0f, Ct & 1 ? Free: 0.0f) && Pass;
	Host.Error(Max, Rms[0]);
	if (Max > Worst)
		Worst = Max;
	Holes += Host.Empty();
	} // for
Pass = Pass && Worst < 0.001 && ! Holes;
printf("%s smooth plane   largest error %.5f on four planes (limit 0.001), %d cells left empty\n", Pass ? "PASS": "FAIL", Worst, Holes);
Ok = Pass && Ok;

// On rolling ground more smoothing must mean less error, and less again when the limits are lifted.
Host.Surface = HillSurface;
for (Pass = true, Ct = 0; Ct < 3; Ct++)
	{
	Pass = Host.Grid(Amount[Ct]) && Pass;
	Host.Error(Max, Rms[Ct]);
	} // for
Pass = Host.Grid(100.0f, Free, Free) && Pass;
Host.Error(Max, RmsFree);
Pass = Pass && Rms[1] < Rms[0] && Rms[2] < Rms[1] && Rms[2] < HILLS_LIMITED * Rms[0] && RmsFree < HILLS_FREE * Rms[0];
printf("%s smooth hills   rms error %.4f at 0, %.4f at 50, %.4f at 100, %.4f without limits: must fall, to under %g and %g of the first\n",
	Pass ? "PASS": "FAIL", Rms[0], Rms[1], Rms[2], RmsFree, HILLS_LIMITED, HILLS_FREE);
Ok = Pass && Ok;

// Points are used where they are. Moving them off their cells must not cost accuracy, smoothed or not.
Host.OffCells = true;
Pass = Host.Grid(0.0f);
Host.Error(Max, OffRms[0]);
Pass = Host.Grid(100.0f, Free, Free) && Pass;
Host.Error(Max, OffRms[1]);
Host.OffCells = false;
Pass = Pass && OffRms[0] < 1.1 * Rms[0] && OffRms[1] < 1.5 * RmsFree;
printf("%s points off their cells   rms error %.4f at 0 (%.4f on cells), %.4f smoothed (%.4f on cells): within 1.1 and 1.5 times\n",
	Pass ? "PASS": "FAIL", OffRms[0], Rms[0], OffRms[1], RmsFree);
Ok = Pass && Ok;

// The limits. At zero no curve leaves its two ends, so the grid stays between the lowest and the highest point.
// A peak allowance lets it rise above the highest and not drop below the lowest; a pit allowance the reverse.
Pass = Host.Grid(100.0f, 0.0f, 0.0f);
Host.Outside(Over[0], Under[0]);
Pass = Host.Grid(100.0f, Free, 0.0f) && Pass;
Host.Outside(Over[1], Under[1]);
Pass = Host.Grid(100.0f, 0.0f, Free) && Pass;
Host.Outside(Over[2], Under[2]);
Pass = Pass && Over[0] < 0.001 && Under[0] < 0.001 && Over[1] > LIMIT_SHOWS && Under[1] < 0.001 && Over[2] < 0.001 && Under[2] > LIMIT_SHOWS;
printf("%s smooth limits  above highest / below lowest point: %.4f / %.4f at zero, %.4f / %.4f peaks free, %.4f / %.4f pits free\n",
	Pass ? "PASS": "FAIL", Over[0], Under[0], Over[1], Under[1], Over[2], Under[2]);
Ok = Pass && Ok;

// A lake: level ground between two banks. It stays flat with the limits at zero. Freeing the peaks must not dig it,
// and a pit depth of 2, which lets the banks carry on down under it, must drop the floor by no more than 2 and not
// raise it: each limit acts on its own side only.
Host.Surface = FloorSurface;
Host.EmptyFloor = true;
Pass = Host.Grid(100.0f, 0.0f, 0.0f);
Host.Level(Rise[0], Drop[0]);
Pass = Host.Grid(100.0f, Free, 0.0f) && Pass;
Host.Level(Rise[1], Drop[1]);
Pass = Host.Grid(100.0f, 0.0f, 2.0f) && Pass;
Host.Level(Rise[2], Drop[2]);
Pass = Pass && Rise[0] < 0.001 && Drop[0] < 0.001 && Drop[1] < 0.001 && Rise[2] < 0.001 && Drop[2] > LIMIT_SHOWS && Drop[2] <= 2.0;
printf("%s smooth lake    floor dropped %.4f at zero, %.4f peaks free, %.4f with pit depth 2 (must drop, by 2 at most) and rose %.4f\n",
	Pass ? "PASS": "FAIL", Drop[0], Drop[1], Drop[2], Rise[2]);
Ok = Pass && Ok;

// A mesa, the lake upside down: flat at zero, not raised by freeing the pits; a peak rise of 2 rounds its top up by 2 at most.
Host.Surface = MesaSurface;
Pass = Host.Grid(100.0f, 0.0f, 0.0f);
Host.Level(Rise[0], Drop[0]);
Pass = Host.Grid(100.0f, 0.0f, Free) && Pass;
Host.Level(Rise[1], Drop[1]);
Pass = Host.Grid(100.0f, 2.0f, 0.0f) && Pass;
Host.Level(Rise[2], Drop[2]);
Pass = Pass && Rise[0] < 0.001 && Drop[0] < 0.001 && Rise[1] < 0.001 && Drop[2] < 0.001 && Rise[2] > LIMIT_SHOWS && Rise[2] <= 2.0;
printf("%s smooth mesa    top rose %.4f at zero, %.4f pits free, %.4f with peak rise 2 (must rise, by 2 at most) and dropped %.4f\n",
	Pass ? "PASS": "FAIL", Rise[0], Rise[1], Rise[2], Drop[2]);
Ok = Pass && Ok;

// Two contour lines through the same cells: along the lake's shore and the mesa's rim the next line up or down the
// bank shares each cell with the shore line. Averaging the two would put nodes halfway up the bank and run sloping
// triangles from them across the level ground. The cell must keep the line that faces the open ground, both ways up,
// smoothed or not.
Host.TwinShore = true;
for (Pass = true, Worst = 0.0, Ct = 0; Ct < 4; Ct++)
	{
	Host.Surface = (Ct & 1) ? MesaSurface: FloorSurface;
	Pass = Host.Grid(Ct & 2 ? 100.0f: 0.0f) && Pass;
	Host.Level(Rise[0], Drop[0]);
	Worst = simplemax(Worst, simplemax(Rise[0], Drop[0]));
	} // for
Host.TwinShore = false;
Pass = Pass && Worst < 0.001;
printf("%s two lines in one cell   level ground of a lake and of a mesa moved by %.5f, limit 0.001\n", Pass ? "PASS": "FAIL", Worst);
Ok = Pass && Ok;

return(Ok);
} // SelfTestSmoothing

int main(int Count, char *Vector[])
{
DEFG Gridder;
int NumPoints;
bool Ok;

if (Count == 2 && ! strcmp(Vector[1], "--selftest"))
	{
	Ok = SelfTestTile("defaults", 100.0, 100.0f, 1, 0x778DE901u);	// TerraGridder_OneTile_Defaults
	Ok = SelfTestTile("plain", 0.0, 0.0f, 0, 0x849C9249u) && Ok;	// TerraGridder_OneTile_Plain
	Ok = SelfTestTile("cancel", 100.0, 100.0f, 1, 0, 6) && Ok;
	Ok = SelfTestSmoothing() && Ok;
	return(Ok ? 0: 1);
	} // if
if (Count != 3)
	{
	printf("usage: defg <in.xyz> <out.raw>\n       defg --selftest\n");
	return(2);
	} // if

Gridder.InitSizes(1000, 1000, 1.0, 1.0, 600000);
NumPoints = LoadPoints(Vector[1], &Gridder);
printf("Total Points loaded: %d.\n", NumPoints);
if (! NumPoints)
	return(1);
Gridder.AutoBoundPoints();
if (! Gridder.Grid())
	return(1);
Gridder.DumpElev(Vector[2]);

return(0);
} // main
