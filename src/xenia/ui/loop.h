/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_LOOP_H_
#define XENIA_UI_LOOP_H_

#include <functional>

namespace xe {
namespace ui {

class Loop {
 public:
  Loop() = default;
  virtual ~Loop() = default;

  virtual void Post(std::function<void()> fn) = 0;
  virtual void PostDelayed(std::function<void()> fn, uint64_t delay_millis) = 0;

  virtual void Quit() = 0;
  virtual void AwaitQuit() = 0;
};

}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_LOOP_H_
