// vibe module for VibeOS
// Python bindings to kernel API

#include "py/runtime.h"
#include "py/obj.h"
#include "vibe.h"

// External reference to kernel API
extern kapi_t *mp_vibeos_api;

// vibe.clear()
static mp_obj_t mod_vibe_clear(void) {
    mp_vibeos_api->clear();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_clear_obj, mod_vibe_clear);

// vibe.puts(s)
static mp_obj_t mod_vibe_puts(mp_obj_t s_obj) {
    const char *s = mp_obj_str_get_str(s_obj);
    mp_vibeos_api->puts(s);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mod_vibe_puts_obj, mod_vibe_puts);

// vibe.set_color(fg, bg)
static mp_obj_t mod_vibe_set_color(mp_obj_t fg_obj, mp_obj_t bg_obj) {
    uint32_t fg = mp_obj_get_int(fg_obj);
    uint32_t bg = mp_obj_get_int(bg_obj);
    mp_vibeos_api->set_color(fg, bg);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mod_vibe_set_color_obj, mod_vibe_set_color);

// vibe.sleep_ms(ms)
static mp_obj_t mod_vibe_sleep_ms(mp_obj_t ms_obj) {
    uint32_t ms = mp_obj_get_int(ms_obj);
    mp_vibeos_api->sleep_ms(ms);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mod_vibe_sleep_ms_obj, mod_vibe_sleep_ms);

// vibe.uptime_ms()
static mp_obj_t mod_vibe_uptime_ms(void) {
    return mp_obj_new_int(mp_vibeos_api->get_uptime_ticks() * 10);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_uptime_ms_obj, mod_vibe_uptime_ms);

// vibe.has_key()
static mp_obj_t mod_vibe_has_key(void) {
    return mp_obj_new_bool(mp_vibeos_api->has_key());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_has_key_obj, mod_vibe_has_key);

// vibe.getc()
static mp_obj_t mod_vibe_getc(void) {
    return mp_obj_new_int(mp_vibeos_api->getc());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_getc_obj, mod_vibe_getc);

// vibe.yield()
static mp_obj_t mod_vibe_yield(void) {
    mp_vibeos_api->yield();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_yield_obj, mod_vibe_yield);

// --- Graphics ---

