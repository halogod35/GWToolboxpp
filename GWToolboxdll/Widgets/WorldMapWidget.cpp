#include "stdafx.h"

#include <GWCA/Constants/Maps.h>

#include <GWCA/Utilities/MemoryPatcher.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Pathing.h>

#include <GWCA/Context/MapContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/QuestMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <GWCA/Packets/StoC.h>

#include <Widgets/WorldMapWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Modules/GwDatTextureModule.h>
#include <Modules/Resources.h>

#include <Utils/GuiUtils.h>
#include <Utils/ToolboxUtils.h>

#include "Defines.h"

#include <ImGuiAddons.h>
#include <Constants/EncStrings.h>
#include <Modules/QuestModule.h>
#include <GWCA/Managers/AgentMgr.h>


namespace {
    ImRect show_all_rect;
    ImRect hard_mode_rect;
    ImRect place_marker_rect;
    ImRect remove_marker_rect;
    ImRect show_lines_on_world_map_rect;

    bool showing_all_outposts = false;

    bool drawn = false;

    GW::MemoryPatcher view_all_outposts_patch;
    GW::MemoryPatcher view_all_carto_areas_patch;

    uint32_t __cdecl GetCartographyFlagsForArea(uint32_t, uint32_t, uint32_t, uint32_t)
    {
        return 0xffffffff;
    }

    bool world_map_clicking = false;
    GW::Vec2f world_map_click_pos;

    bool show_lines_on_world_map = true;

    bool MapContainsWorldPos(GW::Constants::MapID map_id, const GW::Vec2f& world_map_pos, GW::Constants::Campaign campaign)
    {
        const auto map = GW::Map::GetMapInfo(map_id);
        if (!(map && map->campaign == campaign))
            return false;
        ImRect map_bounds;
        return GW::Map::GetMapWorldMapBounds(map, &map_bounds) && map_bounds.Contains(world_map_pos);
    }

    bool WorldMapContextMenu(void*)
    {
        if (!GW::Map::GetWorldMapContext())
            return false;
        const auto c = ImGui::GetCurrentContext();
        auto viewport_offset = c->CurrentViewport->Pos;
        viewport_offset.x *= -1;
        viewport_offset.y *= -1;

        ImGui::Text("%.2f, %.2f", world_map_click_pos.x, world_map_click_pos.y);
#ifdef _DEBUG
        GW::GamePos game_pos;
        if (WorldMapWidget::WorldMapToGamePos(world_map_click_pos, game_pos)) {
            ImGui::Text("%.2f, %.2f", game_pos.x, game_pos.y);
        }
#endif
        const auto map_id = WorldMapWidget::GetMapIdForLocation(world_map_click_pos);
        ImGui::TextUnformatted(Resources::GetMapName(map_id)->string().c_str());

        if (ImGui::Button("Place Marker")) {
            GW::GameThread::Enqueue([] {
                QuestModule::SetCustomQuestMarker(world_map_click_pos, true);
            });
            return false;
        }
        place_marker_rect = c->LastItemData.Rect;
        place_marker_rect.Translate(viewport_offset);
        memset(&remove_marker_rect, 0, sizeof(remove_marker_rect));
        if (QuestModule::GetCustomQuestMarker()) {
            if (ImGui::Button("Remove Marker")) {
                GW::GameThread::Enqueue([] {
                    QuestModule::SetCustomQuestMarker({0, 0});
                });
                return false;
            }
            remove_marker_rect = c->LastItemData.Rect;
            remove_marker_rect.Translate(viewport_offset);
        }
        return true;
    }

    const uint32_t GetMapPropModelFileId(GW::MapProp* prop)
    {
        if (!(prop && prop->h0034[4]))
            return 0;
        uint32_t* sub_deets = (uint32_t*)prop->h0034[4];
        return GwDatTextureModule::FileHashToFileId((wchar_t*)sub_deets[1]);
    };

    bool IsTravelPortal(GW::MapProp* prop)
    {
        switch (GetMapPropModelFileId(prop)) {
        case 0xa825: // Prophecies, Factions
            return true;
        }
        return false;
    }

