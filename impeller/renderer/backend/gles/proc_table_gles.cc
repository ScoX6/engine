// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/gles/proc_table_gles.h"

#include <sstream>

#include "impeller/base/validation.h"

namespace impeller {

const char* GLErrorToString(GLenum value) {
  switch (value) {
    case GL_NO_ERROR:
      return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
      return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
      return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
      return "GL_INVALID_OPERATION";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "GL_INVALID_FRAMEBUFFER_OPERATION";
    case GL_FRAMEBUFFER_COMPLETE:
      return "GL_FRAMEBUFFER_COMPLETE";
    case GL_OUT_OF_MEMORY:
      return "GL_OUT_OF_MEMORY";
  }
  return "Unknown.";
}

ProcTableGLES::ProcTableGLES(Resolver resolver) {
  if (!resolver) {
    return;
  }

  auto error_fn = reinterpret_cast<PFNGLGETERRORPROC>(resolver("glGetError"));
  if (!error_fn) {
    VALIDATION_LOG << "Could not resolve "
                   << "glGetError";
    return;
  }

#define IMPELLER_PROC(proc_ivar)                                \
  if (auto fn_ptr = resolver(proc_ivar.name)) {                 \
    proc_ivar.function =                                        \
        reinterpret_cast<decltype(proc_ivar.function)>(fn_ptr); \
    proc_ivar.error_fn = error_fn;                              \
  } else {                                                      \
    VALIDATION_LOG << "Could not resolve " << proc_ivar.name;   \
    return;                                                     \
  }

  FOR_EACH_IMPELLER_PROC(IMPELLER_PROC);

#undef IMPELLER_PROC

  is_valid_ = true;

  description_ = std::make_unique<GLDescription>(*this);
}

ProcTableGLES::~ProcTableGLES() = default;

bool ProcTableGLES::IsValid() const {
  return is_valid_;
}

void ProcTableGLES::ShaderSourceMapping(GLuint shader,
                                        const fml::Mapping& mapping) const {
  const GLchar* sources[] = {
      reinterpret_cast<const GLchar*>(mapping.GetMapping())};
  const GLint lengths[] = {static_cast<GLint>(mapping.GetSize())};
  ShaderSource(shader, 1u, sources, lengths);
}

const GLDescription* ProcTableGLES::GetDescription() const {
  return description_.get();
}

static const char* FramebufferStatusToString(GLenum status) {
  switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
      return "GL_FRAMEBUFFER_COMPLETE";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
#if GL_ES_VERSION_2_0
    case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
      return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
#endif
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
    case GL_FRAMEBUFFER_UNSUPPORTED:
      return "GL_FRAMEBUFFER_UNSUPPORTED";
    case GL_INVALID_ENUM:
      return "GL_INVALID_ENUM";
  }

  return "Unknown FBO Error Status";
}

static const char* AttachmentTypeString(GLint type) {
  switch (type) {
    case GL_RENDERBUFFER:
      return "GL_RENDERBUFFER";
    case GL_TEXTURE:
      return "GL_TEXTURE";
    case GL_NONE:
      return "GL_NONE";
  }

  return "Unknown Type";
}

static std::string DescribeFramebufferAttachment(const ProcTableGLES& gl,
                                                 GLenum attachment) {
  GLint param = GL_NONE;
  gl.GetFramebufferAttachmentParameteriv(
      GL_FRAMEBUFFER,                         // target
      attachment,                             // attachment
      GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE,  // parameter name
      &param                                  // parameter
  );

  if (param != GL_NONE) {
    param = GL_NONE;
    gl.GetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER,                         // target
        attachment,                             // attachment
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME,  // parameter name
        &param                                  // parameter
    );
    std::stringstream stream;
    stream << AttachmentTypeString(param) << "(" << param << ")";
    return stream.str();
  }

  return "No Attachment";
}

std::string ProcTableGLES::DescribeCurrentFramebuffer() const {
  GLint framebuffer = GL_NONE;
  GetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  if (IsFramebuffer(framebuffer) == GL_FALSE) {
    return "No framebuffer or the default window framebuffer is bound.";
  }

  GLenum status = CheckFramebufferStatus(framebuffer);
  std::stringstream stream;
  stream << "FBO "
         << ((framebuffer == GL_NONE) ? "(Default)"
                                      : std::to_string(framebuffer))
         << ": " << FramebufferStatusToString(status) << std::endl;
  if (IsCurrentFramebufferComplete()) {
    stream << "Framebuffer is complete." << std::endl;
  } else {
    stream << "Framebuffer is incomplete." << std::endl;
  }
  stream << "Description: " << std::endl;
  stream << "Color Attachment: "
         << DescribeFramebufferAttachment(*this, GL_COLOR_ATTACHMENT0)
         << std::endl;
  stream << "Color Attachment: "
         << DescribeFramebufferAttachment(*this, GL_DEPTH_ATTACHMENT)
         << std::endl;
  stream << "Color Attachment: "
         << DescribeFramebufferAttachment(*this, GL_STENCIL_ATTACHMENT)
         << std::endl;
  return stream.str();
}

bool ProcTableGLES::IsCurrentFramebufferComplete() const {
  GLint framebuffer = GL_NONE;
  GetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  if (IsFramebuffer(framebuffer) == GL_FALSE) {
    // The default framebuffer is always complete.
    return true;
  }
  GLenum status = CheckFramebufferStatus(framebuffer);
  return status == GL_FRAMEBUFFER_COMPLETE;
}

}  // namespace impeller
