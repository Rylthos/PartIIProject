#pragma once

struct ContreeNode
{
  uint64_t high;
  uint64_t low;

  property bool isSolid
  {
    get { return ((high >> 56) & 0x1) != 0; }
  }

  // Nodes
  property float nodeR
  {
    get { return ((high >> 16) & 0xFF) / 255.; }
  }

  property float nodeG
  {
    get { return ((high >> 8) & 0xFF) / 255.; }
  }

  property float nodeB
  {
    get { return ((high >> 0) & 0xFF) / 255.; }
  }

  property float3 nodeColour
  {
    get { return float3(nodeR, nodeG, nodeB); }
  }

  property uint32_t offset
  {
    get { return (uint32_t)(high & 0xFFFFFFFF); }
  }

  property uint64_t childMask
  {
    get { return low; }
  }

  // Leafs
  property float r
  {
    get { return (high & 0xFFFFFFFF) / (float)0xFFFF; }
  }

  property float g
  {
    get { return ((low >> 32) & 0xFFFFFFFF) / (float)0XFFFF; }
  }

  property float b
  {
    get { return (low & 0xFFFFFFFF) / (float)0xFFFF; }
  }

  property float3 colour
  {
    get { return float3(r, g, b); }
  }
}
