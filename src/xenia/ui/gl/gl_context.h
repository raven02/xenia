/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_UI_GL_GL_CONTEXT_H_
#define XENIA_UI_GL_GL_CONTEXT_H_

#include <gflags/gflags.h>

#include <memory>

#include "xenia/ui/gl/blitter.h"
#include "xenia/ui/gl/gl.h"

DECLARE_bool(thread_safe_gl);

namespace xe {
namespace ui {
namespace gl {

class GLContext {
 public:
  GLContext();
  GLContext(HWND hwnd, HGLRC glrc);
  ~GLContext();

  bool Initialize(HWND hwnd);
  void AssertExtensionsPresent();

  HDC dc() const { return dc_; }

  std::unique_ptr<GLContext> CreateShared();

  bool MakeCurrent();
  void ClearCurrent();

  Blitter* blitter() { return &blitter_; }

 private:
  void SetupDebugging();
  void DebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity,
                    GLsizei length, const GLchar* message);
  static void GLAPIENTRY
  DebugMessageThunk(GLenum source, GLenum type, GLuint id, GLenum severity,
                    GLsizei length, const GLchar* message, GLvoid* user_param);

  HWND hwnd_;
  HDC dc_;
  HGLRC glrc_;

  GLEWContext glew_context_;
  WGLEWContext wglew_context_;

  Blitter blitter_;
};

struct GLContextLock {
  GLContextLock(GLContext* context) : context_(context) {
    context_->MakeCurrent();
  }
  ~GLContextLock() { context_->ClearCurrent(); }

 private:
  GLContext* context_;
};

}  // namespace gl
}  // namespace ui
}  // namespace xe

#endif  // XENIA_UI_GL_GL_CONTEXT_H_
