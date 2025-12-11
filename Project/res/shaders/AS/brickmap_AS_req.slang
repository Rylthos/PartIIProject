struct Brickgrid {
  uint32_t data;

  property bool isValid {
    get { return (data & 0x1) != 0; }
    set { data = (data & ~(0x1)) | ((newValue == true) ? 1 : 0) << 0; }
  }

  property bool isRequested {
    get { return (data & 0x2) != 0; }
    set { data = (data & ~(0x2)) | ((newValue == true) ? 1 : 0) << 1; }
  }

  property bool isEmpty {
    get { return (data >> 2) == 0; }
  }

  property uint32_t getPtr {
    get { return (data >> 2) - 1; }
  }
}

struct Brickmap {
  uint32_t colourPtr;
  uint64_t data[8];
}

[[vk::binding(0, 0)]]
RWStructuredBuffer<Brickgrid> i_Brickgrid;

[[vk::binding(1, 0)]]
StructuredBuffer<Brickmap> i_Brickmap;

[[vk::binding(2, 0)]]
StructuredBuffer<uint8_t> i_Colours;

[[vk::binding(3, 0)]]
StructuredBuffer<uint32_t> i_Request;

[shader("compute")]
[numthreads(1, 1, 1)]
void computeMain(uint3 dispatchThreadID: SV_DispatchThreadID) {
  uint target = i_Request[dispatchThreadID.x + 1];
  i_Brickgrid[target].data &= ~0x3;
  i_Brickgrid[target].data |= 0x1;
}
