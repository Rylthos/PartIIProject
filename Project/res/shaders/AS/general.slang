#pragma once

#include "../hit_data.slang"
#include "../hit_record.slang"
#include "../lighting.slang"
#include "../ray.slang"
#include "../intersection_colour.slang"
#include "../gBuffer_descriptor.slang"

interface IRayMarch
{
  static func traverse(in ray : Ray) -> HitRecord;
}

func render<T>(in pixel_coord : int2,
               in origin : float3,
               in direction : float3,
               in image_size : int2,
               in hit_data_address : uint64_t) -> void
  where T : IRayMarch
{
  #ifdef CYCLES
    uint64_t start_cycles = clockARB();
  #endif

  Ray ray = Ray(origin, direction);

  HitRecord hit;

  #ifdef GENERATION_FINISHED
    hit = T::traverse(ray);
  #endif

  #ifdef CYCLES
    uint64_t end_cycles = clockARB();
  #endif

  if (pixel_coord.x == image_size.x / 2 && pixel_coord.y == image_size.y / 2) {
    setHitRecord((HitData*)hit_data_address, hit);
  }

  #ifdef HEATMAP
    if (hit.intersection_checks != 0) {
      o_Colours[pixel_coord] = float4(viridis(hit.intersection_checks), 1.);
    } else {
      o_Colours[pixel_coord] = float4(float3(0), 1.);
    }
  #elif defined(CYCLES)
    float3 mix = lerp(
        MIN_CYCLE_COLOUR, MAX_CYCLE_COLOUR, float3((end_cycles - start_cycles) / float(CYCLE_MAX)));

      o_Colours[pixel_coord] = float4(mix, 2.);
  #else
    if (hit.hit) {
      #ifdef SHADOW_RAY
        Ray shadowRay = createShadowRay(hit, LightPos);
        HitRecord shadow = T::traverse(shadowRay);

        return calculateColour(hit, shadow.hit);
      #else

      o_Positions[pixel_coord] = float4(hit.hit_position, 0.);
      o_Colours[pixel_coord] = float4(hit.colour, 0.);
      o_Normals[pixel_coord] = float4(hit.normal, 0.);
      o_Depth[pixel_coord] = 1.f / hit.t;
      #endif
    } else {
      o_Colours[pixel_coord] = float4(0);
      o_Depth[pixel_coord] = 0.f;
    }
  #endif
}
