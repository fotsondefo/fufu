#include "depch.h"
#include "RHI/GL/GLResources.h"
#include <stdexcept>

namespace Fufu::RHI
{

// ── Helpers ──────────────────────────────────────────────────────────────────

static GLenum toGLBufferTarget(BufferUsage usage)
{
    if (hasFlag(usage, BufferUsage::Storage)) return GL_SHADER_STORAGE_BUFFER;
    if (hasFlag(usage, BufferUsage::Uniform)) return GL_UNIFORM_BUFFER;
    if (hasFlag(usage, BufferUsage::Index))   return GL_ELEMENT_ARRAY_BUFFER;
    return GL_ARRAY_BUFFER;
}

static GLenum toGLMemoryHint(MemoryType mem)
{
    switch (mem)
    {
    case MemoryType::CPU_TO_GPU: return GL_DYNAMIC_DRAW;
    case MemoryType::GPU_TO_CPU: return GL_STREAM_READ;
    default:                     return GL_STATIC_DRAW;
    }
}

static void formatToGL(Format fmt,
                        GLenum& internalFmt,
                        GLenum& glFmt,
                        GLenum& dataType)
{
    switch (fmt)
    {
    case Format::RGBA8_UNORM:
        internalFmt = GL_RGBA8;   glFmt = GL_RGBA; dataType = GL_UNSIGNED_BYTE; break;
    case Format::RGBA16_FLOAT:
        internalFmt = GL_RGBA16F; glFmt = GL_RGBA; dataType = GL_HALF_FLOAT;    break;
    case Format::RGBA32_FLOAT:
        internalFmt = GL_RGBA32F; glFmt = GL_RGBA; dataType = GL_FLOAT;         break;
    case Format::R32_FLOAT:
        internalFmt = GL_R32F;    glFmt = GL_RED;  dataType = GL_FLOAT;         break;
    case Format::DEPTH24_STENCIL8:
        internalFmt = GL_DEPTH24_STENCIL8;
        glFmt       = GL_DEPTH_STENCIL;
        dataType    = GL_UNSIGNED_INT_24_8; break;
    case Format::DEPTH32_FLOAT:
        internalFmt = GL_DEPTH_COMPONENT32F;
        glFmt       = GL_DEPTH_COMPONENT;
        dataType    = GL_FLOAT; break;
    default:
        internalFmt = GL_RGBA8; glFmt = GL_RGBA; dataType = GL_UNSIGNED_BYTE; break;
    }
}

// ── GLBuffer ─────────────────────────────────────────────────────────────────

GLBuffer::GLBuffer(const BufferDesc& desc)
{
    m_Desc   = desc;
    m_Target = toGLBufferTarget(desc.usage);

    glGenBuffers(1, &m_Handle);
    glBindBuffer(m_Target, m_Handle);
    glBufferData(m_Target,
                 static_cast<GLsizeiptr>(desc.size),
                 desc.initialData,
                 toGLMemoryHint(desc.memory));
    glBindBuffer(m_Target, 0);
}

GLBuffer::~GLBuffer()
{
    if (m_Handle) glDeleteBuffers(1, &m_Handle);
}

void GLBuffer::upload(const void* data, uint64_t size, uint64_t offset)
{
    glBindBuffer(m_Target, m_Handle);
    glBufferSubData(m_Target,
                    static_cast<GLintptr>(offset),
                    static_cast<GLsizeiptr>(size),
                    data);
    glBindBuffer(m_Target, 0);
}

void* GLBuffer::map(uint64_t offset, uint64_t size)
{
    GLsizeiptr mapSize = (size == WHOLE_SIZE)
        ? static_cast<GLsizeiptr>(m_Desc.size - offset)
        : static_cast<GLsizeiptr>(size);

    glBindBuffer(m_Target, m_Handle);
    return glMapBufferRange(m_Target,
                             static_cast<GLintptr>(offset),
                             mapSize,
                             GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT);
}

void GLBuffer::unmap()
{
    glUnmapBuffer(m_Target);
    glBindBuffer(m_Target, 0);
}

// ── GLTexture ────────────────────────────────────────────────────────────────

GLTexture::GLTexture(const TextureDesc& desc)
{
    m_Desc = desc;
    formatToGL(desc.format, m_InternalFormat, m_Format, m_DataType);
    m_Target = (desc.type == TextureType::TextureCube) ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;

    glGenTextures(1, &m_Handle);
    glBindTexture(m_Target, m_Handle);

    glTexStorage2D(m_Target,
                   static_cast<GLsizei>(desc.mipLevels),
                   m_InternalFormat,
                   static_cast<GLsizei>(desc.width),
                   static_cast<GLsizei>(desc.height));

    glBindTexture(m_Target, 0);
}

GLTexture::~GLTexture()
{
    if (m_Handle) glDeleteTextures(1, &m_Handle);
}

void GLTexture::upload(const void* data, uint32_t mip, uint32_t layer)
{
    if (!data) return;
    uint32_t w = std::max(1u, m_Desc.width  >> mip);
    uint32_t h = std::max(1u, m_Desc.height >> mip);

    glBindTexture(m_Target, m_Handle);
    if (m_Target == GL_TEXTURE_CUBE_MAP)
    {
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer,
                         static_cast<GLint>(mip),
                         0, 0,
                         static_cast<GLsizei>(w),
                         static_cast<GLsizei>(h),
                         m_Format, m_DataType, data);
    }
    else
    {
        glTexSubImage2D(m_Target,
                         static_cast<GLint>(mip),
                         0, 0,
                         static_cast<GLsizei>(w),
                         static_cast<GLsizei>(h),
                         m_Format, m_DataType, data);
    }
    glBindTexture(m_Target, 0);
}

