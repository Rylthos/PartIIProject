#pragma once

struct OctreeNode
{
  uint32_t data;

  __init(uint32_t data) { this.data = data; }

  // Flags
  property bool isSolid
  {
    get { return ((data >> 30) & 0x1) != 0; }
  }
  property bool isFar
  {
    get { return (data & 0x00200000) != 0; }
  }

  // Node properties
  property uint32_t childMask
  { // Only lower 8 bits used
    get { return (data >> 22) & 0xFF; }
  }
  property uint32_t offset
  {
    get { return (data & 0x001FFFFF); }
  }

  property uint32_t farPtr
  {
    get { return data; }
  }

  // Lead properties
  property float r
  {
    get { return ((data & 0x00FF0000) >> 16) / 255.; }
  }
  property float g
  {
    get { return ((data & 0x0000FF00) >> 8) / 255.; }
  }
  property float b
  {
    get { return ((data & 0x000000FF) >> 0) / 255.; }
  }

  property float3 colour
  {
    get { return float3(this.r, this.g, this.b); }
  }
};
