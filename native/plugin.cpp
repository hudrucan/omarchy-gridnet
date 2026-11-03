#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <linux/input-event-codes.h>
#include <wayland-server-core.h>

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "geometry.hpp"

namespace {
using omasnap::Box;
using omasnap::Zone;
using Target = Layout::ITarget;

Box plain(const CBox& b) { return {b.x, b.y, b.w, b.h}; }
CBox native(Box b) { return {b.x, b.y, b.w, b.h}; }

// Floating targets describe the client surface. Snap the whole frame,
// including Omarchy's reserved titlebar and the compositor border.
CBox frame(SP<Target> target, CBox client) {
    const auto w = target->window();
    const auto reserved = w->getFullWindowReservedArea();
    const Vector2D border{double(w->getRealBorderSize()), double(w->getRealBorderSize())};
    return {client.pos() - reserved.topLeft - border,
            client.size() + reserved.topLeft + reserved.bottomRight + border * 2};
}

CBox client(SP<Target> target, CBox outer) {
    const auto w = target->window();
    const auto reserved = w->getFullWindowReservedArea();
    const Vector2D border{double(w->getRealBorderSize()), double(w->getRealBorderSize())};
    return {outer.pos() + reserved.topLeft + border,
            (outer.size() - reserved.topLeft - reserved.bottomRight - border * 2).clamp({1, 1}, {})};
}

struct Saved {
    WP<Target> target;
    CBox original;
    CBox snapped;
};
struct Drag {
    WP<Target> target;
    CBox original;
    Vector2D pointer;
    bool moved = false;
    bool restored = false;
};

class Snap {
  public:
    std::vector<CHyprSignalListener> listeners;
    SP<SHyprCtlCommand> command;
    wl_event_source* idle = nullptr;
    std::optional<Drag> drag, drop;
    std::vector<Saved> saved;
    PHLMONITORREF previewMonitor;
    CBox previewBox;
    Zone previewZone = Zone::none;
    double edge = 24, corner = 160, gap = 8;
    bool preview = true, keepOnScreen = true;
    bool cancelled = false;
    CHyprColor accent{0.35, 0.65, 1.0, 0.22};

    ~Snap() {
        if (idle) wl_event_source_remove(idle);
        hide();
    }

    bool eligible(SP<Target> t) const {
        const auto w = t ? t->window() : nullptr;
        return validMapped(w) && !w->isHidden() && t->floating()
            && !g_pSessionLockManager->isSessionLocked()
            && w->m_ruleApplicator->m_tagKeeper.isTagged("omarchy-floating-workspace");
    }

    PHLMONITOR monitorAt(Vector2D point) const {
        for (const auto& mon : State::monitorState()->monitors())
            if (mon->logicalBox().containsPoint(point)) return mon;
        return nullptr;
    }

    void hide() {
        if (auto mon = previewMonitor.lock()) g_pHyprRenderer->damageMonitor(mon);
        previewMonitor.reset();
        previewZone = Zone::none;
    }

    void show(PHLMONITOR mon, Zone zone) {
        if (!preview || zone == Zone::none || !mon) { hide(); return; }
        const auto box = native(omasnap::placement(zone, plain(mon->logicalBoxMinusReserved()), gap));
        if (mon == previewMonitor.lock() && zone == previewZone && box == previewBox) return;
        hide();
        previewMonitor = mon;
        previewZone = zone;
        previewBox = box;
        g_pHyprRenderer->damageMonitor(mon);
    }

    Saved* savedFor(SP<Target> target) {
        std::erase_if(saved, [](const auto& s) { return s.target.expired(); });
        for (auto& s : saved) if (s.target.lock() == target) return &s;
        return nullptr;
    }

    void forget(SP<Target> target) {
        std::erase_if(saved, [&](const auto& s) { return s.target.expired() || s.target.lock() == target; });
    }

    void place(SP<Target> target, PHLMONITOR mon, Zone zone, CBox original) {
        if (!eligible(target) || !mon || zone == Zone::none) return;
        const auto box = client(target, native(omasnap::placement(zone, plain(mon->logicalBoxMinusReserved()), gap)));
        if (const auto s = savedFor(target)) original = s->original;
        forget(target);
        g_layoutManager->setTargetGeom(box, target);
        target->rememberFloatingSize(target->position().size());
        Desktop::windowState()->raise(target->window());
        saved.push_back({target, original, target->position()});
    }

