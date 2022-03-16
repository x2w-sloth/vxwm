#define ALT      XCB_MOD_MASK_1
#define MOD      XCB_MOD_MASK_4
#define SHIFT    XCB_MOD_MASK_SHIFT
#define CTRL     XCB_MOD_MASK_CONTROL
#define PAGE(P)  (1 << (P))

#define VXWM_CLN_BORDER_W     4
#define VXWM_CLN_NORMAL_CLR   0x6B6B6B 
#define VXWM_CLN_FOCUS_CLR    0xCCCCCC 
#define VXWM_TAB_HEIGHT       8
#define VXWM_TAB_NORMAL_CLR   VXWM_CLN_NORMAL_CLR
#define VXWM_TAB_FOCUS_CLR    VXWM_CLN_FOCUS_CLR
#define VXWM_TAB_SELECT_CLR   0xFFA500
#define VXWM_FONT             "monospace"
#define VXWM_FONT_SIZE        22

static page_t pages[] = {
  { "1", column, {  3, -1, -1 } },
  { "2", stack,  {  2, -1, -1 } },
};

static const char *menucmd[] = { "dmenu_run", NULL };
static const char *termcmd[] = { "xterm", NULL };

static int incp0[2] = { 0, +1 };
static int decp0[2] = { 0, -1 };
static int incp1[2] = { 1, +1 };
static int decp1[2] = { 1, -1 };

static keybind_t keybinds[] = {
  { MOD|CTRL,    XK_q,       bn_quit,           { .v = NULL } },
  { MOD|SHIFT,   XK_Return,  bn_spawn,          { .v = menucmd } },
  { MOD,         XK_Return,  bn_spawn,          { .v = termcmd } },
  { MOD,         XK_m,       bn_merge_cln,      { .p = This } },
  { MOD,         XK_comma,   bn_split_cln,      { .v = NULL } },
  { MOD,         XK_q,       bn_kill_tab,       { .v = NULL } },
  { MOD,         XK_s,       bn_toggle_select,  { .v = NULL } },
  { MOD,         XK_d,       bn_toggle_float,   { .v = NULL } },
  { MOD,         XK_f,       bn_toggle_fullscr, { .v = NULL } },
  { MOD,         XK_space,   bn_focus_cln,      { .p = First } },
  { MOD,         XK_j,       bn_focus_cln,      { .p = Next } },
  { MOD,         XK_k,       bn_focus_cln,      { .p = Prev } },
  { MOD,         XK_l,       bn_focus_tab,      { .p = Next } },
  { MOD,         XK_h,       bn_focus_tab,      { .p = Prev } },
  { MOD|SHIFT,   XK_space,   bn_swap_cln,       { .p = Top } },
  { MOD|SHIFT,   XK_j,       bn_swap_cln,       { .p = Next } },
  { MOD|SHIFT,   XK_k,       bn_swap_cln,       { .p = Prev } },
  { MOD|SHIFT,   XK_l,       bn_swap_tab,       { .p = Next } },
  { MOD|SHIFT,   XK_h,       bn_swap_tab,       { .p = Prev } },
  { MOD|ALT,     XK_o,       bn_set_layout,     { .lt = column } },
  { MOD|ALT,     XK_i,       bn_set_layout,     { .lt = stack } },
  { MOD|ALT,     XK_j,       bn_set_param,      { .v = decp1 } },
  { MOD|ALT,     XK_k,       bn_set_param,      { .v = incp1 } },
  { MOD|ALT,     XK_l,       bn_set_param,      { .v = incp0 } },
  { MOD|ALT,     XK_h,       bn_set_param,      { .v = decp0 } },
  { MOD|SHIFT,   XK_1,       bn_set_tag,        { .u32 = PAGE(0) } },
  { MOD|SHIFT,   XK_2,       bn_set_tag,        { .u32 = PAGE(1) } },
  { MOD|CTRL,    XK_1,       bn_toggle_tag,     { .u32 = PAGE(0) } },
  { MOD|CTRL,    XK_2,       bn_toggle_tag,     { .u32 = PAGE(1) } },
  { MOD,         XK_1,       bn_focus_page,     { .i = 0 } },
  { MOD,         XK_2,       bn_focus_page,     { .i = 1 } },
};

static btnbind_t btnbinds[] = {
  { MOD, 1, bn_move_cln,   { .v = NULL } },
  { MOD, 3, bn_resize_cln, { .v = NULL } },
};

// vim: ts=2:sw=2:et
