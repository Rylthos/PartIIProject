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

#define MIN_INTERSECTION_COLOUR float3(0.2, 0.2, 0.8)
#define MAX_INTERSECTION_COLOUR float3(0.9, 0.4, 0.05)
#endif // HEATMAP

#ifdef CYCLES
#ifndef CYCLE_MAX
#define CYCLE_MAX 10000
#endif // CYCLE_MAX

#define MIN_CYCLE_COLOUR float3(0, 0, 1)
#define MAX_CYCLE_COLOUR float3(1, 0, 0)
#endif // CYCLES

#ifndef EPS
#define EPS 1e-5
#endif

#ifndef MAX_FLOAT
#define MAX_FLOAT 1e6
#endif

#ifndef MAX_DEPTH
#define MAX_DEPTH 10
#endif