// ── GLSampler ────────────────────────────────────────────────────────────────

static GLenum toGLFilter   (Filter f) { return f == Filter::Nearest ? GL_NEAREST : GL_LINEAR; }
static GLenum toGLMipFilter(Filter f) { return f == Filter::Nearest ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR; }
static GLenum toGLWrap(AddressMode m)
{
    switch (m)
    {
    case AddressMode::ClampToEdge:  return GL_CLAMP_TO_EDGE;
    case AddressMode::MirrorRepeat: return GL_MIRRORED_REPEAT;
    default:                        return GL_REPEAT;
    }
}

GLSampler::GLSampler(const SamplerDesc& desc)
{
    m_Desc = desc;
    glGenSamplers(1, &m_Handle);
    glSamplerParameteri(m_Handle, GL_TEXTURE_MIN_FILTER, toGLMipFilter(desc.minFilter));
    glSamplerParameteri(m_Handle, GL_TEXTURE_MAG_FILTER, toGLFilter(desc.magFilter));
    glSamplerParameteri(m_Handle, GL_TEXTURE_WRAP_S,     toGLWrap(desc.addressU));
    glSamplerParameteri(m_Handle, GL_TEXTURE_WRAP_T,     toGLWrap(desc.addressV));
    glSamplerParameteri(m_Handle, GL_TEXTURE_WRAP_R,     toGLWrap(desc.addressW));
    // GL_TEXTURE_MAX_ANISOTROPY_EXT vaut 0x84FE (ARB_texture_filter_anisotropic)
    if (desc.maxAnisotropy > 1.f)
        glSamplerParameterf(m_Handle, 0x84FE, desc.maxAnisotropy);
}

GLSampler::~GLSampler()
{
    if (m_Handle) glDeleteSamplers(1, &m_Handle);
}

// ── GLShader ─────────────────────────────────────────────────────────────────

static GLenum toGLShaderType(ShaderStage s)
{
    switch (s)
    {
    case ShaderStage::Vertex:   return GL_VERTEX_SHADER;
    case ShaderStage::Fragment: return GL_FRAGMENT_SHADER;
    case ShaderStage::Compute:  return GL_COMPUTE_SHADER;
    }
    return GL_VERTEX_SHADER;
}

GLShader::GLShader(const ShaderDesc& desc)
{
    m_Desc = desc;

    if (desc.glslSource.empty())
        throw std::runtime_error("GLShader: glslSource vide");

    m_ShaderID = glCreateShader(toGLShaderType(desc.stage));
    const char* src = desc.glslSource.c_str();
    glShaderSource(m_ShaderID, 1, &src, nullptr);
    glCompileShader(m_ShaderID);

    GLint ok = 0;
    glGetShaderiv(m_ShaderID, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetShaderInfoLog(m_ShaderID, sizeof(log), nullptr, log);
        glDeleteShader(m_ShaderID);
        m_ShaderID = 0;
        throw std::runtime_error(std::string("GLShader compile error:\n") + log);
    }
}

GLShader::~GLShader()
{
    if (m_ShaderID) glDeleteShader(m_ShaderID);
}

} // namespace Fufu::RHI
