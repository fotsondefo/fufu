#pragma once

#include "RHI/RHIBuffer.h"
#include "RHI/RHITexture.h"
#include "RHI/RHISampler.h"
#include "RHI/RHIShader.h"
#include <glad/glad.h>

namespace Fufu::RHI
{

// ── GLBuffer ─────────────────────────────────────────────────────────────────

class GLBuffer final : public RHIBuffer
{
public:
    explicit GLBuffer(const BufferDesc& desc);
    ~GLBuffer() override;

    void  upload(const void* data, uint64_t size, uint64_t offset = 0) override;
    void* map  (uint64_t offset = 0, uint64_t size = WHOLE_SIZE)       override;
    void  unmap()                                                        override;

    GLuint getHandle() const { return m_Handle; }
    GLenum getTarget() const { return m_Target; }

private:
    GLuint m_Handle = 0;
    GLenum m_Target = GL_SHADER_STORAGE_BUFFER;
};

// ── GLTexture ────────────────────────────────────────────────────────────────

class GLTexture final : public RHITexture
{
public:
    explicit GLTexture(const TextureDesc& desc);
    ~GLTexture() override;

    void upload(const void* data, uint32_t mip = 0, uint32_t layer = 0) override;

    GLuint getHandle()        const { return m_Handle; }
    GLenum getGLTarget()      const { return m_Target; }
    GLenum getGLFormat()      const { return m_Format; }
    GLenum getGLInternalFmt() const { return m_InternalFormat; }
    GLenum getGLDataType()    const { return m_DataType; }

private:
    GLuint m_Handle         = 0;
    GLenum m_Target         = GL_TEXTURE_2D;
    GLenum m_InternalFormat = GL_RGBA8;
    GLenum m_Format         = GL_RGBA;
    GLenum m_DataType       = GL_UNSIGNED_BYTE;
};

// ── GLSampler ────────────────────────────────────────────────────────────────

class GLSampler final : public RHISampler
{
public:
    explicit GLSampler(const SamplerDesc& desc);
    ~GLSampler() override;

    GLuint getHandle() const { return m_Handle; }

private:
    GLuint m_Handle = 0;
};

// ── GLShader ─────────────────────────────────────────────────────────────────

class GLShader final : public RHIShader
{
public:
    explicit GLShader(const ShaderDesc& desc);
    ~GLShader() override;

    GLuint getShaderID() const { return m_ShaderID; }

private:
    GLuint m_ShaderID = 0;
};

} // namespace Fufu::RHI
