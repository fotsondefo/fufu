#pragma once

#include <memory>
#include <cstdint>

namespace Fufu::RHI
{

template<typename T>
using Ref = std::shared_ptr<T>;

enum class Backend { OpenGL, Vulkan };

// ── Format pixel ─────────────────────────────────────────────────────────────
enum class Format
{
    RGBA8_UNORM,
    RGBA16_FLOAT,
    RGBA32_FLOAT,
    R32_FLOAT,
    DEPTH24_STENCIL8,
    DEPTH32_FLOAT,
};

// ── Usages de buffer (combinables via |) ─────────────────────────────────────
enum class BufferUsage : uint32_t
{
    Vertex      = 1 << 0,
    Index       = 1 << 1,
    Uniform     = 1 << 2,   // UBO — binding point GL_UNIFORM_BUFFER
    Storage     = 1 << 3,   // SSBO — binding point GL_SHADER_STORAGE_BUFFER
    TransferSrc = 1 << 4,
    TransferDst = 1 << 5,
};
inline BufferUsage operator|(BufferUsage a, BufferUsage b)
{
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool hasFlag(BufferUsage value, BufferUsage flag)
{
    return (static_cast<uint32_t>(value) & static_cast<uint32_t>(flag)) != 0;
}

// ── Usages de texture (combinables via |) ────────────────────────────────────
enum class TextureUsage : uint32_t
{
    Sampled         = 1 << 0,
    StorageImage    = 1 << 1,
    ColorAttachment = 1 << 2,
    DepthAttachment = 1 << 3,
};
inline TextureUsage operator|(TextureUsage a, TextureUsage b)
{
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

// ── Type de mémoire ──────────────────────────────────────────────────────────
enum class MemoryType
{
    GPU,        // Device local — VRAM
    CPU_TO_GPU, // Staging / upload — host coherent
    GPU_TO_CPU, // Readback
};

} // namespace Fufu::RHI