    void schedule() {
        if (!idle)
            idle = wl_event_loop_add_idle(g_pCompositor->m_wlEventLoop, [](void* data) {
                auto& self = *static_cast<Snap*>(data);
                self.idle = nullptr;
                self.flush();
            }, this);
    }

    void motion() {
        if (cancelled) return;
        const auto& controller = g_layoutManager->dragController();
        const auto target = controller->target();
        if (controller->mode() != MBIND_MOVE || controller->draggingTiled() || !eligible(target)) {
            drag.reset();
            hide();
            if (target && controller->mode() != MBIND_MOVE) forget(target);
            return;
        }
        // This signal runs before Hyprland applies the motion. Keep the actual
        // drag target; the focused window can change before release.
        if (!drag || drag->target.lock() != target)
            drag = Drag{target, target->position(), g_pInputManager->getMouseCoordsInternal()};
        schedule();
    }

    void release(const IPointer::SButtonEvent& event) {
        if (event.button != BTN_LEFT) return;
        if (event.state == WL_POINTER_BUTTON_STATE_PRESSED) { cancelled = false; return; }
        if (!cancelled && drag && g_layoutManager->dragController()->mode() == MBIND_MOVE) drop = drag;
        cancelled = false;
        drag.reset();
        // Native dragEnd may still move/group the target. Apply only after it
        // finishes, and recheck that it remains a floating window.
        schedule();
    }

    void flush() {
        if (drop) {
            auto completed = *drop;
            drop.reset();
            hide();
            const auto t = completed.target.lock();
            if (!eligible(t)) return;
            const auto cursor = g_pInputManager->getMouseCoordsInternal();
            if (!completed.moved && (t->position().pos() - completed.original.pos()).size() < 6) return;
            const auto mon = monitorAt(cursor);
            if (!mon) return;
            const auto zone = omasnap::zoneAt({cursor.x, cursor.y}, plain(mon->logicalBox()), edge, corner);
            if (zone != Zone::none) place(t, mon, zone, completed.original);
            else if (keepOnScreen) {
                const auto before = t->position();
                const auto after = client(t, native(omasnap::keepVisible(plain(frame(t, before)), plain(mon->logicalBoxMinusReserved()))));
                if (after != before) g_layoutManager->setTargetGeom(after, t);
            }
            return;
        }

        if (!drag) { hide(); return; }
        const auto t = drag->target.lock();
        const auto& controller = g_layoutManager->dragController();
        if (!eligible(t) || controller->target() != t || controller->mode() != MBIND_MOVE) {
            drag.reset();
            hide();
            return;
        }
        const auto cursor = g_pInputManager->getMouseCoordsInternal();
        if (!controller->dragThresholdReached()) return;
        drag->moved = drag->moved || (t->position().pos() - drag->original.pos()).size() >= 6;
        if (!drag->moved) return;

        if (!drag->restored) {
            drag->restored = true;
            if (const auto s = savedFor(t)) {
                const auto original = s->original;
                const auto snapped = s->snapped;
                // A manual resize supersedes the old restore size.
                if ((t->position().size() - snapped.size()).size() < 2) {
                    const double fraction = std::clamp((drag->pointer.x - drag->original.x) / drag->original.w, 0.0, 1.0);
                    const auto reserved = t->window()->getFullWindowReservedArea();
                    const double grabY = std::clamp(drag->pointer.y - drag->original.y, -reserved.topLeft.y, 24.0);
                    const CBox restored{cursor.x - fraction * original.w, cursor.y - grabY, original.w, original.h};
                    g_layoutManager->setTargetGeom(restored, t);
                    t->warpPositionSize();
                    // Rebase native dragging on the restored size, using the
                    // public controller API rather than replacing its code.
                    controller->dragBegin(t, MBIND_MOVE, std::nullopt, controller->exclusiveDeviceGrab());
                    drag->original = original;
                }
                forget(t);
            }
        }
        const auto mon = monitorAt(cursor);
        const auto zone = mon ? omasnap::zoneAt({cursor.x, cursor.y}, plain(mon->logicalBox()), edge, corner) : Zone::none;
        show(mon, zone);
    }

    void render(eRenderStage stage) {
        if (stage != RENDER_LAST_MOMENT || g_pSessionLockManager->isSessionLocked()) return;
        const auto mon = previewMonitor.lock();
        if (!mon || g_pHyprRenderer->m_renderData.pMonitor.lock() != mon) return;
        auto box = previewBox;
        box.translate(-mon->m_position).scale(mon->m_scale).round();
        CRectPassElement::SRectData data{};
        data.box = box;
        data.color = accent;
        data.round = int(8 * mon->m_scale);
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(data));
    }

