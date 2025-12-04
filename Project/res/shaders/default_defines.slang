#pragma once

#ifndef STEP_LIMIT
#define STEP_LIMIT 100
#endif // STEP_LIMIT

#ifndef VOXEL_SIZE
#define VOXEL_SIZE 1.0
#endif // VOXEL_SIZE

#ifdef HEATMAP
#ifndef INTERSECTION_MAX
#define INTERSECTION_MAX 10
#endif // INTERSECTION_MAX
#endif // HEATMAP

#ifdef CYCLES
#ifndef CYCLE_MAX
#define CYCLE_MAX 10000
#endif // CYCLE_MAX

#define MIN_CYCLE_COLOUR float3(0, 0, 1)
#define MAX_CYCLE_COLOUR float3(1, 0, 0)
#endif // CYCLES

#ifndef EPS
// Small power of 2
#define EPS (asfloat((127 << 23) + (1 << 10)) - 1)
#endif

#ifndef MAX_FLOAT
#define MAX_FLOAT 1e6
#endif

#ifndef MAX_DEPTH
#define MAX_DEPTH 23
#endif
