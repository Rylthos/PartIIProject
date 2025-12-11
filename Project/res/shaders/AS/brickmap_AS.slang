#include "ray.slang"
#include "hit_data.slang"
#include "hit_record.slang"
#include "lighting.slang"
#include "default_defines.slang"
#include "render_descriptor.slang"
#include "intersection_colour.slang"

struct PushConstants {
  float3 camera_position;
  uint3 brickgrid_dimensions;
  uint64_t hit_data_address;
};

struct Brickgrid {
  uint32_t data;

  property bool isValid {
    get { return (data & 0x1) != 0; }
  }

  property bool isEmpty {
    get { return (data >> 1) == 0; }
  }

  property uint32_t getPtr {
    get { return (data >> 1) - 1; }
  }
}

struct Brickmap {
  uint32_t colourPtr;
  uint64_t data[8];
}

[[vk_push_constant]]
PushConstants push_constants;

[[vk::binding(0, 1)]]
StructuredBuffer<Brickgrid> i_Brickgrid;

[[vk::binding(1, 1)]]
StructuredBuffer<Brickmap> i_Brickmap;

[[vk::binding(2, 1)]]
StructuredBuffer<uint8_t> i_Colours;

func traverse_brick(in ray: Ray, in brick: Brickmap, in step_dir: int3, in step_size: float3, in starting_t: float, in min_bound: float3, inout steps: int, inout hit: HitRecord) -> bool {
  const int3 brick_size = int3(8);

  float3 entry_pos = ray.calculate(starting_t) - min_bound * 8;

  int3 voxel_pos = int3(clamp(floor(entry_pos), int3(0), int3(brick_size - 1)));

  float3 next_dist = abs((voxel_pos + max(step_dir, int3(0)) - entry_pos) * ray.inverse_direction);

  float t = starting_t;

  for (; steps < STEP_LIMIT; steps++) {
#ifdef HEATMAP
    hit.intersection_checks++;
#endif

    uint index = voxel_pos.y;
    uint offset = voxel_pos.x + voxel_pos.z * brick_size.x;

    if (((brick.data[index] >> offset) & 1) != 0) {
      hit.hit = true;

      uint ptr = brick.colourPtr;
      for (int i = 0; i < index; i++) {
        ptr += countbits(brick.data[i]);
      }
      ptr += countbits(brick.data[index] & ((((uint64_t)1) << (offset)) - 1));

      uint8_t r = i_Colours[ptr * 3 + 0];
      uint8_t g = i_Colours[ptr * 3 + 1];
      uint8_t b = i_Colours[ptr * 3 + 2];
      hit.colour = float3(float(r) / 255., float(g) / 255., float(b) / 255.);

      hit.voxel_index = int3(min_bound * 8) + voxel_pos;

      hit.hit_position = ray.calculate(t);

      return true;
    }

    int3 step_axis = next_dist.xyz <= (min(next_dist.yzx, next_dist.zxy));

    t = starting_t + dot(next_dist, step_axis);

    next_dist +=  step_axis * step_size;
    voxel_pos +=  step_axis * step_dir;
    hit.normal = -step_axis * step_dir;


    if (any(voxel_pos < int3(0)) || any(voxel_pos >= brick_size)) {
      return false;
    }
  }

  return false;
}

func traverse(in ray_original: Ray) -> HitRecord {
  const float3 min_bound = float3(0);
  const float3 max_bound = float3(push_constants.brickgrid_dimensions * 8);

  Ray ray = ray_original;

  HitRecord hit;
  float boundingTMin, boundingTMax;
  if (!ray.aabb(min_bound, max_bound, boundingTMin, boundingTMax, EPS, MAX_FLOAT)) {
    return hit;
  }

  float t = boundingTMin;

  float3 entry_pos = ray.calculate(t + EPS) / 8;

  int3 voxel_pos = clamp(int3(floor(entry_pos)), int3(0), int3(push_constants.brickgrid_dimensions));

  int3 step_dir = sign(ray.direction);
  float3 next_dist = abs((voxel_pos + max(step_dir, int3(0)) - entry_pos) * ray.inverse_direction);
  float3 step_size = ray.inverse_direction * step_dir;

  hit.normal = calculateNormal(entry_pos, min_bound, max_bound);;

  for (int i = 0; i < STEP_LIMIT; i++) {
#ifdef HEATMAP
    hit.intersection_checks++;
#endif

    uint index = voxel_pos.x + voxel_pos.z * push_constants.brickgrid_dimensions.x +
    voxel_pos.y * push_constants.brickgrid_dimensions.x * push_constants.brickgrid_dimensions.z;

    Brickgrid grid = i_Brickgrid[index];

    if (!grid.isEmpty) {
      Brickmap brick = i_Brickmap[grid.getPtr];

      if (traverse_brick(ray, brick, step_dir, step_size, t, voxel_pos, i, hit)) {
        return hit;
      }
    }

    int3 step_axis = next_dist.xyz <= (min(next_dist.yzx, next_dist.zxy));

    t = boundingTMin + dot(next_dist, step_axis) * 8;

    next_dist +=  step_axis * step_size;
    voxel_pos +=  step_axis * step_dir;
    hit.normal = -step_axis * step_dir;

    if (any(voxel_pos < int3(0)) || any(voxel_pos >= push_constants.brickgrid_dimensions)) {
      break;
    }
  }

  return hit;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
#ifdef CYCLES
  uint64_t start_cycles = clockARB();
#endif
  int2 pixelCoord = int2(dispatchThreadID.xy);

  Ray ray = Ray(push_constants.camera_position, i_RayDirectionImage[pixelCoord].xyz);

  HitRecord hit;
#ifdef BRICKMAP_GENERATION_FINISHED
  hit = traverse(ray);
#endif

#ifdef CYCLES
  uint64_t end_cycles = clockARB();
#endif

#ifdef HEATMAP
  if (hit.intersection_checks != 0) {
    o_Image[pixelCoord] = float4(viridis(hit.intersection_checks), 1.);
  } else {
    o_Image[pixelCoord] = float4(0);
  }
#elif defined(CYCLES)
  float3 mix = lerp(MIN_CYCLE_COLOUR, MAX_CYCLE_COLOUR, float3((end_cycles - start_cycles) / float(CYCLE_MAX)));
  o_Image[pixelCoord] = float4(mix, 1.);
#else
  int width, height;
  i_RayDirectionImage.GetDimensions(width, height);
  if (pixelCoord.x == width / 2 && pixelCoord.y == height / 2) {
    setHitRecord((HitData*)push_constants.hit_data_address, hit);
  }

  if (hit.hit) {
    o_Image[pixelCoord] = calculateColour(hit);
  } else {
    o_Image[pixelCoord] = missColour();
  }
#endif
}
