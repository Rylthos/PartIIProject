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
