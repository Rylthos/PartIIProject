#include "hit_record.slang"

static const float3 LightColour = float3(1.);
static const float3 LightPos = float3(0, -1, 0);

func missColour() -> float4 {
  return float4(float3(0.2), 1.);
}

func calculateColour(in hit: HitRecord) -> float4 {
  return calculateColour(hit, false);
}

func calculateColour(in hit: HitRecord, in shadow: bool) -> float4 {
  const float ambient_strength = 0.3;

  const float3 ambient = ambient_strength * LightColour;

  float3 light_dir = normalize(LightPos - hit.hit_position);
  float diffuse_strength = max(dot(hit.normal, light_dir), 0.0);
  float3 diffuse = diffuse_strength * LightColour;

  if (shadow) {
    diffuse = float3(0.);
  }

  return float4(float3(hit.colour * (ambient + diffuse)), 1.0);
}
