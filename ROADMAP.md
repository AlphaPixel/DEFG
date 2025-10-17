# DEFG Modernization Roadmap

This roadmap defines the phases for reviving and modernizing the **DEFG (Decent Extremely Fast Gridder)** codebase.

---

## Phase 1: Build and Compile

**Goal:** Get the original source building cleanly on a modern C++ toolchain (e.g., MSVC 2022, GCC 14, Clang 18) with **minimal code changes**.

**Tasks:**
- [ ] Remove legacy dependencies (e.g., `stdafx.h` precompiled headers).
- [ ] Fix type issues (`unsigned long` pointer truncation, use `uintptr_t`).
- [ ] Replace deprecated C headers and implicit conversions.
- [ ] Resolve warnings-as-errors for modern compilers (narrowing, implicit int conversions, etc.).
- [ ] Verify standalone build from `main.cpp` runs successfully on Windows, Linux, macOS.

**Deliverable:** Successful build and run producing identical output to legacy binaries.

---

## Phase 2: Baseline Testing and Validation

**Goal:** Confirm that modernized builds reproduce legacy behavior.

**Tasks:**
- [ ] Design small test datasets (grid-aligned, sparse, with/without spans).
- [ ] Implement regression tests that run the same inputs and compare output rasters numerically.
- [ ] Automate test execution with CTest or simple Python harness.
- [ ] Validate results against known legacy outputs, tolerating small floating-point variance.

**Deliverable:** Repeatable test suite ensuring functional parity before any refactors.

---

## Phase 3: Immediate Fixes (Correctness & Stability)

**Goal:** Resolve functional errors that cause incorrect results or undefined behavior without altering intended algorithmic design.

**Tasks:**
- [ ] Replace all pointer/ID casts with safe integer or pointer types.
- [ ] Clamp raster write indices (`IX`, `IY`) to grid boundaries.
- [ ] Fix divide-by-zero hazards in coordinate transforms and smoothing math.
- [ ] Correct extrapolation routines to skip unset (`NullVal` / `-FLT_MAX`) cells.
- [ ] Fix time computation bug in `GetSystemTimeFP()` (`tv_usec / 1e6`).
- [ ] Require `fscanf` to read 3 valid floats per line in `LoadPoints`.

**Deliverable:** Stable runtime with defined behavior across all code paths.

---

## Phase 4: Structural Refactoring (Non-Functional Modernization)

**Goal:** Improve code clarity, maintainability, and type safety **without changing core algorithms**.

**Tasks:**
- [ ] Move all global/static buffers to class-managed `std::vector` or RAII types.
- [ ] Use modern `constexpr`, `enum class`, and `struct` for constants and types.
- [ ] Replace manual memory management with smart pointers or containers.
- [ ] Namespace encapsulation for `DEFG`, `DEFGSpline`, and support utilities.
- [ ] Add unit tests for isolated math and interpolation routines.
- [ ] Modernize build system (CMake presets, manifest configuration, CI integration).

**Deliverable:** Clean, warning-free, maintainable code with unchanged output.

---

## Phase 5: Major Feature Restoration and Enhancement

**Goal:** Address long-standing broken or incomplete features (notably smoothing), and prepare the code for parallel or accelerated implementations.

**Tasks:**
- [ ] Re-enable and correct the **span smoothing** logic:
  - Fix incorrect tangent derivation from normals.
  - Implement proper Hermite evaluation using `DEFGSpline` utilities.
  - Validate continuity and stability across test cases.
- [ ] Add C++20 parallel execution policies (`std::for_each(std::execution::par, ...)`) for raster loops.
- [ ] Prototype OpenMP or TBB parallel versions for comparison.
- [ ] Design GPU backend concept (OpenCL, CUDA, or compute shader pipeline) for future extension.

**Deliverable:** Working smoothing mode, parallel-capable gridder prototype.

---

## Phase 6: Future Extensions

**Goal:** Prepare DEFG for modern use as a standalone or embeddable gridding engine.

**Potential Directions:**
- [ ] Export library API as a clean C++ interface.
- [ ] Create Python bindings for rapid testing.
- [ ] Add modern file I/O formats (GeoTIFF, NetCDF, CSV).
- [ ] Support multi-threaded tiling and multi-resolution output.
- [ ] Integrate GPU path using GLSL, Vulkan, or CUDA for high-speed interpolation.

---

## Summary

| Phase | Focus | Outcome |
|-------|--------|----------|
| 1 | Compile | Modern compiler compatibility |
| 2 | Validate | Regression tests for correctness |
| 3 | Fix | Eliminate functional bugs |
| 4 | Refactor | Clean, modern, maintainable code |
| 5 | Enhance | Restore smoothing, add parallelism |
| 6 | Extend | Long-term modernization and GPU path |

---

**Primary Objective:**  
Maintain **functional equivalence** during cleanup and modernization, then progressively unlock dormant and performance features once stability is verified.
