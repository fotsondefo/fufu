#pragma once

#include "RHI/RHIPipeline.h"
#include <glad/glad.h>

namespace Fufu::RHI
{

class GLPipeline final : public RHIPipeline
{
public:
    explicit GLPipeline(const GraphicsPipelineDesc& desc);
    explicit GLPipeline(const ComputePipelineDesc&  desc);
    ~GLPipeline() override;

    // Applique tout l'état GL fixé dans le descripteur.
    void applyState() const;

    GLuint getProgramID() const { return m_ProgramID; }

private:
    GLuint m_ProgramID = 0;

    // État sauvegardé pour applyState()
    RasterizerState   m_Rasterizer{};
    DepthStencilState m_DepthStencil{};
    BlendAttachment   m_Blend{};

    static GLuint linkProgram(GLuint vert, GLuint frag);
    static GLenum toGLCompareOp(CompareOp op);
    static GLenum toGLBlendFactor(BlendFactor f);
    static GLenum toGLBlendOp(BlendOp op);
};

} // namespace Fufu::RHI
