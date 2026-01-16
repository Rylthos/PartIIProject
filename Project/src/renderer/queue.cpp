#include "queue.hpp"

Queue::Queue(VkQueue queue, uint32_t queueFamily) : m_Queue(queue), m_QueueFamily(queueFamily) { }
