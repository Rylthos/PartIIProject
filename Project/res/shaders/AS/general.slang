#pragma once

#include "../hit_data.slang"
#include "../hit_record.slang"
#include "../lighting.slang"
#include "../ray.slang"

interface IRayMarch
{
  static func traverse(in ray : Ray) -> HitRecord;
}

func render<T>(in pixel_coord : int2,
               in origin : float3,
               in direction : float3,
               in image_size : int2,
               in hit_data_address : uint64_t) -> float4
  where T : IRayMarch
{
  #ifdef CYCLES
  uint64_t start_cycles = clockARB();
  #endif

  Ray ray = Ray(origin, direction);

  HitRecord hit;

  #ifdef GRID_GENERATION_FINISHED
  hit = T::traverse(ray);
  #endif

  #ifdef CYCLES
  uint64_t end_cycles = clockARB();
  #endif

  #ifdef HEATMAP
  if (hit.intersection_checks != 0) {
    return float4(viridis(hit.intersection_checks), 1.);
  } else {
    return float4(0);
  }
  #elif defined(CYCLES)
  float3 mix = lerp(
      MIN_CYCLE_COLOUR, MAX_CYCLE_COLOUR, float3((end_cycles - start_cycles) / float(CYCLE_MAX)));
  return float4(mix, 1.);
  #else
  if (pixel_coord.x == image_size.x / 2 && pixel_coord.y == image_size.y / 2) {
    setHitRecord((HitData*)hit_data_address, hit);
  }

  if (hit.hit) {
    #ifdef SHADOW_RAY
    Ray shadowRay = createShadowRay(hit, LightPos);
    HitRecord shadow = T::traverse(shadowRay);

    return calculateColour(hit, shadow.hit);
    #else
    return calculateColour(hit);
    #endif
  } else {
    return missColour();
  }
  #endif
}