    bool IsValidOutpost(GW::Constants::MapID map_id)
    {
        const auto map_info = GW::Map::GetMapInfo(map_id);
        if (!map_info || !map_info->thumbnail_id || !map_info->name_id || !(map_info->x || map_info->y))
            return false;
        if ((map_info->flags & 0x5000000) == 0x5000000)
            return false; // e.g. "wrong" augury rock is map 119, no NPCs
        if ((map_info->flags & 0x80000000) == 0x80000000)
            return false; // e.g. Debug map
        switch (map_info->type) {
        case GW::RegionType::City:
        case GW::RegionType::CompetitiveMission:
        case GW::RegionType::CooperativeMission:
        case GW::RegionType::EliteMission:
        case GW::RegionType::MissionOutpost:
        case GW::RegionType::Outpost:
            break;
        default:
            return false;
        }
        return true;
    }

    struct MapPortal {
        GW::Constants::MapID from;
        GW::Constants::MapID to;
        GW::Vec2f world_pos;
    };
    std::vector<MapPortal> map_portals;

    GW::Constants::MapID GetClosestMapToPoint(GW::Vec2f world_map_point) {
        for (size_t i = 0; i < (size_t)GW::Constants::MapID::Count; i++) {
            const auto map_info = GW::Map::GetMapInfo((GW::Constants::MapID)i);
            if (!map_info || !map_info->thumbnail_id || !map_info->name_id || !(map_info->x || map_info->y))
                continue;
            if ((map_info->flags & 0x5000000) == 0x5000000)
                continue; // e.g. "wrong" augury rock is map 119, no NPCs
            if ((map_info->flags & 0x80000000) == 0x80000000)
                continue; // e.g. Debug map
            if(!map_info->GetIsOnWorldMap())
                continue;
            (world_map_point);
            // TODO: distance from point to rect
        }
        return GW::Constants::MapID::None;
    }

    bool AppendMapPortals() {
        const auto props = GW::Map::GetMapProps();
        const auto map_id = GW::Map::GetMapID();
        if (!props) return false;
        for (auto prop : *props) {
            if (IsTravelPortal(prop)) {
                GW::Vec2f world_pos;
                if (!WorldMapWidget::GamePosToWorldMap({ prop->position.x, prop->position.y }, world_pos))
                    continue;
                map_portals.push_back({
                    map_id,GetClosestMapToPoint(world_pos),world_pos
                    });
            }

        }
        return true;
    }


    GW::HookEntry OnUIMessage_HookEntry;

    void OnUIMessage(GW::HookStatus* status, GW::UI::UIMessage message_id, void*, void*)
    {
        if (status->blocked)
            return;

        switch (message_id) {
            case GW::UI::UIMessage::kMapLoaded:
                map_portals.clear();
                AppendMapPortals();
                break;
            break;
        }
    }

