#pragma once

struct Brickgrid
{
  uint32_t data;

  property bool isValid
  {
    get { return (data & 0x1) != 0; }
    set { data = (data & ~0x1) | ((newValue == true) ? 1 : 0); }
  }

  property bool isRequested
  {
    get { return (data & 0x2) != 0; }
    set { data = (data & ~0x2) | (((newValue == true) ? 1 : 0) << 1); }
  }

  property bool isEmpty
  {
    get { return (data >> 2) == 0; }
    set { if (newValue == true) { data &= 0x3; } }
  }

  property uint32_t getPtr
  {
    get { return (data >> 2) - 1; }
    set {
      data &= 0x3;
      data |= ((newValue + 1) << 2); }
  }
}

struct Brickmap
{
  uint64_t colourPtr;
  uint64_t data[8];
}

struct Colour {
  uint8_t data;
  uint8_t r;
  uint8_t g;
  uint8_t b;

  property uint8_t type {
    get { return data & 0x3; }
    set { data &= ~0x3; data |= (newValue & 0x3); }
  }

  property bool isUsed {
    get { return (data & 0x80) != 0; }
    set { data &= ~0x80; data |= (uint8_t)((newValue == true) ? 0x80 : 0x00); }
  }
};

inline func getTypeSize(uint8_t type) -> uint {
  switch (type) {
    case 0: return 8*8*8;
    case 1: return 4*4*4;
    case 2: return 2*2*2;
  }
  return 0;
}

inline func getSetBits(in brick: Brickmap, in index: int3) -> uint {
  uint ptr = 0;
  for (int i = 0; i < index.y; i++) {
    ptr += countbits(brick.data[i]);
  }

  if (index.y != 8) {
    uint offset = index.x + index.z * 8;
    uint64_t mask = ((uint64_t)1 << offset) - 1;
    ptr += countbits(brick.data[index.y] & mask);
  }

  return ptr;
}
