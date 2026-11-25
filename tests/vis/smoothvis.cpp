// smoothvis.cpp
// Grids a fixed set of cases with one copy of DEFG and writes each result as raw floats, for defgvis.py to turn
// into numbers and pictures. Run it once per code version or setting, each into its own folder.
//   cl /O2 /EHsc /I<DEFG dir> smoothvis.cpp <DEFG dir>/DEFG.cpp <DEFG dir>/DEFGDiag.cpp <DEFG dir>/DEFGSpline.cpp
//   smoothvis <out dir> <data dir> [peak rise] [pit depth]
// <data dir> holds rmnp42.xyz, Green.xyz and etopo511B.xyz (the data folder of the AlphaPixel/DEFG repository);
// a file that is missing is skipped. The other six cases are synthetic and also get the true surface at every cell.
// To look at an older DEFG, extract its folder (git archive <revision> <DEFG dir> | tar -x -C <somewhere>) and build
// against that; add /DNO_LIMITS for a version from before Peak Rise and Pit Depth.
// Each output: <case>_s<amount>.raw (row-major float32, amount = smoothing 0, 50, 100), <case>_truth.raw,
// <case>.pts ("col row z" per point), and one line per grid in manifest.txt: case amount cols rows.

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include <vector>
#include <string.h>
#include "DEFG.h"

struct Pt {double X, Y, Z;};

class VisHost: public DEFGHost
	{
	public:
		const std::vector<Pt> *Pts;
		std::vector<float> Out;
		int Cols, Rows;
		virtual bool EnumeratePoints(const char *Stage, DEFGPointSink &Sink)
			{
			Sink.BeginObject();
			for (size_t Ct = 0; Ct < Pts->size(); Ct++)
				Sink.Point((*Pts)[Ct].X, (*Pts)[Ct].Y, (*Pts)[Ct].Z);
			return(true);
			};
		virtual int SaveGrid(const DEFGGridResult &G)
			{
			Cols = G.Cols; Rows = G.Rows;
			Out.assign(G.Elev, G.Elev + (size_t)G.Cols * G.Rows);
			return(1);
			};
	}; // VisHost

static std::string OutDir;
static FILE *Manifest;
static float PeakRise, PitDepth;

static void WriteRaw(const std::string &Name, const std::vector<float> &Data)
{
FILE *F = fopen((OutDir + "/" + Name).c_str(), "wb");
if (F) {fwrite(Data.data(), sizeof(float), Data.size(), F); fclose(F);}
} // WriteRaw

// grids Pts over X0..X1, Y0..Y1 at each amount and writes the results
static void GridCase(const char *Name, const std::vector<Pt> &Pts, int Cols, int Rows, double X0, double X1, double Y0, double Y1,
	double (*Truth)(double, double))
{
static const float Amount[3] = {0.0f, 50.0f, 100.0f};
char File[256];
FILE *F;

for (int A = 0; A < 3; A++)
	{
	VisHost Host;
	DEFG Gridder;
	DEFGGridSpec Spec;

	Host.Pts = &Pts;
	memset(&Spec, 0, sizeof(Spec));
#ifndef NO_LIMITS
	Spec.PeakRise = PeakRise;
	Spec.PitDepth = PitDepth;
#endif
	Spec.Cols = Cols; Spec.Rows = Rows;
	Spec.XStart = X0; Spec.XTerm = X1; Spec.YStart = Y0; Spec.YTerm = Y1;
	Spec.HorOverlap = Spec.VertOverlap = 0.0;
	Spec.Extrapolate = 0;
	Spec.NullValue = -9999.0f;
	Spec.Smoothing = Amount[A];
	Gridder.SetHost(&Host);
	if (! Gridder.DoGrid(Spec) || Host.Out.empty())
		{
		printf("%s amount %g FAILED\n", Name, Amount[A]);
		continue;
		} // if
	snprintf(File, sizeof(File), "%s_s%d.raw", Name, (int)Amount[A]);
	WriteRaw(File, Host.Out);
	fprintf(Manifest, "%s %d %d %d\n", Name, (int)Amount[A], Host.Cols, Host.Rows);
	printf("%s amount %g: %d x %d\n", Name, Amount[A], Host.Cols, Host.Rows);
	} // for
if (Truth)
	{
	std::vector<float> T((size_t)Cols * Rows);
	for (int R = 0; R < Rows; R++)
		for (int C = 0; C < Cols; C++)
			T[C + (size_t)R * Cols] = (float)Truth(X0 + C * (X1 - X0) / (Cols - 1), Y0 + R * (Y1 - Y0) / (Rows - 1));
	snprintf(File, sizeof(File), "%s_truth.raw", Name);
	WriteRaw(File, T);
	} // if
snprintf(File, sizeof(File), "%s.pts", Name);
if (F = fopen((OutDir + "/" + File).c_str(), "w"))
	{
	for (size_t Ct = 0; Ct < Pts.size(); Ct++)
		fprintf(F, "%.3f %.3f %.3f\n", (Pts[Ct].X - X0) * (Cols - 1) / (X1 - X0), (Pts[Ct].Y - Y0) * (Rows - 1) / (Y1 - Y0), Pts[Ct].Z);
	fclose(F);
	} // if
} // GridCase

