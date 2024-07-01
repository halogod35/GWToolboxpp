#pragma once
#include <GWCA/GameEntities/Agent.h>

#include <GWCA/Constants/Skills.h>

#include <GWCA/Managers/UIMgr.h>

#include <Color.h>
#include <Timer.h>
#include <Widgets/SnapsToPartyWindow.h>

class SkillMonitorWidget : public SnapsToPartyWindow {
protected:
    static void OnStoCPacket(GW::HookStatus* status, GW::Packet::StoC::PacketBase* base);
    static void SkillCallback(const uint32_t value_id, const uint32_t caster_id, const uint32_t value);
public:
    static SkillMonitorWidget& Instance()
    {
        static SkillMonitorWidget instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Skill Monitor"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_HISTORY; }

    void Initialize() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* device) override;
    void Update(float delta) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;
};