    std::string request(const std::string& input) {
        std::istringstream in(input);
        std::string prefix, action;
        in >> prefix >> action;
        if (action.empty()) action = "status";
        if (action == "status")
            return std::string("{\"ready\":true,\"dragging\":") + (drag ? "true" : "false")
                + ",\"zone\":\"" + std::string(omasnap::zoneName(previewZone)) + "\"}";
        if (action == "configure") {
            double nextEdge, nextCorner, nextGap;
            int nextPreview, nextKeep;
            unsigned rgb;
            if (!(in >> nextEdge >> nextCorner >> nextGap >> nextPreview >> nextKeep >> std::hex >> rgb)
                || !std::isfinite(nextEdge) || !std::isfinite(nextCorner) || !std::isfinite(nextGap)
                || nextEdge < 4 || nextEdge > 100 || nextCorner < 16 || nextCorner > 500
                || nextGap < 0 || nextGap > 100 || rgb > 0xffffff
                || (nextPreview != 0 && nextPreview != 1) || (nextKeep != 0 && nextKeep != 1))
                return "error: invalid configuration";
            edge = nextEdge; corner = nextCorner; gap = nextGap;
            preview = nextPreview; keepOnScreen = nextKeep;
            accent = CHyprColor{float((rgb >> 16) & 255) / 255.f, float((rgb >> 8) & 255) / 255.f, float(rgb & 255) / 255.f, 0.22f};
            hide();
            return "ok";
        }
        const auto w = Desktop::focusState()->window();
        const auto t = w ? w->m_target : nullptr;
        if (!eligible(t)) return "error: focus a window on a floating workspace first";
        if (g_layoutManager->dragController()->target()) return "error: a drag is in progress";
        if (action == "restore") {
            const auto s = savedFor(t);
            if (!s) return "error: no saved geometry for this window";
            const auto original = s->original;
            forget(t);
            g_layoutManager->setTargetGeom(original, t);
            return "ok";
        }
        if (action == "snap") {
            std::string name;
            in >> name;
            const auto zone = omasnap::parseZone(name);
            if (zone == Zone::none) return "error: unknown snap zone";
            place(t, w->m_monitor.lock(), zone, t->position());
            return "ok";
        }
        return "error: expected status, configure, snap <zone>, or restore";
    }
};

std::unique_ptr<Snap> instance;
}

APICALL EXPORT std::string PLUGIN_API_VERSION() { return HYPRLAND_API_VERSION; }

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    if (std::string(__hyprland_api_get_hash()) != __hyprland_api_get_client_hash())
        throw std::runtime_error("Omasnap: installed headers do not match the running Hyprland; rebuild after logging in again");
    instance = std::make_unique<Snap>();
    auto& events = Event::bus()->m_events;
    instance->listeners.push_back(events.input.mouse.move.listen([](Vector2D, Event::SCallbackInfo&) { instance->motion(); }));
    instance->listeners.push_back(events.input.mouse.button.listen([](IPointer::SButtonEvent e, Event::SCallbackInfo&) { instance->release(e); }));
    instance->listeners.push_back(events.input.keyboard.key.listen([](IKeyboard::SKeyEvent e, Event::SCallbackInfo&) {
        if (e.keycode == KEY_ESC && instance->drag) {
            instance->cancelled = true;
            instance->drag.reset(); instance->drop.reset(); instance->hide();
        }
    }));
    instance->listeners.push_back(events.window.close.listen([](PHLWINDOW w) {
        instance->forget(w->m_target);
        if (instance->drag && instance->drag->target.lock() == w->m_target) { instance->drag.reset(); instance->hide(); }
    }));
    instance->listeners.push_back(events.render.stage.listen([](eRenderStage stage) { instance->render(stage); }));
    instance->command = HyprlandAPI::registerHyprCtlCommand(handle, SHyprCtlCommand{
        .name = "omasnap", .exact = false,
        .fn = [](eHyprCtlOutputFormat, std::string request) { return instance->request(request); }});
    if (!instance->command) throw std::runtime_error("Omasnap: could not register IPC command");
    return {"omasnap-window-snap", "Edge and corner snapping for native Omarchy floating workspaces", "Omasnap contributors", "0.1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() { instance.reset(); }
