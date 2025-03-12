//===- ProgressBar.h-------------------------------------------------------===//
// Part of the eld Project, under the BSD License
// See https://github.com/qualcomm/eld/LICENSE.txt for license information.
// SPDX-License-Identifier: BSD-3-Clause
//===----------------------------------------------------------------------===//

#ifndef ELD_SUPPORT_PROGRESSBAR_H
#define ELD_SUPPORT_PROGRESSBAR_H

#include "llvm/Support/DataTypes.h"
#include <chrono>
#include <iostream>

namespace eld {
class ProgressBar {
private:
  uint32_t ticks = 0;
  uint32_t total_ticks;
  uint32_t bar_width;
  const char complete_char = '=';
  const char incomplete_char = ' ';
  const std::chrono::steady_clock::time_point start_time =
      std::chrono::steady_clock::now();

public:
  ProgressBar(unsigned int total, unsigned int width, bool enabled)
      : total_ticks{total}, bar_width{width}, m_Enabled(enabled) {}

  unsigned int operator++() { return ++ticks; }

  void display(bool isSpin) {
    if (!m_Enabled)
      return;
    const char spin_chars[] = "/-\\|";
    float progress = (float)ticks / total_ticks;
    uint32_t pos = (int)(bar_width * progress);

    std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();
    auto time_elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time)
            .count();

    std::cout << "Progress : ";
    std::cout << "[";

    for (uint32_t i = 0; i < bar_width; ++i) {
      if (i < pos)
        std::cout << complete_char;
      else if (i == pos) {
        if (!isSpin) {
          std::cout << ">";
        } else {
          std::cout << spin_chars[spin_count++ % sizeof(spin_chars)];
        }
      } else
        std::cout << incomplete_char;
    }
    std::cout << "] " << int(progress * 100.0) << "% "
              << float(time_elapsed) / 1000.0 << "s\r";
    std::cout.flush();
  }

  void displaySpin() { display(true); }

  void incrementAndDisplayProgress() {
    if (!m_Enabled)
      return;
    spin_count = 0;
    this->operator++();
    display(false);
  }

  void updateTicks(uint32_t Total) { total_ticks = Total; }

  void addMoreTicks(uint32_t Ticks) { total_ticks += Ticks; }

  void done() const { std::cout << std::endl; }

  ~ProgressBar() {
    if (m_Enabled)
      done();
  }

private:
  bool m_Enabled = false;
  uint32_t spin_count = 0;
};

} // namespace eld
#endif // ELD_SUPPORT_PROGRESSBAR_H