// ---- synthetic surfaces on a 257 x 257 grid over 0..256; every point sits on a whole cell ----
static const int N = 257;
static const double PI = 3.14159265358979;

static double Plane(double X, double Y) {return(200.0 + 0.5 * X - 0.5 * Y);}
static double Hills(double X, double Y) {return(200.0 + 40.0 * sin(X / 40.0) * cos(Y / 52.0) + 0.3 * X);}
static double HillR(double Th) {return(70.0 * (1.0 + 0.22 * cos(3.0 * Th) + 0.10 * sin(5.0 * Th)));}
static double Hill(double X, double Y)
{
double DX = X - 128.0, DY = Y - 128.0, R = sqrt(DX * DX + DY * DY), RR = HillR(atan2(DY, DX));
return(100.0 + 120.0 * exp(-(R * R) / (RR * RR)));
} // Hill
static double Cliff(double X, double Y)
{
return(100.0 + 60.0 / (1.0 + exp(-(X - 128.0 + 24.0 * sin(Y / 30.0)) / 3.0)) + 0.05 * Y);
} // Cliff

static unsigned long Seed;
static double Rand(void) {Seed = Seed * 1664525UL + 1013904223UL; return(((Seed >> 8) & 0xFFFFFF) / 16777216.0);}

static std::vector<Pt> Lattice(double (*F)(double, double), int Step, int Jitter)
{
std::vector<Pt> P;
int Count = (N - 1) / Step;
for (int J = 0; J <= Count; J++)
	for (int I = 0; I <= Count; I++)
		{	// jittered lattice; the outer rows and columns stay on the boundary
		double X = Step * I + ((I > 0 && I < Count) ? ((I * 7 + J * 13) % (2 * Jitter + 1)) - Jitter: 0);
		double Y = Step * J + ((J > 0 && J < Count) ? ((I * 11 + J * 5) % (2 * Jitter + 1)) - Jitter: 0);
		Pt Q = {X, Y, F(X, Y)};
		P.push_back(Q);
		} // for
return(P);
} // Lattice

// the same lattice with every inner point moved off its cell by a fraction of a cell
static std::vector<Pt> LatticeFrac(double (*F)(double, double), int Step, int Jitter)
{
std::vector<Pt> P = Lattice(F, Step, Jitter);
for (size_t Ct = 0; Ct < P.size(); Ct++)
	{
	if (P[Ct].X > 0 && P[Ct].X < N - 1) P[Ct].X += ((Ct * 37) % 100) / 100.0 - 0.495;
	if (P[Ct].Y > 0 && P[Ct].Y < N - 1) P[Ct].Y += ((Ct * 61) % 100) / 100.0 - 0.495;
	P[Ct].Z = F(P[Ct].X, P[Ct].Y);
	} // for
return(P);
} // LatticeFrac

static void Border(std::vector<Pt> &P, double (*F)(double, double), int Step)
{
for (int I = 0; I < N; I += Step)
	{
	Pt A = {(double)I, 0.0, F(I, 0)}, B = {(double)I, N - 1.0, F(I, N - 1)}, C = {0.0, (double)I, F(0, I)}, D = {N - 1.0, (double)I, F(N - 1, I)};
	P.push_back(A); P.push_back(B); P.push_back(C); P.push_back(D);
	} // for
} // Border

