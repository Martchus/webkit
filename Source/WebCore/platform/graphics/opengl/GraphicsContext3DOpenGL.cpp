/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 ChangSeok Oh <shivamidow@gmail.com>
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2014 Digia Plc. and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

// Note this implementation serves a double role for Qt where it also handles OpenGLES.

#if USE(3D_GRAPHICS)

#include "GraphicsContext3D.h"

#include "Extensions3DOpenGL.h"
#include "IntRect.h"
#include "IntSize.h"
#include "NotImplemented.h"

#include <algorithm>
#include <cstring>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

#include <QOpenGLContext>
#include <private/qopenglextensions_p.h>

#if PLATFORM(MAC)
#include <OpenGL/gl.h>
#elif PLATFORM(GTK) || PLATFORM(EFL) || PLATFORM(WIN)
#include "OpenGLShims.h"
#endif

#if PLATFORM(QT)

#ifndef GL_BGRA
#define GL_BGRA                         0x80E1
#endif

#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER             0x8CA8
#endif

#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER             0x8CA9
#endif

#ifndef GL_MAX_VARYING_FLOATS
#define GL_MAX_VARYING_FLOATS             0x8B4B
#endif

#ifndef GL_ALPHA16F_ARB
#define GL_ALPHA16F_ARB                   0x881C
#endif

#ifndef GL_LUMINANCE16F_ARB
#define GL_LUMINANCE16F_ARB               0x881E
#endif

#ifndef GL_LUMINANCE_ALPHA16F_ARB
#define GL_LUMINANCE_ALPHA16F_ARB         0x881F
#endif

#ifndef GL_HALF_FLOAT_OES
#define GL_HALF_FLOAT_OES                 0x8D61
#endif

///////////////////////////////////////////////////////////////////////////////
// HALT!
//
// This macro breaks if clauses with single statement, i.e. all code looking
// like
//
//   if (...)
//       ::glThingy(...)
//
//  starts calling glThingy unconditionally. Fix it before merging this changes!
//
///////////////////////////////////////////////////////////////////////////////

ALWAYS_INLINE static void doNothing() {}
#define GLFUN(f) doNothing(); m_functions->f

#define glGetIntegerv                       GLFUN(glGetIntegerv)
#define glBindFramebufferEXT                GLFUN(glBindFramebuffer)
#define glBindRenderbufferEXT               GLFUN(glBindRenderbuffer)
#define glRenderbufferStorageEXT            GLFUN(glRenderbufferStorage)
#define glRenderbufferStorageMultisampleEXT GLFUN(glRenderbufferStorageMultisample)
#define glBindTexture                       GLFUN(glBindTexture)
#define glTexImage2D                        GLFUN(glTexImage2D)
#define glFramebufferTexture2DEXT           GLFUN(glFramebufferTexture2D)
#define glFlush                             GLFUN(glFlush)
#define glDepthRange                        GLFUN(glDepthRangef)
#define glClearDepth                        GLFUN(glClearDepthf)
#define glBlitFramebufferEXT                GLFUN(glBlitFramebuffer)
#define glFramebufferRenderbufferEXT        GLFUN(glFramebufferRenderbuffer)

#define glCheckFramebufferStatusEXT m_functions->glCheckFramebufferStatus

#endif