    void TriggerWorldMapRedraw()
    {
        GW::GameThread::Enqueue([] {
            // Trigger a benign ui message e.g. guild context update; world map subscribes to this, and automatically updates the view.
            // GW::UI::SendUIMessage((GW::UI::UIMessage)0x100000ca); // disables guild/ally chat until reloading char/map
            const auto world_map_context = GW::Map::GetWorldMapContext();
            const auto frame = GW::UI::GetFrameById(world_map_context->frame_id);
            GW::UI::SendFrameUIMessage(frame, GW::UI::UIMessage::kMapLoaded, nullptr);
            //GW::UI::SendFrameUIMessage(frame,(GW::UI::UIMessage)0x1000008e, nullptr);
        });
    }

}
GW::Constants::MapID WorldMapWidget::GetMapIdForLocation(const GW::Vec2f& world_map_pos) {
    auto map_id = GW::Map::GetMapID();
    auto map_info = GW::Map::GetMapInfo();
    if (!map_info)
        return GW::Constants::MapID::None;
    const auto campaign = map_info->campaign;
    if (MapContainsWorldPos(map_id, world_map_pos, campaign))
        return map_id;
    for (size_t i = 1; i < static_cast<size_t>(GW::Constants::MapID::Count); i++) {
        map_id = static_cast<GW::Constants::MapID>(i);
        map_info = GW::Map::GetMapInfo(map_id);
        if (!(map_info && map_info->GetIsOnWorldMap()))
            continue;
        if (MapContainsWorldPos(map_id, world_map_pos, campaign))
            return map_id;
    }
    return GW::Constants::MapID::None;
}
void WorldMapWidget::Initialize()
{
    ToolboxWidget::Initialize();



    uintptr_t address = GW::Scanner::Find("\x8b\x45\xfc\xf7\x40\x10\x00\x00\x01\x00", "xxxxxxxxxx", 0xa);
    if (address) {
        view_all_outposts_patch.SetPatch(address, "\xeb", 1);
    }
    address = GW::Scanner::Find("\x8b\xd8\x83\xc4\x10\x8b\xcb\x8b\xf3\xd1\xe9", "xxxxxxxxxxx", -0x5);
    if (address) {
        view_all_carto_areas_patch.SetRedirect(address, GetCartographyFlagsForArea);
    }


    ASSERT(view_all_outposts_patch.IsValid());
    ASSERT(view_all_carto_areas_patch.IsValid());

    const GW::UI::UIMessage ui_messages[] = {
        GW::UI::UIMessage::kQuestAdded,
        GW::UI::UIMessage::kSendSetActiveQuest,
        GW::UI::UIMessage::kMapLoaded,
        GW::UI::UIMessage::kOnScreenMessage,
        GW::UI::UIMessage::kSendAbandonQuest
    };
    for (auto ui_message : ui_messages) {
        GW::UI::RegisterUIMessageCallback(&OnUIMessage_HookEntry, ui_message, OnUIMessage);
    }
}
bool WorldMapWidget::WorldMapToGamePos(const GW::Vec2f& world_map_pos, GW::GamePos& game_map_pos) {
    ImRect map_bounds;
    if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
        return false;

    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context)
        return false;

    const auto game_map_rect = ImRect({
        current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
        current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
        });

    const auto gwinches_per_unit = 96.f;

    // Calculate the mid-point of the map in world coordinates
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    // Convert from world map position to game map position
    game_map_pos.x = (world_map_pos.x - map_mid_world_point.x) * gwinches_per_unit;
    game_map_pos.y = (world_map_pos.y - map_mid_world_point.y) * gwinches_per_unit * -1.f; // Invert Y axis

    return true;
}
bool WorldMapWidget::GamePosToWorldMap(const GW::GamePos& game_map_pos, GW::Vec2f& world_map_pos)
{
    ImRect map_bounds;
    if (!GW::Map::GetMapWorldMapBounds(GW::Map::GetMapInfo(), &map_bounds))
        return false;
    const auto current_map_context = GW::GetMapContext();
    if (!current_map_context)
        return false;

    const auto game_map_rect = ImRect({
        current_map_context->map_boundaries[1], current_map_context->map_boundaries[2],
        current_map_context->map_boundaries[3], current_map_context->map_boundaries[4],
        });

    // NB: World map is 96 gwinches per unit, this is hard coded in the GW source
    const auto gwinches_per_unit = 96.f;
    GW::Vec2f map_mid_world_point = {
        map_bounds.Min.x + (abs(game_map_rect.Min.x) / gwinches_per_unit),
        map_bounds.Min.y + (abs(game_map_rect.Max.y) / gwinches_per_unit),
    };

    world_map_pos.x = (game_map_pos.x / gwinches_per_unit) + map_mid_world_point.x;
    world_map_pos.y = ((game_map_pos.y * -1.f) / gwinches_per_unit) + map_mid_world_point.y; // Inverted Y Axis
    return true;
}

void WorldMapWidget::SignalTerminate()
{
    ToolboxWidget::Terminate();

    view_all_outposts_patch.Reset();
    view_all_carto_areas_patch.Reset();
    GW::UI::RemoveUIMessageCallback(&OnUIMessage_HookEntry);
}

void WorldMapWidget::ShowAllOutposts(const bool show = showing_all_outposts)
{
    if (view_all_outposts_patch.IsValid())
        view_all_outposts_patch.TogglePatch(show);
    if (view_all_carto_areas_patch.IsValid())
        view_all_carto_areas_patch.TogglePatch(show);
    TriggerWorldMapRedraw();
}