static std::vector<Pt> Scatter(double (*F)(double, double), int Count, unsigned long NewSeed)
{
std::vector<Pt> P;
Seed = NewSeed;
Border(P, F, 32);
for (int Ct = 0; Ct < Count; Ct++)
	{
	double X = floor(1.0 + Rand() * (N - 2)), Y = floor(1.0 + Rand() * (N - 2));
	Pt Q = {X, Y, F(X, Y)};
	P.push_back(Q);
	} // for
return(P);
} // Scatter

// points along the contour lines of Hill, as digitized contours would give them; no summit point
static std::vector<Pt> Contours(void)
{
std::vector<Pt> P;
Border(P, Hill, 32);
for (double Level = 110.0; Level <= 215.0; Level += 15.0)
	{
	double K = sqrt(-log((Level - 100.0) / 120.0)), LastX = -1.0, LastY = -1.0;
	for (int Step = 0; Step < 720; Step++)
		{
		double Th = Step * PI / 360.0, R = HillR(Th) * K;
		double X = floor(128.0 + R * cos(Th) + 0.5), Y = floor(128.0 + R * sin(Th) + 0.5);
		if (X < 1 || Y < 1 || X > N - 2 || Y > N - 2) continue;
		if (fabs(X - LastX) + fabs(Y - LastY) < 5.0) continue;	// a point every 5 cells or so along the line
		Pt Q = {X, Y, Level};	// a digitized contour carries its level, not the exact surface value
		P.push_back(Q);
		LastX = X; LastY = Y;
		} // for
	} // for
return(P);
} // Contours

static void FileCase(const char *Name, const std::string &Path, int LongSide)
{
std::vector<Pt> P;
double A, B, C, X0 = 1e300, X1 = -1e300, Y0 = 1e300, Y1 = -1e300;
FILE *In = fopen(Path.c_str(), "r");
if (! In) {printf("%s: cannot open %s\n", Name, Path.c_str()); return;}
while (fscanf(In, "%lf %lf %lf", &A, &B, &C) == 3)
	{	// as DEFG's own reference driver: WCS longitude is positive west, and north goes to row 0
	Pt Q = {-A, -B, C};
	P.push_back(Q);
	if (Q.X < X0) X0 = Q.X; if (Q.X > X1) X1 = Q.X; if (Q.Y < Y0) Y0 = Q.Y; if (Q.Y > Y1) Y1 = Q.Y;
	} // while
fclose(In);
double W = X1 - X0, H = Y1 - Y0;
int Cols = W >= H ? LongSide: (int)(LongSide * W / H + 0.5), Rows = W >= H ? (int)(LongSide * H / W + 0.5): LongSide;
GridCase(Name, P, Cols, Rows, X0, X1, Y0, Y1, NULL);
} // FileCase

int main(int Count, char *Vector[])
{
if (Count < 3 || Count > 5) {printf("usage: smoothvis <out dir> <data dir> [peak rise] [pit depth]\n"); return(2);}
OutDir = Vector[1];
if (Count > 3) PeakRise = (float)atof(Vector[3]);
if (Count > 4) PitDepth = (float)atof(Vector[4]);
if (! (Manifest = fopen((OutDir + "/manifest.txt").c_str(), "w"))) return(1);
GridCase("plane", Lattice(Plane, 16, 5), N, N, 0, N - 1, 0, N - 1, Plane);
GridCase("hills", Lattice(Hills, 16, 5), N, N, 0, N - 1, 0, N - 1, Hills);
GridCase("hillsfrac", LatticeFrac(Hills, 16, 5), N, N, 0, N - 1, 0, N - 1, Hills);
GridCase("scatter", Scatter(Hills, 260, 12345UL), N, N, 0, N - 1, 0, N - 1, Hills);
GridCase("contours", Contours(), N, N, 0, N - 1, 0, N - 1, Hill);
GridCase("cliff", Scatter(Cliff, 420, 777UL), N, N, 0, N - 1, 0, N - 1, Cliff);
FileCase("rmnp42", std::string(Vector[2]) + "/rmnp42.xyz", 257);
FileCase("green", std::string(Vector[2]) + "/Green.xyz", 480);
FileCase("etopo", std::string(Vector[2]) + "/etopo511B.xyz", 800);
fclose(Manifest);
return(0);
} // main