namespace WebCore {

void GraphicsContext3D::releaseShaderCompiler()
{
    makeContextCurrent();
#if PLATFORM(QT)
    ASSERT(m_private);
    m_functions->glReleaseShaderCompiler();
#else
    notImplemented();
#endif
}

void GraphicsContext3D::readPixelsAndConvertToBGRAIfNecessary(int x, int y, int width, int height, unsigned char* pixels)
{
#if PLATFORM(QT)
    ASSERT(m_private);
    bool readBGRA = !isGLES2Compliant() || platformGraphicsContext3D()->hasExtension("GL_EXT_read_format_bgra");

    if (readBGRA)
        m_functions->glReadPixels(x, y, width, height, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
    else
        m_functions->glReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    int totalBytes = width * height * 4;
    if (!readBGRA) {
        for (int i = 0; i < totalBytes; i += 4)
            std::swap(pixels[i], pixels[i + 2]); // Convert to BGRA.
    }
#else
    ::glReadPixels(x, y, width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pixels);
#endif
}

void GraphicsContext3D::validateAttributes()
{
#if PLATFORM(QT)
    if (isGLES2Compliant())
        validateDepthStencil("GL_OES_packed_depth_stencil");
    else
        validateDepthStencil("GL_EXT_packed_depth_stencil");

    if (m_attrs.antialias && isGLES2Compliant()) {
        if (!m_functions->hasOpenGLExtension(QOpenGLExtensions::FramebufferMultisample) || !m_functions->hasOpenGLExtension(QOpenGLExtensions::FramebufferBlit))
            m_attrs.antialias = false;
    }
#else
    validateDepthStencil("GL_EXT_packed_depth_stencil");
#endif
}

bool GraphicsContext3D::reshapeFBOs(const IntSize& size)
{
    const int width = size.width();
    const int height = size.height();
    GLuint colorFormat, internalDepthStencilFormat = 0;
    GLuint pixelDataType = 0;
    if (m_attrs.alpha) {
        m_internalColorFormat = isGLES2Compliant() ? GL_RGBA : GL_RGBA8;
        colorFormat = GL_RGBA;
        pixelDataType = GL_UNSIGNED_BYTE;
    } else {
        m_internalColorFormat = isGLES2Compliant() ? GL_RGB : GL_RGB8;
        colorFormat = GL_RGB;
        pixelDataType = isGLES2Compliant() ? GL_UNSIGNED_SHORT_5_6_5 : GL_UNSIGNED_BYTE;
    }

    if (m_attrs.stencil || m_attrs.depth) {
        // We don't allow the logic where stencil is required and depth is not.
        // See GraphicsContext3D::validateAttributes.

        Extensions3D* extensions = getExtensions();
        // Use a 24 bit depth buffer where we know we have it.
        if (extensions->supports("GL_EXT_packed_depth_stencil"))
            internalDepthStencilFormat = GL_DEPTH24_STENCIL8_EXT;
        else
            internalDepthStencilFormat = GL_DEPTH_COMPONENT;
    }

    bool mustRestoreFBO = false;

    // Resize multisample FBO.
    if (m_attrs.antialias && !isGLES2Compliant()) {
        GLint maxSampleCount;
        ::glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSampleCount);
        GLint sampleCount = std::min(8, maxSampleCount);
        if (sampleCount > maxSampleCount)
            sampleCount = maxSampleCount;
        if (m_state.boundFBO != m_multisampleFBO) {
            ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
            mustRestoreFBO = true;
        }
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, m_internalColorFormat, width, height);
        ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_RENDERBUFFER_EXT, m_multisampleColorBuffer);
        if (m_attrs.stencil || m_attrs.depth) {
            ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            ::glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, sampleCount, internalDepthStencilFormat, width, height);
            if (m_attrs.stencil)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
            if (m_attrs.depth)
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_multisampleDepthStencilBuffer);
        }
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
            // FIXME: cleanup.
            notImplemented();
        }
    }

    // resize regular FBO
    if (m_state.boundFBO != m_fbo) {
        mustRestoreFBO = true;
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
    }

    ::glBindTexture(GL_TEXTURE_2D, m_texture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, pixelDataType, 0);
    ::glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_texture, 0);

    ::glBindTexture(GL_TEXTURE_2D, m_compositorTexture);
    ::glTexImage2D(GL_TEXTURE_2D, 0, m_internalColorFormat, width, height, 0, colorFormat, pixelDataType, 0);
    ::glBindTexture(GL_TEXTURE_2D, 0);

    if (!m_attrs.antialias && (m_attrs.stencil || m_attrs.depth)) {
#if PLATFORM(QT)
        bool supportPackedDepthStencilBuffer = internalDepthStencilFormat == GL_DEPTH24_STENCIL8_EXT;
        if (supportPackedDepthStencilBuffer || !isGLES2Compliant()) {
#endif
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        ::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, internalDepthStencilFormat, width, height);
        if (m_attrs.stencil)
            ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        if (m_attrs.depth)
            ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthStencilBuffer);
        ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
