// Where ImGui controls are, for tests that click them (tools/cvtest): with the environment variable given set to a
// file, each control marked in a frame (its rectangle in the window's pixels, by a name) and the page shown, as JSON,
// written again whenever they change. The launcher's (CV_LAUNCHER_STATE) and the in-game menu's (CV_MENU_STATE), whose
// controls a test also clicks by name: Click activates the one named, as keyboard or controller navigation does (a
// button pressed, a switch turned, a list opened, a choice taken), whatever window has the focus. A control scrolled
// out of sight (ImGui does not draw a child window wholly outside its parent, so neither what is in it) is scrolled to
// first, as a player would, and activated once it is drawn.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>

#include <imgui.h>
#include <imgui_internal.h>

class Marks {
public:
    explicit Marks(const char* variable) {
        if (const char* f = std::getenv(variable)) file_ = f;
    }
    bool On() const { return !file_.empty(); }
    // the control just drawn
    void Mark(const std::string& name) {
        if (file_.empty()) return;
        ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        // the window it scrolls in: its own, else the first above it that scrolls
        ImGuiWindow* w = ImGui::GetCurrentWindow();
        bool shown = w && !w->SkipItems && b.y > a.y && a.y >= w->ClipRect.Min.y - 1 && b.y <= w->ClipRect.Max.y + 1;
        while (w && w->ScrollMax.y <= 0 && (w->Flags & ImGuiWindowFlags_ChildWindow) && w->ParentWindow)
            w = w->ParentWindow;
        ids_[name] = {ImGui::GetItemID(), w, a.y, shown};
        char rect[96];
        std::snprintf(rect, sizeof(rect), "[%d, %d, %d, %d]", int(a.x), int(a.y), int(b.x), int(b.y));
        std::string key;  // as a JSON string: a status line's words may hold a file's name
        for (char c : name) {
            if (c == '"' || c == '\\') key += '\\';
            if (uint8_t(c) >= 0x20) key += c;
        }
        frame_ += (frame_.empty() ? "\"" : ", \"") + key + "\": " + rect;
    }
    // the frame's end
    void Write(int page) {
        if (file_.empty()) return;
        last_.swap(ids_);
        ids_.clear();
        std::string json = "{\"page\": " + std::to_string(page) + ", \"controls\": {" + frame_ + "}}\n";
        frame_.clear();
        if (json == written_) return;
        written_ = json;
        std::error_code ec;
        std::ofstream(file_ + ".tmp") << json;
        std::filesystem::rename(file_ + ".tmp", file_, ec);
    }
    // the control of that name of the frame before activated, at the next frame; false when there was none, or while it
    // is scrolled into sight (ask again)
    bool Click(const std::string& name) const {
        auto it = last_.find(name);
        if (it == last_.end()) return false;
        const Spot& s = it->second;
        // not drawn (its id 0 then) or out of sight: scrolled to, the click asked for again
        if (!s.shown && s.scroll) {
            float view = s.scroll->InnerRect.GetHeight();
            float to = s.scroll->Scroll.y + (s.y - s.scroll->InnerRect.Min.y) - view * 0.3f;
            ImGui::SetScrollY(s.scroll, std::clamp(to, 0.0f, s.scroll->ScrollMax.y));
            return false;
        }
        if (!s.id) return false;
        ImGui::ActivateItemByID(s.id);
        return true;
    }

private:
    struct Spot {
        ImGuiID id = 0;
        ImGuiWindow* scroll = nullptr;  // the window it scrolls in
        float y = 0;                    // its top, in the window's pixels
        bool shown = true;              // drawn in sight
    };
    std::string file_, frame_, written_;
    std::unordered_map<std::string, Spot> ids_, last_;
};