void WorldMapWidget::LoadSettings(ToolboxIni* ini)
{
    ToolboxWidget::LoadSettings(ini);
    LOAD_BOOL(showing_all_outposts);
    LOAD_BOOL(show_lines_on_world_map);
}

void WorldMapWidget::SaveSettings(ToolboxIni* ini)
{
    ToolboxWidget::SaveSettings(ini);
    SAVE_BOOL(showing_all_outposts);
    SAVE_BOOL(show_lines_on_world_map);
}

void WorldMapWidget::Draw(IDirect3DDevice9*)
{
    if (!GW::UI::GetIsWorldMapShowing()) {
        //ShowAllOutposts(showing_all_outposts = false);
        drawn = false;
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowPos(ImVec2(16.f, 16.f), ImGuiCond_FirstUseEver);
    visible = true;
    if (ImGui::Begin(Name(), &visible, GetWinFlags() | ImGuiWindowFlags_AlwaysAutoResize)) {
        const auto c = ImGui::GetCurrentContext();
        auto viewport_offset = c->CurrentViewport->Pos;
        viewport_offset.x *= -1;
        viewport_offset.y *= -1;

        if (ImGui::Checkbox("Show all areas", &showing_all_outposts)) {
            GW::GameThread::Enqueue([] {
                ShowAllOutposts(showing_all_outposts);
                });
        }
        show_all_rect = c->LastItemData.Rect;
        show_all_rect.Translate(viewport_offset);
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Outpost) {
            bool is_hard_mode = GW::PartyMgr::GetIsPartyInHardMode();
            ImGui::Checkbox("Hard mode", &is_hard_mode);
            hard_mode_rect = c->LastItemData.Rect;
            hard_mode_rect.Translate(viewport_offset);
        }
        else {
            memset(&hard_mode_rect, 0, sizeof(hard_mode_rect));
        }

        ImGui::Checkbox("Show toolbox minimap lines", &show_lines_on_world_map);
        show_lines_on_world_map_rect = c->LastItemData.Rect;
        show_lines_on_world_map_rect.Translate(viewport_offset);
    }
    ImGui::End();
    ImGui::PopStyleColor();

    const auto world_map_context = GW::Map::GetWorldMapContext();
    if (!(world_map_context && world_map_context->zoom == 1.0f))
        return;
    const auto viewport = ImGui::GetMainViewport();
    const auto& viewport_offset = viewport->Pos;

    const auto draw_list = ImGui::GetBackgroundDrawList(viewport);

    const auto ui_scale = GW::UI::GetFrameById(world_map_context->frame_id)->position.GetViewportScale(GW::UI::GetRootFrame());

    // Draw custom quest marker on world map
    const auto custom_quest = QuestModule::GetCustomQuestMarker();
    GW::Vec2f custom_quest_marker_world_pos;
    if (custom_quest && GamePosToWorldMap(custom_quest->marker, custom_quest_marker_world_pos)) {
        static constexpr auto uv0 = ImVec2(0.0f, 0.0f);
        static constexpr auto ICON_SIZE = ImVec2(24.0f, 24.0f);

        const ImVec2 viewport_quest_pos = {
            ui_scale.x * (custom_quest_marker_world_pos.x - world_map_context->top_left.x) + viewport_offset.x - (ICON_SIZE.x / 2.f),
            ui_scale.y * (custom_quest_marker_world_pos.y - world_map_context->top_left.y) + viewport_offset.y - (ICON_SIZE.y / 2.f)
        };

        const ImRect quest_marker_image_rect = {
            viewport_quest_pos, {viewport_quest_pos.x + ICON_SIZE.x, viewport_quest_pos.y + ICON_SIZE.y}
        };

        //draw_list->AddImage(*GwDatTextureModule::LoadTextureFromFileId(0x1b4d5), quest_marker_image_rect.GetTL(), quest_marker_image_rect.GetBR());

        if (quest_marker_image_rect.Contains(ImGui::GetMousePos())) {
            ImGui::SetTooltip("Custom marker placed @ %.2f, %.2f", custom_quest_marker_world_pos.x, custom_quest_marker_world_pos.y);
        }
    }
    /*for (const auto& portal : map_portals) {
        static constexpr auto uv0 = ImVec2(0.0f, 0.0f);
        static constexpr auto ICON_SIZE = ImVec2(24.0f, 24.0f);

        const ImVec2 portal_pos = {
            ui_scale.x * (portal.world_pos.x - world_map_context->top_left.x) + viewport_offset.x - (ICON_SIZE.x / 2.f),
            ui_scale.y * (portal.world_pos.y - world_map_context->top_left.y) + viewport_offset.y - (ICON_SIZE.y / 2.f)
        };



        const ImRect hover_rect = {
            portal_pos, {portal_pos.x + ICON_SIZE.x, portal_pos.y + ICON_SIZE.y}
        };

        draw_list->AddImage(*GwDatTextureModule::LoadTextureFromFileId(0x1b4d5), hover_rect.GetTL(), hover_rect.GetBR());


        if (hover_rect.Contains(ImGui::GetMousePos())) {
            ImGui::SetTooltip("Portal");
        }
    }*/
    if (show_lines_on_world_map) {
        const auto& lines = Minimap::Instance().custom_renderer.GetLines();
        const auto map_id = GW::Map::GetMapID();
        GW::Vec2f line_start;
        GW::Vec2f line_end;
        for (auto& line : lines | std::views::filter([](auto line) { return line->visible; })) {
            if (line->map != map_id)
                continue;
            if (!GamePosToWorldMap(line->p1, line_start))
                continue;
            if (!GamePosToWorldMap(line->p2, line_end))
                continue;

            line_start.x = (line_start.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_start.y = (line_start.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;
            line_end.x = (line_end.x - world_map_context->top_left.x) * ui_scale.x + viewport_offset.x;
            line_end.y = (line_end.y - world_map_context->top_left.y) * ui_scale.y + viewport_offset.y;

            draw_list->AddLine(line_start, line_end, line->color);
        }
    }
    drawn = true;
}

bool WorldMapWidget::WndProc(const UINT Message, WPARAM, LPARAM lParam)
{
    switch (Message) {
        case WM_GW_RBUTTONCLICK: {
            const auto world_map_context = GW::Map::GetWorldMapContext();
            if (!(world_map_context && world_map_context->zoom == 1.0f))
                break;

            const auto world_map_frame = GW::UI::GetFrameById(world_map_context->frame_id);
            const auto ui_scale =
                world_map_frame ?
                world_map_frame->position.GetViewportScale(GW::UI::GetRootFrame()) :
                GW::Vec2f{ GuiUtils::GetGWScaleMultiplier(), GuiUtils::GetGWScaleMultiplier() };
            world_map_click_pos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            world_map_click_pos.x /= ui_scale.x;
            world_map_click_pos.y /= ui_scale.y;
            world_map_click_pos.x += world_map_context->top_left.x;
            world_map_click_pos.y += world_map_context->top_left.y;
            ImGui::SetContextMenu(WorldMapContextMenu);
        } break;
        case WM_LBUTTONDOWN:
            if (!drawn || !GW::UI::GetIsWorldMapShowing()) {
                return false;
            }
            auto check_rect = [lParam](const ImRect& rect) {
                ImVec2 p = {(float)GET_X_LPARAM(lParam), (float)GET_Y_LPARAM(lParam)};
                return rect.Contains(p);
            };
            if (check_rect(remove_marker_rect)) {
                return true;
            }
            if (check_rect(place_marker_rect)) {
                return true;
            }
            if (check_rect(show_lines_on_world_map_rect)) {
                //show_lines_on_world_map = !show_lines_on_world_map;
                return true;
            }
            if (check_rect(hard_mode_rect)) {
                GW::GameThread::Enqueue([] {
                    GW::PartyMgr::SetHardMode(!GW::PartyMgr::GetIsPartyInHardMode());
                });
                return true;
            }
            if (check_rect(show_all_rect)) {
                //showing_all_outposts = !showing_all_outposts;

                return true;
            }
            break;
    }
    return false;
}

void WorldMapWidget::DrawSettingsInternal()
{
    ImGui::Text("Note: only visible in Hard Mode explorable areas.");
}
