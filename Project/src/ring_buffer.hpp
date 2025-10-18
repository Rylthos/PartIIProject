#pragma once

#include <array>
#include <cstddef>

#include "logger.hpp"

template <typename T, size_t Size> class RingBuffer {
  public:
    RingBuffer() { }
    ~RingBuffer() { }

    void pushBack(T data)
    {
        m_Data[m_Back] = data;
        m_Back = (m_Back + 1) % Size;

        if (m_Full) {
            m_Front = (m_Front + 1) % Size;
        }

        m_Full = m_Front == m_Back;
    }

    constexpr size_t getSize() const { return Size; }

    std::array<T, Size> getData()
    {
        std::array<T, Size> returnData;

        for (size_t i = 0; i < returnData.size(); i++) {
            size_t index = (i + m_Front) % returnData.size();
            returnData[i] = m_Data[index];
        }

        return returnData;
    }

  private:
    std::array<T, Size> m_Data;

    size_t m_Front = 0;
    size_t m_Back = 0;
    bool m_Full = false;
};
