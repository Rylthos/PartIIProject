#pragma once

#include "vulkan/vulkan.h"

#include <mutex>

class QueueCounter { };

class Queue {
  public:
    Queue(VkQueue queue, uint32_t queueFamily);

    VkQueue getQueue() { return m_Queue; }
    uint32_t getFamily() { return m_QueueFamily; }

    std::mutex& getLock() { return m_Mutex; }

  private:
    VkQueue m_Queue;
    uint32_t m_QueueFamily;

    std::mutex m_Mutex;
};
