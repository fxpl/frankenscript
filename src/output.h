#pragma once

#include <iostream>
#include <string>

namespace objects {
  [[noreturn]] inline void error(const std::string &msg) {
    std::cerr << "Error: " << msg << std::endl;
    abort();
  }
}