#if PLATFORM(QT)
        } else {
            if (m_attrs.stencil) {
                ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_stencilBuffer);
                ::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_STENCIL_INDEX8, width, height);
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_STENCIL_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_stencilBuffer);
            }
            if (m_attrs.depth) {
                ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, m_depthBuffer);
                ::glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT16, width, height);
                ::glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, m_depthBuffer);
            }
            ::glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);
        }
#endif
    }

    if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT) {
        // FIXME: cleanup
        notImplemented();
    }

    if (m_attrs.antialias && !isGLES2Compliant()) {
        ::glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_multisampleFBO);
        if (m_state.boundFBO == m_multisampleFBO)
            mustRestoreFBO = false;
    }

    return mustRestoreFBO;
}

void GraphicsContext3D::resolveMultisamplingIfNecessary(const IntRect& rect)
{
#if PLATFORM(QT)
    Q_ASSERT(m_private);
    if (!m_attrs.antialias)
        return;

    if (isGLES2Compliant()) {
        notImplemented();
        return;
    }
#endif

    ::glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_multisampleFBO);
    ::glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);

    IntRect resolveRect = rect;
    if (rect.isEmpty())
        resolveRect = IntRect(0, 0, m_currentWidth, m_currentHeight);

    ::glBlitFramebufferEXT(resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), resolveRect.x(), resolveRect.y(), resolveRect.maxX(), resolveRect.maxY(), GL_COLOR_BUFFER_BIT, GL_LINEAR);
}

void GraphicsContext3D::renderbufferStorage(GC3Denum target, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height)
{
    makeContextCurrent();
#if PLATFORM(QT)
    if (!isGLES2Compliant()) {
#endif
    switch (internalformat) {
    case DEPTH_STENCIL:
        internalformat = GL_DEPTH24_STENCIL8_EXT;
        break;
    case DEPTH_COMPONENT16:
        internalformat = GL_DEPTH_COMPONENT;
        break;
    case RGBA4:
    case RGB5_A1:
        internalformat = GL_RGBA;
        break;
    case RGB565:
        internalformat = GL_RGB;
        break;
    }
#if PLATFORM(QT)
    }
#endif
    ::glRenderbufferStorageEXT(target, internalformat, width, height);
}

void GraphicsContext3D::getIntegerv(GC3Denum pname, GC3Dint* value)
{
    // Need to emulate MAX_FRAGMENT/VERTEX_UNIFORM_VECTORS and MAX_VARYING_VECTORS
    // because desktop GL's corresponding queries return the number of components
    // whereas GLES2 return the number of vectors (each vector has 4 components).
    // Therefore, the value returned by desktop GL needs to be divided by 4.
    makeContextCurrent();
#if PLATFORM(QT)
    if (isGLES2Compliant()) {
        ::glGetIntegerv(pname, value);
        return;
    }
#endif
    switch (pname) {
    case MAX_FRAGMENT_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VERTEX_UNIFORM_VECTORS:
        ::glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS, value);
        *value /= 4;
        break;
    case MAX_VARYING_VECTORS:
        ::glGetIntegerv(GL_MAX_VARYING_FLOATS, value);
        *value /= 4;
        break;
    default:
        ::glGetIntegerv(pname, value);
    }
}

