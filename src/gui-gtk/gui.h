/** GUI based on GTK+ v3.
Copyright (c) 2019 Simon Zolin */

#include <fmedia.h>
#include <FF/gui-gtk/gtk.h>
#include <FFOS/thread.h>


#undef dbglog
#undef errlog
#undef syserrlog
#define dbglog(...)  fmed_dbglog(core, NULL, "gui", __VA_ARGS__)
#define errlog(...)  fmed_errlog(core, NULL, "gui", __VA_ARGS__)
#define syserrlog(...)  fmed_syserrlog(core, NULL, "gui", __VA_ARGS__)


enum {
	MAXVOL = 125,
};

#define CTL_CONF_FN  "fmedia.gui.conf"
#define FMED_USERCONF  "fmedia-user.conf"
#define AUTOPLIST_FN  "list%u.m3u8"

struct gui_wmain {
	ffui_wnd wmain;
	ffui_menu mm;
	ffui_btn bpause
		, bstop
		, bprev
		, bnext;
	ffui_label lpos;
	ffui_trkbar tvol;
	ffui_trkbar tpos;
	ffui_tab tabs;
	ffui_view vlist;
	ffui_ctl stbar;
	ffui_trayicon tray_icon;
};

struct gui_wconvert {
	ffui_wnd wconvert;
	ffui_menu mmconv;
	ffui_label lfn, lsets;
	ffui_edit eout;
	ffui_btn boutbrowse;
	ffui_view vsets;
};

struct gui_wabout {
	ffui_wnd wabout;
	ffui_label labout, lurl;
};

struct gui_wuri {
	ffui_wnd wuri;
	ffui_edit turi;
	ffui_btn bok;
};

struct gui_winfo {
	ffui_wnd winfo;
	ffui_view vinfo;
};

struct gtrk;
struct conv_sets {
	uint init :1;

	char *output;
};
struct gui_conf {
	uint seek_step_delta,
		seek_leap_delta;
	byte autosave_playlists;
	byte list_random;
	ushort list_col_width[16];
};
typedef struct ggui {
	uint state;
	uint load_err;
	const fmed_queue *qu;
	const fmed_track *track;
	ffthd th;
	struct gtrk *curtrk;
	int focused;
	uint vol; //0..MAXVOL
	uint go_pos;
	uint tabs_counter;

	struct gui_conf conf;
	struct conv_sets conv_sets;

	struct gui_wmain wmain;
	struct gui_wconvert wconvert;
	struct gui_wabout wabout;
	struct gui_wuri wuri;
	struct gui_winfo winfo;
	ffui_dialog dlg;
	ffui_menu mfile;
	ffui_menu mlist;
	ffui_menu mplay;
	ffui_menu mconvert;
	ffui_menu mhelp;
} ggui;

extern const fmed_core *core;
extern ggui *gg;

enum ACTION {
	A_NONE,

	A_LIST_ADDFILE,
	A_LIST_ADDURL,
	A_SHOWPCM,
	A_SHOWINFO,
	A_SHOWDIR,
	A_DELFILE,
	A_SHOW,
	A_HIDE,
	A_QUIT,

	A_PLAY,
	A_PLAYPAUSE,
	A_SEEK,
	A_STOP,
	A_STOP_AFTER,
	A_NEXT,
	A_PREV,
	A_FFWD,
	A_RWND,
	A_LEAP_FWD,
	A_LEAP_BACK,
	A_SETGOPOS,
	A_GOPOS,
	A_VOL,
	A_VOLUP,
	A_VOLDOWN,
	A_VOLRESET,

	A_LIST_NEW,
	A_LIST_DEL,
	A_LIST_SEL,
	A_LIST_SAVE,
	A_LIST_SELECTALL,
	A_LIST_REMOVE,
	A_LIST_RMDEAD,
	A_LIST_CLEAR,
	A_LIST_RANDOM,
	A_LIST_SORTRANDOM,

	A_SHOWCONVERT,
	A_CONVERT,
	A_CONVOUTBROWSE,

	A_ABOUT,
	A_CONF_EDIT,
	A_USRCONF_EDIT,
	A_FMEDGUI_EDIT,
	A_README_SHOW,
	A_CHANGES_SHOW,

	A_URL_ADD,

	A_ONCLOSE,
	A_ONDROPFILE,
	LOADLISTS,
	LIST_DISPINFO,
};

void corecmd_add(uint cmd, void *udata);
void ctlconf_write(void);
void usrconf_write(void);
void gui_showtextfile(uint id);

void wmain_init();
void wmain_newtrack(fmed_que_entry *ent, uint time_total, fmed_filt *d);
void wmain_fintrack();
void wmain_update(uint playtime, uint time_total);
void wmain_ent_added(uint idx);
void wmain_ent_removed(uint idx);
void wmain_status(const char *fmt, ...);
void wmain_tab_new();
void wmain_list_clear();
void wmain_list_cols_width_write(ffconfw *conf);
void wmain_list_update(uint idx, int delta);

int conf_convert(ffparser_schem *p, void *obj, ffpars_ctx *ctx);
void wconvert_init();
void wconv_destroy();
void wconv_show();
void convert();

void wabout_init();
void wuri_init();
void winfo_init();
void winfo_show(uint idx);
