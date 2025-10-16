# DEFG - Decent Extremely Fast Gridder

Spun off from VNS
https://github.com/AlphaPixel/3DNature/tree/master/WCS-VNS/DEFG

**DEFG** is a lightweight C++ library for converting irregularly spaced 3D point data — such as survey points or elevation samples — into a regular raster grid (Digital Elevation Model).  
It supports optional *breaklines* (“spans”) for enforcing ridge and stream continuity, and can fill or extrapolate gaps in sparse datasets to produce a complete terrain surface.

---

## Overview

DEFG reads a set of scattered XYZ points (and optional connecting line segments), analyzes their spatial distribution, and produces a raster DEM grid covering the bounding region.  
It uses a hybrid of distance-weighted interpolation, span rasterization, and smoothing passes to generate stable surfaces even from irregular data.

The library is self-contained and written in standard C++. It has no external dependencies beyond the C standard library and basic math routines.

## Deeper overview

A stand-alone/scoped “Decent Extremely Fast Gridder” (DEFG) that converts scattered 3D points and optional breaklines (“spans”) into a raster elevation grid (DEM), with optional smoothing and extrapolation to fill gaps. Core data structures include InputPoint/SmoothedInputPoint (X,Y,Z, point id, flags, per-point normal when smoothed), Span/SmoothedSpan (point indices, flags, precomputed slope/normal), and multiple working rasters/buffers: TempBuf (flags), TempFloat (distance/weight), FinalOutput (DEM), EdgeBuf, XCoordBuf, ID, and NonInfectious, along with grid/bounds and precomputed coordinate-mapping parameters. Public API exposes initialization, bounds/null/smoothing configuration, point/edge ingestion, gridding, DEM save/higher-level integration, and diagnostics. 

Coordinate handling/precompute: the gridder computes min/max of X,Y,Z across loaded points; establishes mapping between world coordinates and grid indices with cached factors for fast index/coordinate conversion; provides integer and floating versions of grid↔coord transforms. It also reports lat/lon/elev ranges during autobounding. 

Gap detection and extrapolation: cells needing interpolation/extrapolation are flagged; propagation (“infection”) routines spread values into empty cells using weighted distances and a NonInfectious mask; normalization pass scales accumulated weights. Supporting utilities include squared-distance evaluation and an atan2-like helper used for slope/angle work. 

Span rasterization and smoothing: spans between points are rasterized across the grid to write/interpolate elevation along lines; “smooth” variants use precomputed per-span/per-point normals and a binning scheme to influence interpolation across edges/ridges. There are helpers to rasterize spans either via direct point stepping or grid-aligned passes, compute span normals from endpoints, and accumulate smoothed contributions into the FinalOutput raster. (Interfaces and intent are declared in the header; implementations sit with the other raster routines.) 

Spline evaluation for line sampling: DEFGSpline implements cubic Hermite interpolation (Catmull-Rom-style) over scalar samples with distances, with two modes: global-T (uniform tension factor via OneMinusT) and a no-TCB variant; optional per-node Tension/Continuity/Bias paths are compiled behind a macro. Tangents are distance-weighted combinations of neighbor differences; evaluation uses standard Hermite basis h1..h4 on the normalized segment parameter. These spline evaluators are intended for generating smooth values along polylines used as breaklines. 

Standalone support and test harness: when not built inside the larger application, minimal wrappers provide zeroed/heap allocation, a portable millisecond timer, and an ASCII “XYZ” loader that negates X and Y to match the grid’s orientation before calling AddPoint. The sample main configures grid size and capacity, loads points (from argv or a default), autobounds, then calls Grid() to produce the DEM. 
GitHub

Key class API: InitSizes/InitSpans/AllocRasterBufs; SetExtrap/SetBounds/SetNullVal/SetSmoothVal/SetDensify/SetupUnifyInfo; Clear* buffers; AutoBoundPoints; SetupGridCoord*/GridFromCoord*/CoordFromGrid*; EvalDistNoSq; FlagInterps/InfectCell*/NormalizeExtrap; AddSpanToNode/SpanCell; RasterizeSpanElev* and SmoothRasterizeSpanElev*; CalcSpanNormals*; AddPoint/AddPoints; SaveDEM; DoGrid; plus optional diagnostic dump/print helpers and a spline test function.

---

## Key Features

- **Point-to-Grid Conversion**  
  Converts scattered (x, y, z) samples into a regularly spaced elevation raster.

- **Automatic Bounding and Scaling**  
  Automatically computes dataset bounds and establishes mapping between world coordinates and grid indices.

- **Breakline Support (Spans)**  
  Accepts line segments between points to enforce continuity across ridges, valleys, or man-made features.

- **Smoothing and Normal-Based Blending**  
  Optional smoothing uses precomputed point and span normals to reduce artifacts and blend elevations across features.

- **Gap Detection and Extrapolation**  
  Detects empty cells and fills them using weighted propagation from known cells, with controllable infection and normalization passes.

- **Spline Interpolation Utilities**  
  Includes cubic Hermite spline (Catmull–Rom–style) functions for smooth interpolation along lines or sampled paths.

- **Standalone Test Harness**  
  A minimal `main.cpp` demonstrates how to load an ASCII XYZ file, run the gridder, and produce a DEM output.

---

## Data Structures

- **InputPoint / SmoothedInputPoint** – Holds raw and optionally smoothed XYZ samples and flags.  
- **Span / SmoothedSpan** – Represents linear features connecting points with optional normal vectors.  
- **Raster Buffers** – Multiple working arrays track elevation, distance weighting, flags, IDs, and interpolation status.

