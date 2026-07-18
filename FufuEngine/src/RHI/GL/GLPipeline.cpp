#include "depch.h"
#include "RHI/GL/GLPipeline.h"
#include "RHI/GL/GLResources.h"
#include <stdexcept>
#include <string>

namespace Fufu::RHI
{

// ── Helpers ──────────────────────────────────────────────────────────────────

GLenum GLPipeline::toGLCompareOp(CompareOp op)
{
    switch (op)
    {
    case CompareOp::Never:        return GL_NEVER;
    case CompareOp::Less:         return GL_LESS;
    case CompareOp::Equal:        return GL_EQUAL;
    case CompareOp::LessEqual:    return GL_LEQUAL;
    case CompareOp::Greater:      return GL_GREATER;
    case CompareOp::NotEqual:     return GL_NOTEQUAL;
    case CompareOp::GreaterEqual: return GL_GEQUAL;
    case CompareOp::Always:       return GL_ALWAYS;
    }
    return GL_LESS;
}

GLenum GLPipeline::toGLBlendFactor(BlendFactor f)
{
    switch (f)
    {
    case BlendFactor::Zero:              return GL_ZERO;
    case BlendFactor::One:               return GL_ONE;
    case BlendFactor::SrcAlpha:          return GL_SRC_ALPHA;
    case BlendFactor::OneMinusSrcAlpha:  return GL_ONE_MINUS_SRC_ALPHA;
    case BlendFactor::DstAlpha:          return GL_DST_ALPHA;
    case BlendFactor::OneMinusDstAlpha:  return GL_ONE_MINUS_DST_ALPHA;
    }
    return GL_ONE;
}

GLenum GLPipeline::toGLBlendOp(BlendOp op)
{
    switch (op)
    {
    case BlendOp::Add:      return GL_FUNC_ADD;
    case BlendOp::Subtract: return GL_FUNC_SUBTRACT;
    case BlendOp::Min:      return GL_MIN;
    case BlendOp::Max:      return GL_MAX;
    }
    return GL_FUNC_ADD;
}

GLuint GLPipeline::linkProgram(GLuint vert, GLuint frag)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        glDeleteProgram(prog);
        throw std::runtime_error(std::string("GLPipeline link error:\n") + log);
    }
    glDetachShader(prog, vert);
    glDetachShader(prog, frag);
    return prog;
}

// ── Graphics ─────────────────────────────────────────────────────────────────

GLPipeline::GLPipeline(const GraphicsPipelineDesc& desc)
{
    m_IsCompute   = false;
    m_Rasterizer  = desc.rasterizer;
    m_DepthStencil = desc.depthStencil;
    m_Blend       = desc.blendAttachment;

    auto* vert = static_cast<GLShader*>(desc.vertexShader.get());
    auto* frag = static_cast<GLShader*>(desc.fragmentShader.get());

    if (!vert || !frag)
        throw std::runtime_error("GLPipeline: vertex ou fragment shader manquant");

    m_ProgramID = linkProgram(vert->getShaderID(), frag->getShaderID());
}

// ── Compute ──────────────────────────────────────────────────────────────────

GLPipeline::GLPipeline(const ComputePipelineDesc& desc)
{
    m_IsCompute = true;

    auto* comp = static_cast<GLShader*>(desc.computeShader.get());
    if (!comp)
        throw std::runtime_error("GLPipeline: compute shader manquant");

    m_ProgramID = glCreateProgram();
    glAttachShader(m_ProgramID, comp->getShaderID());
    glLinkProgram(m_ProgramID);

    GLint ok = 0;
    glGetProgramiv(m_ProgramID, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetProgramInfoLog(m_ProgramID, sizeof(log), nullptr, log);
        glDeleteProgram(m_ProgramID);
        throw std::runtime_error(std::string("GLPipeline (compute) link error:\n") + log);
    }
    glDetachShader(m_ProgramID, comp->getShaderID());
}

GLPipeline::~GLPipeline()
{
    if (m_ProgramID) glDeleteProgram(m_ProgramID);
}

// ── applyState ────────────────────────────────────────────────────────────────

void GLPipeline::applyState() const
{
    glUseProgram(m_ProgramID);

    // Rasterizer
    switch (m_Rasterizer.fillMode)
    {
    case FillMode::Wireframe: glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);  break;
    default:                  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  break;
    }
    if (m_Rasterizer.cullMode == CullMode::None)
    {
        glDisable(GL_CULL_FACE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(m_Rasterizer.cullMode == CullMode::Back ? GL_BACK : GL_FRONT);
    }
    glFrontFace(m_Rasterizer.frontFace == FrontFace::CCW ? GL_CCW : GL_CW);

    if (m_Rasterizer.depthBiasConstant != 0.f || m_Rasterizer.depthBiasSlope != 0.f)
    {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(m_Rasterizer.depthBiasSlope, m_Rasterizer.depthBiasConstant);
    }
    else
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    // Depth-stencil
    if (m_DepthStencil.depthTestEnable)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(toGLCompareOp(m_DepthStencil.depthCompareOp));
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(m_DepthStencil.depthWriteEnable ? GL_TRUE : GL_FALSE);

    // Blend
    if (m_Blend.blendEnable)
    {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(
            toGLBlendFactor(m_Blend.srcColor),
            toGLBlendFactor(m_Blend.dstColor),
            toGLBlendFactor(m_Blend.srcAlpha),
            toGLBlendFactor(m_Blend.dstAlpha));
        glBlendEquationSeparate(
            toGLBlendOp(m_Blend.colorOp),
            toGLBlendOp(m_Blend.alphaOp));
    }
    else
    {
        glDisable(GL_BLEND);
    }
}

} // namespace Fufu::RHI
