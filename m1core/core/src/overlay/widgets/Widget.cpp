#include <stdafx.h>

#include "Widget.h"

#include <M1CoreRuntime.h>
#include <overlay/M1CoreUIColors.h>

Widget::Widget(const std::string& acpName, bool aOwnerDraw)
    : m_name(acpName)
    , m_ownerDraw(aOwnerDraw)
{
}

WidgetResult Widget::OnEnable()
{
    m_enabled = true;
    return WidgetResult::ENABLED;
}

WidgetResult Widget::OnDisable()
{
    m_enabled = false;
    return WidgetResult::DISABLED;
}

bool Widget::IsEnabled() const
{
    return m_enabled;
}

void Widget::Toggle()
{
    m_toggle = true;
}

WidgetResult Widget::OnPopup()
{
    return m_enabled ? WidgetResult::DISABLED : WidgetResult::ENABLED;
}

void Widget::OnToggle()
{
    if (m_enabled)
    {
        const auto ret = OnDisable();
        if (ret == WidgetResult::CANCEL)
        {
            m_toggle = false;
        }
        if (ret == WidgetResult::DISABLED)
        {
            m_enabled = false;
            m_toggle = false;
        }
    }
    else
    {
        const auto ret = OnEnable();
        if (ret == WidgetResult::CANCEL)
        {
            m_toggle = false;
        }
        if (ret == WidgetResult::ENABLED)
        {
            m_enabled = true;
            m_toggle = false;
        }
    }
}

void Widget::Draw()
{
    if (m_drawPopup)
    {
        M1CoreRuntime::Get().GetVM().BlockDraw(true);
        m_drawPopup = OnPopup() == WidgetResult::ENABLED;
        if (!m_drawPopup)
        {
            M1CoreRuntime::Get().GetVM().BlockDraw(false);
            ImGui::CloseCurrentPopup();
        }
    }

    if (m_toggle)
        OnToggle();

    if (!m_enabled)
        return;

    bool newEnabled = m_enabled;

    if (m_ownerDraw)
        OnUpdate();
    else
    {
        const auto [width, height] = M1CoreRuntime::Get().GetD3D12().GetResolution();
        ImGui::SetNextWindowPos(ImVec2(width * 0.2f, height * 0.2f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(width * 0.6f, height * 0.6f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(420, 315), ImVec2(FLT_MAX, FLT_MAX));
        // Apply dark title text only while ImGui builds the title bar.
        // Do not recolor buttons, labels or the regular window content.
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
            ImVec2(ImGui::GetStyle().FramePadding.x, 4.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, m1core::ui::TitleText());
        const bool visible = ImGui::Begin(m_name.c_str(), &newEnabled);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        if (visible)
            OnUpdate();
        ImGui::End();
    }

    if (!newEnabled)
    {
        Toggle();
        OnToggle();
    }
}