// vibe.put_pixel(x, y, color)
static mp_obj_t mod_vibe_put_pixel(mp_obj_t x_obj, mp_obj_t y_obj, mp_obj_t c_obj) {
    uint32_t x = mp_obj_get_int(x_obj);
    uint32_t y = mp_obj_get_int(y_obj);
    uint32_t c = mp_obj_get_int(c_obj);
    mp_vibeos_api->fb_put_pixel(x, y, c);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(mod_vibe_put_pixel_obj, mod_vibe_put_pixel);

// vibe.fill_rect(x, y, w, h, color)
static mp_obj_t mod_vibe_fill_rect(size_t n_args, const mp_obj_t *args) {
    uint32_t x = mp_obj_get_int(args[0]);
    uint32_t y = mp_obj_get_int(args[1]);
    uint32_t w = mp_obj_get_int(args[2]);
    uint32_t h = mp_obj_get_int(args[3]);
    uint32_t c = mp_obj_get_int(args[4]);
    mp_vibeos_api->fb_fill_rect(x, y, w, h, c);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_vibe_fill_rect_obj, 5, 5, mod_vibe_fill_rect);

// vibe.draw_string(x, y, s, fg, bg)
static mp_obj_t mod_vibe_draw_string(size_t n_args, const mp_obj_t *args) {
    uint32_t x = mp_obj_get_int(args[0]);
    uint32_t y = mp_obj_get_int(args[1]);
    const char *s = mp_obj_str_get_str(args[2]);
    uint32_t fg = mp_obj_get_int(args[3]);
    uint32_t bg = mp_obj_get_int(args[4]);
    mp_vibeos_api->fb_draw_string(x, y, s, fg, bg);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_vibe_draw_string_obj, 5, 5, mod_vibe_draw_string);

// vibe.screen_size() -> (width, height)
static mp_obj_t mod_vibe_screen_size(void) {
    mp_obj_t tuple[2];
    tuple[0] = mp_obj_new_int(mp_vibeos_api->fb_width);
    tuple[1] = mp_obj_new_int(mp_vibeos_api->fb_height);
    return mp_obj_new_tuple(2, tuple);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_screen_size_obj, mod_vibe_screen_size);

// --- Mouse ---

// vibe.mouse_pos() -> (x, y)
static mp_obj_t mod_vibe_mouse_pos(void) {
    int x, y;
    mp_vibeos_api->mouse_get_pos(&x, &y);
    mp_obj_t tuple[2];
    tuple[0] = mp_obj_new_int(x);
    tuple[1] = mp_obj_new_int(y);
    return mp_obj_new_tuple(2, tuple);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_mouse_pos_obj, mod_vibe_mouse_pos);

// vibe.mouse_buttons() -> int
static mp_obj_t mod_vibe_mouse_buttons(void) {
    return mp_obj_new_int(mp_vibeos_api->mouse_get_buttons());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_mouse_buttons_obj, mod_vibe_mouse_buttons);

// --- Memory info ---

// vibe.mem_free()
static mp_obj_t mod_vibe_mem_free(void) {
    return mp_obj_new_int(mp_vibeos_api->get_mem_free());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_mem_free_obj, mod_vibe_mem_free);

// vibe.mem_used()
static mp_obj_t mod_vibe_mem_used(void) {
    return mp_obj_new_int(mp_vibeos_api->get_mem_used());
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_vibe_mem_used_obj, mod_vibe_mem_used);

// Module globals table
// Color constants come from vibe.h
static const mp_rom_map_elem_t mp_module_vibe_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_vibe) },

    // Console
    { MP_ROM_QSTR(MP_QSTR_clear), MP_ROM_PTR(&mod_vibe_clear_obj) },
    { MP_ROM_QSTR(MP_QSTR_puts), MP_ROM_PTR(&mod_vibe_puts_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_color), MP_ROM_PTR(&mod_vibe_set_color_obj) },

    // Input
    { MP_ROM_QSTR(MP_QSTR_has_key), MP_ROM_PTR(&mod_vibe_has_key_obj) },
    { MP_ROM_QSTR(MP_QSTR_getc), MP_ROM_PTR(&mod_vibe_getc_obj) },

    // Timing
    { MP_ROM_QSTR(MP_QSTR_sleep_ms), MP_ROM_PTR(&mod_vibe_sleep_ms_obj) },
    { MP_ROM_QSTR(MP_QSTR_uptime_ms), MP_ROM_PTR(&mod_vibe_uptime_ms_obj) },
    { MP_ROM_QSTR(MP_QSTR_yield), MP_ROM_PTR(&mod_vibe_yield_obj) },

    // Graphics
    { MP_ROM_QSTR(MP_QSTR_put_pixel), MP_ROM_PTR(&mod_vibe_put_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill_rect), MP_ROM_PTR(&mod_vibe_fill_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_draw_string), MP_ROM_PTR(&mod_vibe_draw_string_obj) },
    { MP_ROM_QSTR(MP_QSTR_screen_size), MP_ROM_PTR(&mod_vibe_screen_size_obj) },

    // Mouse
    { MP_ROM_QSTR(MP_QSTR_mouse_pos), MP_ROM_PTR(&mod_vibe_mouse_pos_obj) },
    { MP_ROM_QSTR(MP_QSTR_mouse_buttons), MP_ROM_PTR(&mod_vibe_mouse_buttons_obj) },

    // Memory
    { MP_ROM_QSTR(MP_QSTR_mem_free), MP_ROM_PTR(&mod_vibe_mem_free_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem_used), MP_ROM_PTR(&mod_vibe_mem_used_obj) },

    // Color constants
    { MP_ROM_QSTR(MP_QSTR_BLACK), MP_ROM_INT(COLOR_BLACK) },
    { MP_ROM_QSTR(MP_QSTR_WHITE), MP_ROM_INT(COLOR_WHITE) },
    { MP_ROM_QSTR(MP_QSTR_RED), MP_ROM_INT(COLOR_RED) },
    { MP_ROM_QSTR(MP_QSTR_GREEN), MP_ROM_INT(COLOR_GREEN) },
    { MP_ROM_QSTR(MP_QSTR_BLUE), MP_ROM_INT(COLOR_BLUE) },
    { MP_ROM_QSTR(MP_QSTR_YELLOW), MP_ROM_INT(COLOR_YELLOW) },
    { MP_ROM_QSTR(MP_QSTR_CYAN), MP_ROM_INT(COLOR_CYAN) },
    { MP_ROM_QSTR(MP_QSTR_MAGENTA), MP_ROM_INT(COLOR_MAGENTA) },
};

static MP_DEFINE_CONST_DICT(mp_module_vibe_globals, mp_module_vibe_globals_table);

const mp_obj_module_t mp_module_vibe = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_vibe_globals,
};

MP_REGISTER_MODULE(MP_QSTR_vibe, mp_module_vibe);
