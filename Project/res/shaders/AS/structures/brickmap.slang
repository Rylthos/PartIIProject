#pragma once

struct Brickgrid
{
  uint32_t data;

  property bool isValid
  {
    get { return (data & 0x1) != 0; }
    set { data = (data & ~(0x1)) | ((newValue == true) ? 1 : 0) << 0; }
  }

  property bool isRequested
  {
    get { return (data & 0x2) != 0; }
    set { data = (data & ~(0x2)) | ((newValue == true) ? 1 : 0) << 1; }
  }

  property bool isEmpty
  {
    get { return (data >> 2) == 0; }
  }

  property uint32_t getPtr
  {
    get { return (data >> 2) - 1; }
  }
}

struct Brickmap
{
  uint32_t colourPtr;
  uint64_t data[8];
}