void GraphicsContext3D::getShaderPrecisionFormat(GC3Denum shaderType, GC3Denum precisionType, GC3Dint* range, GC3Dint* precision)
{
#if !PLATFORM(QT)
    UNUSED_PARAM(shaderType);
#endif
    ASSERT(range);
    ASSERT(precision);

    makeContextCurrent();

#if PLATFORM(QT)
    m_functions->glGetShaderPrecisionFormat(shaderType, precisionType, range, precision);
    return;
#endif

    switch (precisionType) {
    case GraphicsContext3D::LOW_INT:
    case GraphicsContext3D::MEDIUM_INT:
    case GraphicsContext3D::HIGH_INT:
        // These values are for a 32-bit twos-complement integer format.
        range[0] = 31;
        range[1] = 30;
        precision[0] = 0;
        break;
    case GraphicsContext3D::LOW_FLOAT:
    case GraphicsContext3D::MEDIUM_FLOAT:
    case GraphicsContext3D::HIGH_FLOAT:
        // These values are for an IEEE single-precision floating-point format.
        range[0] = 127;
        range[1] = 127;
        precision[0] = 23;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

bool GraphicsContext3D::texImage2D(GC3Denum target, GC3Dint level, GC3Denum internalformat, GC3Dsizei width, GC3Dsizei height, GC3Dint border, GC3Denum format, GC3Denum type, const void* pixels)
{
    if (width && height && !pixels) {
        synthesizeGLError(INVALID_VALUE);
        return false;
    }

    GC3Denum openGLInternalFormat = internalformat;
#if PLATFORM(QT)
    if (!isGLES2Compliant()) {
#endif
    if (type == GL_FLOAT) {
        if (format == GL_RGBA)
            openGLInternalFormat = GL_RGBA32F_ARB;
        else if (format == GL_RGB)
            openGLInternalFormat = GL_RGB32F_ARB;
    } else if (type == HALF_FLOAT_OES) {
        if (format == GL_RGBA)
            openGLInternalFormat = GL_RGBA16F_ARB;
        else if (format == GL_RGB)
            openGLInternalFormat = GL_RGB16F_ARB;
        else if (format == GL_LUMINANCE)
            openGLInternalFormat = GL_LUMINANCE16F_ARB;
        else if (format == GL_ALPHA)
            openGLInternalFormat = GL_ALPHA16F_ARB;
        else if (format == GL_LUMINANCE_ALPHA)
            openGLInternalFormat = GL_LUMINANCE_ALPHA16F_ARB;
        type = GL_HALF_FLOAT_ARB;
    }
#if PLATFORM(QT)
    }
#endif
    texImage2DDirect(target, level, openGLInternalFormat, width, height, border, format, type, pixels);
    return true;
}

void GraphicsContext3D::depthRange(GC3Dclampf zNear, GC3Dclampf zFar)
{
    makeContextCurrent();
    ::glDepthRange(zNear, zFar);
}

void GraphicsContext3D::clearDepth(GC3Dclampf depth)
{
    makeContextCurrent();
    ::glClearDepth(depth);
}

Extensions3D* GraphicsContext3D::getExtensions()
{
    if (!m_extensions)
        m_extensions = adoptPtr(new Extensions3DOpenGL(this));
    return m_extensions.get();
}

void GraphicsContext3D::readPixels(GC3Dint x, GC3Dint y, GC3Dsizei width, GC3Dsizei height, GC3Denum format, GC3Denum type, void* data)
{
    ASSERT(m_private);
    // FIXME: remove the two glFlush calls when the driver bug is fixed, i.e.,
    // all previous rendering calls should be done before reading pixels.
    makeContextCurrent();
    ::glFlush();
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO) {
        resolveMultisamplingIfNecessary(IntRect(x, y, width, height));
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_fbo);
        ::glFlush();
    }
    m_functions->glReadPixels(x, y, width, height, format, type, data);
    if (m_attrs.antialias && m_state.boundFBO == m_multisampleFBO)
        ::glBindFramebufferEXT(GraphicsContext3D::FRAMEBUFFER, m_multisampleFBO);
}

}

#endif // USE(3D_GRAPHICS)
