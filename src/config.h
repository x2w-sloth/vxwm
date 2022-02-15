#define ALT   XCB_MOD_MASK_1
#define MOD   XCB_MOD_MASK_4
#define SHIFT XCB_MOD_MASK_SHIFT
#define CTRL  XCB_MOD_MASK_CONTROL

static layout_t layouts[] = {
  { "COL", column },
  { "STK", stack },
};

static page_t pages[] = {
  { "1", &layouts[0], {  3, -1, -1 } },
  { "2", &layouts[1], {  2, -1, -1 } },
};

static const char *termcmd[] = { "st", NULL };

static keybind_t keybinds[] = {
  { MOD|CTRL,    XK_q,       bn_quit,           { .v = NULL } },
  { MOD,         XK_Return,  bn_spawn,          { .v = termcmd } },
  { MOD,         XK_m,       bn_merge_cln,      { .t = This } },
  { MOD,         XK_comma,   bn_split_cln,      { .v = NULL } },
  { MOD,         XK_q,       bn_kill_tab,       { .v = NULL } },
  { MOD,         XK_s,       bn_toggle_select,  { .v = NULL } },
  { MOD,         XK_d,       bn_toggle_float,   { .v = NULL } },
  { MOD,         XK_f,       bn_toggle_fullscr, { .v = NULL } },
  { MOD,         XK_space,   bn_focus_cln,      { .t = First } },
  { MOD,         XK_j,       bn_focus_cln,      { .t = Next } },
  { MOD,         XK_k,       bn_focus_cln,      { .t = Prev } },
  { MOD,         XK_l,       bn_focus_tab,      { .t = Next } },
  { MOD,         XK_h,       bn_focus_tab,      { .t = Prev } },
  { MOD|SHIFT,   XK_space,   bn_swap_cln,       { .t = Top } },
  { MOD|SHIFT,   XK_j,       bn_swap_cln,       { .t = Next } },
  { MOD|SHIFT,   XK_k,       bn_swap_cln,       { .t = Prev } },
  { MOD|SHIFT,   XK_l,       bn_swap_tab,       { .t = Next } },
  { MOD|SHIFT,   XK_h,       bn_swap_tab,       { .t = Prev } },
  { MOD|ALT,     XK_j,       bn_set_layout,     { .i = 0 } },
  { MOD|ALT,     XK_k,       bn_set_layout,     { .i = 1 } },
  { MOD,         XK_1,       bn_focus_page,     { .i = 0 } },
  { MOD,         XK_2,       bn_focus_page,     { .i = 1 } },
};

static btnbind_t btnbinds[] = {
  { MOD, 1, bn_move_cln,   { .v = NULL } },
  { MOD, 3, bn_resize_cln, { .v = NULL } },
};

// vim: ts=2:sw=2:et
