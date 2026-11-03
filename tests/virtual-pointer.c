// Test client for the standard wlr virtual-pointer protocol. Generate its
// header/code using wayland-scanner and the protocol XML from wlr-protocols.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>
#include <linux/input-event-codes.h>
#include "virtual-pointer.h"

static struct zwlr_virtual_pointer_manager_v1 *manager;
static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    (void)data;
    if (!strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name))
        manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, version > 2 ? 2 : version);
}
static void removed(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data; (void)registry; (void)name;
}
static uint32_t milliseconds(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
int main(void) {
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) return 1;
    struct wl_registry *registry = wl_display_get_registry(display);
    const struct wl_registry_listener listener = {global, removed};
    wl_registry_add_listener(registry, &listener, NULL);
    if (wl_display_roundtrip(display) < 0 || !manager) return 2;
    struct zwlr_virtual_pointer_v1 *pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(manager, NULL);
    char command[256];
    puts("ready"); fflush(stdout);
    while (fgets(command, sizeof(command), stdin)) {
        unsigned x, y, width, height, pressed;
        double dx, dy;
        if (sscanf(command, "move %u %u %u %u", &x, &y, &width, &height) == 4)
            zwlr_virtual_pointer_v1_motion_absolute(pointer, milliseconds(), x, y, width, height);
        else if (sscanf(command, "relative %lf %lf", &dx, &dy) == 2)
            zwlr_virtual_pointer_v1_motion(pointer, milliseconds(), wl_fixed_from_double(dx), wl_fixed_from_double(dy));
        else if (sscanf(command, "button %u", &pressed) == 1)
            zwlr_virtual_pointer_v1_button(pointer, milliseconds(), BTN_LEFT, pressed ? WL_POINTER_BUTTON_STATE_PRESSED : WL_POINTER_BUTTON_STATE_RELEASED);
        else if (!strncmp(command, "quit", 4)) break;
        else return 3;
        zwlr_virtual_pointer_v1_frame(pointer);
        if (wl_display_roundtrip(display) < 0) return 4;
        puts("ok"); fflush(stdout);
    }
    zwlr_virtual_pointer_v1_destroy(pointer);
    wl_display_flush(display);
    wl_display_disconnect(display);
    return 0;
}
