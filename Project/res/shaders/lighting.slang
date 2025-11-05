#include "hit_record.slang"

func calculateColour(in hit: HitRecord, in light_pos: float3, in light_colour: float3) -> float3 {
  const float ambient_strength = 0.1;

  const float3 ambient = ambient_strength * light_colour;

  float3 light_dir = normalize(light_pos - hit.hit_position);
  float diffuse_strength = max(dot(hit.normal, light_dir), 0.0);
  float3 diffuse = diffuse_strength * light_colour;

  return hit.colour * (ambient + diffuse);
}
