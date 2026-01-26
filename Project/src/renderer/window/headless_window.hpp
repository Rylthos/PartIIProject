#pragma once

#include "base_window.hpp"

class HeadlessWindow : public Window {
  public:
    void init() override;
    void cleanup() override;

    bool shouldClose() override;
};
