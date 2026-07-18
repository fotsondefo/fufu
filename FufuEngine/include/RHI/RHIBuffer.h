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
    const void* initialData = nullptr; // nullptr = non initialisé
};

class RHIBuffer
{
public:
    virtual ~RHIBuffer() = default;

    // Copie `size` octets depuis `data` à `offset`.
    // Pour les buffers GPU : gère le staging si nécessaire (VK).
    virtual void  upload(const void* data, uint64_t size, uint64_t offset = 0) = 0;

    // Pointeur CPU en écriture — uniquement pour CPU_TO_GPU.
    // size = WHOLE_SIZE → map tout le buffer.
    virtual void* map  (uint64_t offset = 0, uint64_t size = WHOLE_SIZE) = 0;
    virtual void  unmap()                                                  = 0;

    uint64_t    getSize()  const { return m_Desc.size; }
    BufferUsage getUsage() const { return m_Desc.usage; }

protected:
    BufferDesc m_Desc{};
};

} // namespace Fufu::RHI
