#pragma once

#include "Panels/IEditorPanel.h"

namespace FufuStudio
{

class LogPanel : public IEditorPanel
{
public:
    void onImGuiRender(EditorState& state) override;

private:
    bool m_AutoScroll    = true;
    bool m_ShowTrace     = false; // trace is very verbose — off by default
    bool m_ShowInfo      = true;
    bool m_ShowWarn      = true;
    bool m_ShowError     = true;
    char m_Filter[128]   = {};
};

} // namespace FufuStudio
