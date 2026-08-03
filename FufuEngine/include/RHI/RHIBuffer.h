#pragma once

#include "RHITypes.h"

namespace Fufu::RHI
{

static constexpr uint64_t WHOLE_SIZE = ~0ull;

struct BufferDesc
{
    uint64_t    size;
    BufferUsage usage;
    MemoryType  memory      = MemoryType::GPU;
    const void* initialData = nullptr; // nullptr = uninitialized
};

class RHIBuffer
{
public:
    virtual ~RHIBuffer() = default;

    // Copies `size` bytes from `data` at `offset`.
    // For GPU buffers: handles staging if needed (VK).
    virtual void  upload(const void* data, uint64_t size, uint64_t offset = 0) = 0;

    // CPU write pointer — only for CPU_TO_GPU.
    // size = WHOLE_SIZE → map the entire buffer.
    virtual void* map  (uint64_t offset = 0, uint64_t size = WHOLE_SIZE) = 0;
    virtual void  unmap()                                                  = 0;

    uint64_t    getSize()  const { return m_Desc.size; }
    BufferUsage getUsage() const { return m_Desc.usage; }

    // Native backend handle (e.g. GLuint for GL, VkBuffer for Vulkan).
    virtual uint64_t getNativeHandle() const { return 0; }

protected:
    BufferDesc m_Desc{};
};

} // namespace Fufu::RHI