---


## Spline Utilities

The DEFGSpline module provides cubic Hermite interpolation over irregular samples, with optional Tension/Continuity/Bias modes (compiled via macros).
These are primarily used to generate smooth profiles along spans, but can be reused independently for general curve evaluation.

---

## Breaklin/Span handling

Breaklines (“spans”) in DEFG are treated as linear geometric constraints rather than as additional point samples. Their handling differs from normal point interpolation in both data representation and rasterization behavior:

1. Representation

Each breakline is stored as a Span object linking two input points by index.

When smoothing is enabled, a SmoothedSpan also stores precomputed normals and slope vectors based on the endpoint elevations.

These spans form a secondary dataset parallel to the scattered points, allowing DEFG to maintain directional continuity (along rivers, ridges, roads, etc.) across the grid.

2. Processing Differences
a. Rasterization Path

Regular points contribute elevation directly to the nearest cell(s) based on their X/Y grid indices.

Spans are rasterized along their length — the code iterates through all grid cells intersected by the line segment between endpoints.

Each visited cell receives an interpolated elevation computed from the span endpoints (or spline-smoothed intermediate values when smoothing is active).

b. Normal Influence (Smoothed Mode)

Smoothed spans carry surface orientation information (normals) that affects how neighboring cells blend.

The interpolation across a span is not purely linear in Z; the slope and local tangent vectors influence how the height transitions, creating smoother terrain continuity.

c. Weighting and Priority

Span-contributed cells are treated as “known” values with higher confidence than interpolated fill cells.

This prevents the later extrapolation step from “erasing” or smearing over ridge or valley lines encoded as spans.

3. Integration with Gridding and Extrapolation

After span rasterization:

Point and span contributions are merged in the temporary elevation and distance buffers.

The infection/extrapolation pass fills remaining gaps outward from both point-based and span-based sources.

Because spans already define continuous chains of populated cells, the extrapolation spreads away from those linear features rather than across them, preserving edge integrity.

4. End Result

The resulting DEM differs from a pure point-interpolated surface in that:

Linear terrain features (streams, roads, ridgelines) are preserved as continuous, correctly sloped paths.

The surface transitions smoothly along these features and resists cross-line smearing.

Gaps between breaklines and scattered points are filled more naturally, producing a more realistic and hydrologically consistent terrain.

In short, points define discrete height samples, while breaklines define continuous structural features that shape the terrain’s geometry during gridding.


## Implementation Summary

Internally, DEFG:

Scans input points to determine min/max bounds.

Establishes fast coordinate ↔ grid index transforms.

Rasterizes spans and points into grid cells.

Flags gaps and propagates values into unfilled regions.

Optionally normalizes or smooths the surface.

Exposes the resulting elevation array for export or visualization.

---

## Typical Usage

```cpp
DEFG gridder;

// 1. Initialize dimensions and capacity
gridder.InitSizes(gridWidth, gridHeight, hOversample, vOversample, maxPoints);

// 2. Optionally initialize breaklines
gridder.InitSpans(maxSpans);

// 3. Configure parameters
gridder.SetNullVal(-9999.0f);
gridder.SetSmoothVal(0.25f);
gridder.SetExtrap(1);   // enable extrapolation

// 4. Load input points
gridder.AddPoint(x, y, z);  // repeat for all points

// 5. Auto-compute bounding box
gridder.AutoBoundPoints();

// 6. Execute gridding
gridder.Grid();

// 7. Retrieve or save DEM
gridder.SaveDEM("output.dem");
```

---

## Step by step workflow:

Construct a DEFG.

1. Initialize sizes/capacity:
InitSizes(finalGridWidth, finalGridHeight, hOversample, vOversample, maxPoints). Optional: InitSpans(maxSpans) if you’ll supply breaklines/spans. Optional (tiled/partial writing): AllocRasterBufs(fullGridWidth, fullGridHeight). 

2. Configure behavior (optional):
SetExtrap(int), SetNullVal(float), SetSmoothVal(float), SetDensify(bool). If gridding into tiles and later unifying, call SetupUnifyInfo(rowUnify, colUnify, nsTileNum, ewTileNum, nsTiles, ewTiles). 

3. Load inputs:
Add scattered points with AddPoint(double x, double y, double z) or AddPoints(Point3d&, Point3d&, unsigned long). If using breaklines, allocate spans (after InitSpans) and associate them with endpoints via AddSpanToNode(InputPoint*, Span*). 

4. Define grid bounds:
Either explicitly SetBounds(minX, maxX, minY, maxY) or compute from loaded points via AutoBoundPoints(). 

5. Prepare internal grid↔world mapping (auto inside gridding; available to caller if needed):
SetupGridCoordX/Y, GridFromCoordX/Y, fGridFromCoordX/Y, CoordFromGridX/Y. 

6. Run the gridder:
Call Grid() to rasterize point/spans, fill cells, mark gaps, extrapolate/normalize as configured. The test harness does: load → AutoBoundPoints() → Grid(). 

7. Retrieve or persist results:
Access the DEM via GetFinalArray(), GetFinalArrayWidth(), GetFinalArrayHeight(), or write using SaveDEM(...). 

8. Diagnostics (optional, if enabled):
DumpPoints/ID/TempBuf/Dist/Elev/Edges, CreateDumpEdgeBuf/CreateDumpTempElev/CreateDumpTempDist/CreateDumpSNX, PrintStatus. 

Typical minimal standalone flow (as in main.cpp): construct DEFG, call InitSizes(...), load points (e.g., via LoadPoints helper that calls AddPoint), AutoBoundPoints(), Grid().