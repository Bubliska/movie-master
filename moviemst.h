#ifndef MOVIEMST_H
#define MOVIEMST_H
#include <stdio.h>
#include <stddef.h>
#define MAX_TEXT 8190
#define MAX_PARAS 16384
enum {SCENE, ACTION, CHARACTER, DIALOGUE, PAREN, TRANSITION, TYPES};
#define MAX_SCENE_LABELS 65536
typedef struct { int base, suffix; } SceneNumber;
typedef struct { int type; char *text; int scene_id; } Para;
typedef struct {
    Para *p; int n, cap; char title[128], author[128];
    int numbering, locked;
    SceneNumber *numbers; int nn, nc;
} Doc;
typedef struct { int p, o; } Pos;
typedef struct { Doc d; Pos cur, anchor; int selecting; } State;
typedef struct { int p, start, end, indent, page, line; } Row;
typedef struct { Row *r; int n, cap, pages; } Layout;
typedef struct { State undo[32], redo[32]; int un, rn; size_t bytes; } History;
typedef struct { char *name; int directory; } FileEntry;
enum { PATH_OTHER=0, PATH_FILE=1, PATH_DIR=2 };
extern const unsigned short unicode857[256];
extern const char *type_names[TYPES];
void *mem(size_t n);
char *dupstr(const char *s);
void doc_init(Doc *d);
void doc_free(Doc *d);
void doc_clone(Doc *dst, const Doc *src);
int doc_insert(Doc *d, int at, int type, const char *text);
void doc_remove(Doc *d, int at);
void state_free(State *s);
void state_clone(State *dst, const State *src);
void history_clear(History *h);
void checkpoint(History *h, const State *s);
int undo(History *h, State *s, int redo);
int pos_cmp(Pos a, Pos b);
int selection(const State *s, Pos *a, Pos *b);
int delete_selection(State *s);
int insert_text(State *s, const char *text);
int split_para(State *s);
int backspace(State *s, int forward);
int set_type(State *s, int type);
int element_char(int type, int c);
const char *element_find(int type, const char *text, const char *needle);
int copy_selection(State *s, Doc *clip);
int paste(State *s, const Doc *clip);
int upper857(int c);
int lower857(int c);
int from_unicode(unsigned int c);
int utf8_decode(const char *in, char *out, size_t cap);
char *utf8_encode(const char *in);
enum {KEY_SYSTEM,KEY_US,KEY_UK,KEY_DVORAK,KEY_TR_Q,KEY_TR_F,KEY_ES,KEY_FR,KEY_DE,KEY_IT,KEY_PT,KEY_BR,KEY_COUNT};
extern int keyboard_layout;
extern const char *keyboard_names[KEY_COUNT],*keyboard_tags[KEY_COUNT];
int keyboard_key(int layout,int scan,int shift,int caps,int altgr,int compose);
int keyboard_pending(void);
void keyboard_reset(void);
int letter_case(int c,int upper);
unsigned long doc_hash(const Doc *d);
int save_doc(const Doc *d, const char *path, char *error);
int load_doc(Doc *d, const char *path, char *error);
int import_fountain(Doc *d, const char *path, char *error, int *warnings);
FILE *file_open(const char *path, const char *mode);
int file_remove(const char *path);
int file_rename(const char *from, const char *to);
int path_type(const char *path);
int path_join(char *out, const char *folder, const char *name);
void path_split(const char *path, char *folder, char *name);
int destination_path(char *out, const char *folder, const char *name, const char *extension, char *error);
void path_parent(char *path);
void path_display(const char *path, char *out, size_t cap);
void desktop_path(char *out);
int file_matches(const char *name, int filter);
int files_list(const char *path, FileEntry **entries, int *count, int filter, char *error);
void files_free(FileEntry *entries, int count);
int drive_roots(char roots[26][4]);
void layout_build(const Doc *d, Layout *l);
void layout_free(Layout *l);
int scene_end(const Doc *d, int start);
int scene_count(const Doc *d);
void scene_label(const Doc *d, int para, char out[16]);
int scene_register(Doc *d, int base, int suffix);
int scene_assign(Doc *d, int para);
int scene_parse(const char *label, int *base, int *suffix);
void scene_lock(Doc *d, int locked);
int scene_duplicate(State *s);
int scene_move(State *s, int direction);
int export_text(const Doc *d, const char *path, int fountain, char *error);
int export_pdf(const Doc *d, const char *path, const char *font, char *error);
int selftest(void);
#endif
