#pragma once

#include <ToolboxWidget.h>

namespace GW {
    struct GamePos;
    struct Vec2f;
}

class WorldMapWidget : public ToolboxWidget {
    WorldMapWidget() = default;
    ~WorldMapWidget() override = default;

public:
    static WorldMapWidget& Instance()
    {
        static WorldMapWidget w;
        return w;
    }

    void Initialize() override;

    void SignalTerminate() override;

    void RegisterSettingsContent() override { };

    [[nodiscard]] bool ShowOnWorldMap() const override { return true; }
    [[nodiscard]] const char* Name() const override { return "World Map"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_GLOBE; }

    void LoadSettings(ToolboxIni*) override;
    void SaveSettings(ToolboxIni*) override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingsInternal() override;
    bool WndProc(UINT, WPARAM, LPARAM) override;
    bool CanTerminate() override;

    static void ShowAllOutposts(bool show);
    static bool GamePosToWorldMap(const GW::GamePos& game_map_pos, GW::Vec2f* world_map_pos);
};
