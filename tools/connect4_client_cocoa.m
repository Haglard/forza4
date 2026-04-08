// =============================================
// File: tools/connect4_client_cocoa.m
// Project: forza4 — Native macOS Cocoa GUI
// License: MIT (c) 2025
// =============================================
#import <Cocoa/Cocoa.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "game/api.h"
#include "game/connect4_adapter.h"
#include "connect4/board.h"
#include "connect4/movegen.h"
#include "engine/search.h"
#include "core/log.h"

// ---- Engine thread ----
typedef struct {
    const GameAPI   *api;
    game_state_t    *st;
    search_params_t  sp;
    search_result_t  out;
    void (^completion)(search_result_t *);
} engine_job_t;

static void *engine_thread(void *arg) {
    engine_job_t *job = (engine_job_t *)arg;
    search_root(job->api, job->st, &job->sp, &job->out);
    search_result_t *outp = malloc(sizeof(search_result_t));
    *outp = job->out;
    void (^cb)(search_result_t *) = job->completion;
    free(job);
    dispatch_async(dispatch_get_main_queue(), ^{ cb(outp); free(outp); });
    return NULL;
}

// ---- Layout ----
#define CELL   72.0
#define COLS   7
#define ROWS   6
#define BRD_X  40.0
#define BRD_Y  60.0
#define BRD_W  (COLS * CELL)
#define BRD_H  (ROWS * CELL)
#define WIN_W  620.0
#define WIN_H  600.0
#define PANEL_X (BRD_X + BRD_W + 18.0)
#define UNDO_MAX 42

typedef enum { MODE_HVC = 1, MODE_CVC = 2 } play_mode_t;

typedef struct {
    game_move_t mv;
    char  buf[64];
    int   hist_n;
    int32_t scoreWhite;
} undo_entry_t;

// ================================================================
// C4View
// ================================================================
@interface C4View : NSView {
    const GameAPI  *_api;
    game_state_t   *_st;
    game_state_t   *_eng_st;
    play_mode_t     _mode;
    int             _humanColor;

    undo_entry_t    _undo[UNDO_MAX];
    int             _undoTop;

    char   _hist[UNDO_MAX][4];
    int    _histN;

    int      _hoverCol;   // column mouse is over (-1 = none)
    int      _lastCol;    // last dropped column

    NSString *_message;
    int32_t   _scoreWhite;

    BOOL     _sinfoValid;
    int      _sinfoSide;
    int32_t  _sinfoScore;
    int      _sinfoDepth;
    uint64_t _sinfoNodes;
    double   _sinfoNPS;
    double   _sinfoTimeSec;
    char     _sinfoPV[40];

    volatile int _stopEngine;
    int          _generation;
    BOOL         _engineBusy;

    search_params_t _sp;
}
- (void)showSetupDialog:(BOOL)startup;
- (void)promptNewGame;
- (void)undoLastMove;
@end

@implementation C4View

- (instancetype)initWithFrame:(NSRect)fr {
    self = [super initWithFrame:fr];
    if (!self) return nil;
    _api      = c4_api();
    _st       = calloc(1, _api->state_size);
    _eng_st   = calloc(1, _api->state_size);
    c4_init_state_str(_st, "startpos");
    _hoverCol  = -1;
    _lastCol   = -1;
    _message   = @"Configura la partita dal menu Partita → Nuova Partita";
    _stopEngine = 1;
    return self;
}
- (void)dealloc { free(_st); free(_eng_st); }
- (BOOL)acceptsFirstResponder { return YES; }

// ----------------------------------------------------------------
// Setup
// ----------------------------------------------------------------
- (void)resetGame:(play_mode_t)mode humanColor:(int)hc timeMs:(int)tms {
    _stopEngine = 1; _generation++;
    _mode = mode; _humanColor = hc;
    _hoverCol = -1; _lastCol = -1;
    _undoTop = 0; _histN = 0;
    _message = @"Partita iniziata";
    _sinfoValid = NO; _scoreWhite = 0; _engineBusy = NO;
    memset(_hist, 0, sizeof(_hist));
    c4_new_game_randomize();  // re-seed Zobrist → TT misses → varied play
    c4_init_state_str(_st, "startpos");
    memset(&_sp, 0, sizeof(_sp));
    _sp.use_time = 1; _sp.time_ms = tms; _sp.max_depth = 99;
    _sp.use_qsearch = 0; _sp.tt_size_mb = 32; _sp.stop = &_stopEngine;
    [self setNeedsDisplay:YES];
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 80*NSEC_PER_MSEC),
                   dispatch_get_main_queue(), ^{ [self maybeRunEngine]; });
}

- (void)showSetupDialog:(BOOL)startup {
    _stopEngine = 1; _generation++;
    NSView *box = [[NSView alloc] initWithFrame:NSMakeRect(0,0,340,200)];
    float y = 175;
    NSTextField *(^lbl)(NSString*,float) = ^(NSString *t, float yy) {
        NSTextField *f = [NSTextField labelWithString:t];
        f.frame = NSMakeRect(0, yy, 340, 17);
        f.font  = [NSFont boldSystemFontOfSize:12];
        return f;
    };
    [box addSubview:lbl(@"Modalità", y)]; y -= 26;
    NSSegmentedControl *modeSC = [NSSegmentedControl
        segmentedControlWithLabels:@[@"  Uomo vs Computer  ",@"  Computer vs Computer  "]
        trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:nil];
    modeSC.frame = NSMakeRect(0, y, 340, 26); [modeSC setSelectedSegment:0];
    [box addSubview:modeSC]; y -= 36;
    [box addSubview:lbl(@"Il tuo colore", y)]; y -= 26;
    NSSegmentedControl *colorSC = [NSSegmentedControl
        segmentedControlWithLabels:@[@"  🟡  Giallo  ",@"  🔴  Rosso  "]
        trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:nil];
    colorSC.frame = NSMakeRect(0, y, 220, 26); [colorSC setSelectedSegment:0];
    [box addSubview:colorSC]; y -= 36;
    [box addSubview:lbl(@"Difficoltà motore", y)]; y -= 26;
    NSSegmentedControl *diffSC = [NSSegmentedControl
        segmentedControlWithLabels:@[@"  Lampo 0.5s  ",@"  Normale 2s  ",@"  Forte 5s  "]
        trackingMode:NSSegmentSwitchTrackingSelectOne target:nil action:nil];
    diffSC.frame = NSMakeRect(0, y, 340, 26); [diffSC setSelectedSegment:1];
    [box addSubview:diffSC];

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText     = startup ? @"Vibe Forza 4" : @"Nuova Partita";
    alert.informativeText = startup ? @"Benvenuto! Scegli le opzioni e premi Inizia."
                                    : @"Configura la nuova partita:";
    alert.accessoryView = box;
    [alert addButtonWithTitle:@"Inizia"];
    if (!startup) [alert addButtonWithTitle:@"Annulla"];
    NSModalResponse resp = [alert runModal];
    if (!startup && resp == NSAlertSecondButtonReturn) return;

    play_mode_t mode = ([modeSC selectedSegment] == 0) ? MODE_HVC : MODE_CVC;
    int hc   = ([colorSC selectedSegment] == 0) ? C4_YELLOW : C4_RED;
    int tms  = ([diffSC  selectedSegment] == 0) ? 500 :
               ([diffSC  selectedSegment] == 1) ? 2000 : 5000;
    [self resetGame:mode humanColor:hc timeMs:tms];
}
- (void)promptNewGame { [self showSetupDialog:NO]; }

// ----------------------------------------------------------------
// Drawing
// ----------------------------------------------------------------
- (void)drawRect:(NSRect)__unused dirty {
    const c4_board_t *b = c4_state_as_board(_st);

    // --- Background ---
    [[NSColor colorWithRed:0.07 green:0.09 blue:0.22 alpha:1] setFill];
    NSRectFill(self.bounds);

    // --- Board frame ---
    NSRect boardFrame = NSMakeRect(BRD_X - 6, BRD_Y - 6, BRD_W + 12, BRD_H + 12);
    NSBezierPath *frame = [NSBezierPath bezierPathWithRoundedRect:boardFrame
                                                          xRadius:10 yRadius:10];
    [[NSColor colorWithRed:0.15 green:0.22 blue:0.55 alpha:1] setFill]; [frame fill];

    // --- Hover column highlight ---
    if (_hoverCol >= 0 && _hoverCol < COLS) {
        NSRect hr = NSMakeRect(BRD_X + _hoverCol*CELL, BRD_Y, CELL, BRD_H);
        [[NSColor colorWithRed:1 green:1 blue:1 alpha:0.08] setFill];
        NSRectFill(hr);
    }

    // --- Cells ---
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            float cx = BRD_X + c*CELL + CELL/2;
            float cy = BRD_Y + r*CELL + CELL/2;
            float rad = CELL/2 - 6;
            NSRect cr = NSMakeRect(cx-rad, cy-rad, rad*2, rad*2);
            NSBezierPath *cell = [NSBezierPath bezierPathWithOvalInRect:cr];

            uint64_t bit = 1ULL << c4_sq(c, r);
            if      (b->bb[C4_YELLOW] & bit) [[NSColor colorWithRed:0.98 green:0.82 blue:0.10 alpha:1] setFill];
            else if (b->bb[C4_RED]    & bit) [[NSColor colorWithRed:0.90 green:0.15 blue:0.15 alpha:1] setFill];
            else                              [[NSColor colorWithRed:0.05 green:0.07 blue:0.18 alpha:1] setFill];
            [cell fill];

            // Last-move ring
            if (c == _lastCol && (b->bb[C4_YELLOW]|b->bb[C4_RED]) & bit) {
                [[NSColor colorWithRed:1 green:1 blue:1 alpha:0.6] setStroke];
                cell.lineWidth = 3.0; [cell stroke];
            }
        }
    }

    // --- Column letters at top ---
    NSDictionary *la = @{ NSFontAttributeName: [NSFont systemFontOfSize:12],
                          NSForegroundColorAttributeName: [NSColor colorWithWhite:0.7 alpha:1] };
    for (int c = 0; c < COLS; c++) {
        NSString *s = [NSString stringWithFormat:@"%c", 'a'+c];
        NSSize sz = [s sizeWithAttributes:la];
        [s drawAtPoint:NSMakePoint(BRD_X + c*CELL + CELL/2 - sz.width/2,
                                   BRD_Y + BRD_H + 8) withAttributes:la];
    }

    // --- Hover preview disc (top of hovered column) ---
    if (_hoverCol >= 0 && _hoverCol < COLS && b->height[_hoverCol] < ROWS
        && !_engineBusy) {
        int stm = b->side_to_move;
        float cx = BRD_X + _hoverCol*CELL + CELL/2;
        float cy = BRD_Y + BRD_H + 30;
        float rad = CELL/2 - 10;
        NSRect pr = NSMakeRect(cx-rad, cy-rad, rad*2, rad*2);
        NSBezierPath *pv = [NSBezierPath bezierPathWithOvalInRect:pr];
        NSColor *pc = (stm == C4_YELLOW)
            ? [NSColor colorWithRed:0.98 green:0.82 blue:0.10 alpha:0.7]
            : [NSColor colorWithRed:0.90 green:0.15 blue:0.15 alpha:0.7];
        [pc setFill]; [pv fill];
    }

    // --- Message bar ---
    NSDictionary *msg = @{ NSFontAttributeName: [NSFont systemFontOfSize:12],
                           NSForegroundColorAttributeName: [NSColor systemBlueColor] };
    [_message drawAtPoint:NSMakePoint(BRD_X, 14) withAttributes:msg];

    // --- Side panel ---
    [self drawPanel];
}

- (void)drawPanel {
    float x = PANEL_X, y = WIN_H - 28;
    NSColor *bright = [NSColor colorWithWhite:0.92 alpha:1];
    NSColor *mid    = [NSColor colorWithWhite:0.70 alpha:1];
    NSDictionary *titleA = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:15],
                              NSForegroundColorAttributeName: bright };
    NSDictionary *hdrA   = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:11],
                              NSForegroundColorAttributeName: bright };
    NSDictionary *bodyA  = @{ NSFontAttributeName: [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightRegular],
                              NSForegroundColorAttributeName: mid };
    NSDictionary *hiA    = @{ NSFontAttributeName: [NSFont monospacedSystemFontOfSize:11 weight:NSFontWeightMedium],
                              NSForegroundColorAttributeName: bright };

    [@"Vibe Forza 4" drawAtPoint:NSMakePoint(x, y) withAttributes:titleA]; y -= 22;

    const c4_board_t *b = c4_state_as_board(_st);
    int stm = b->side_to_move;
    NSString *turnS = (stm == C4_YELLOW) ? @"🟡 Giallo" : @"🔴 Rosso";
    NSString *modeS = (_mode == MODE_HVC) ? @"HvC" : @"CvC";
    NSString *busyS = _engineBusy ? @" ⏳" : @"";
    [[NSString stringWithFormat:@"%@ — Turno: %@%@", modeS, turnS, busyS]
     drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 22;

    // Eval bar
    [self drawEvalBar:x y:y width:WIN_W - PANEL_X - 18]; y -= 26;

    if (_sinfoValid) {
        int sc = (_sinfoSide == C4_YELLOW) ? _sinfoScore : -_sinfoScore;
        NSString *scS = (abs(sc) >= 29000)
            ? [NSString stringWithFormat:@"%sMatto", sc > 0 ? "+" : "-"]
            : [NSString stringWithFormat:@"%+.2f", sc/100.0];
        [[NSString stringWithFormat:@"Score: %@   Profondità: %d", scS, _sinfoDepth]
         drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 16;
        NSString *nodesS = (_sinfoNodes >= 1000000)
            ? [NSString stringWithFormat:@"%.2fM", _sinfoNodes/1e6]
            : (_sinfoNodes >= 1000)
            ? [NSString stringWithFormat:@"%.1fK", _sinfoNodes/1e3]
            : [NSString stringWithFormat:@"%llu", (unsigned long long)_sinfoNodes];
        NSString *npsS = (_sinfoNPS >= 1e6)
            ? [NSString stringWithFormat:@"%.1fM/s", _sinfoNPS/1e6]
            : [NSString stringWithFormat:@"%.0fK/s", _sinfoNPS/1e3];
        [[NSString stringWithFormat:@"Nodi: %@   NPS: %@", nodesS, npsS]
         drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 16;
        [[NSString stringWithFormat:@"Tempo: %.2fs", _sinfoTimeSec]
         drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 16;
        if (_sinfoPV[0]) {
            [[NSString stringWithFormat:@"PV: %@",
              [NSString stringWithUTF8String:_sinfoPV]]
             drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA]; y -= 16;
        }
        y -= 4;
    }

    [[NSColor separatorColor] setFill];
    NSRectFill(NSMakeRect(x, y, WIN_W-PANEL_X-18, 1)); y -= 18;
    [@"Mosse giocate" drawAtPoint:NSMakePoint(x, y) withAttributes:hdrA]; y -= 16;
    int startH = (_histN > 18) ? _histN - 18 : 0;
    for (int i = startH; i < _histN; i++) {
        NSString *ms = [NSString stringWithUTF8String:_hist[i]];
        [[NSString stringWithFormat:@"%2d. %@", i+1, ms]
         drawAtPoint:NSMakePoint(x, y)
         withAttributes:(i == _histN-1) ? hiA : bodyA]; y -= 15;
    }
    if (_histN == 0) {
        [@"(nessuna mossa)" drawAtPoint:NSMakePoint(x, y) withAttributes:bodyA];
    }

    NSDictionary *hintA = @{ NSFontAttributeName: [NSFont systemFontOfSize:10],
                             NSForegroundColorAttributeName: [NSColor colorWithWhite:0.45 alpha:1] };
    [@"⌘N Nuova Partita  ·  ⌘Z Annulla mossa"
     drawAtPoint:NSMakePoint(x, 10) withAttributes:hintA];
}

- (void)drawEvalBar:(float)x y:(float)y width:(float)w {
    float h = 20.0, sc = _scoreWhite / 100.0f;
    if (sc >  4.0f) sc =  4.0f;
    if (sc < -4.0f) sc = -4.0f;
    float wFrac = (sc + 4.0f) / 8.0f;
    [[NSColor colorWithRed:0.15 green:0.15 blue:0.15 alpha:1] setFill];
    NSRectFill(NSMakeRect(x, y, w, h));
    [[NSColor colorWithRed:0.98 green:0.82 blue:0.10 alpha:1] setFill];
    NSRectFill(NSMakeRect(x + w - wFrac*w, y, wFrac*w, h));
    NSString *lbl = (_scoreWhite >= 29000) ? @"+M" : (_scoreWhite <= -29000) ? @"−M"
        : [NSString stringWithFormat:@"%+.1f", _scoreWhite/100.0];
    NSDictionary *la = @{ NSFontAttributeName: [NSFont boldSystemFontOfSize:10],
                          NSForegroundColorAttributeName: [NSColor blackColor] };
    NSSize ls = [lbl sizeWithAttributes:la];
    [lbl drawAtPoint:NSMakePoint(x+w/2-ls.width/2, y+(h-ls.height)/2+1) withAttributes:la];
}

// ----------------------------------------------------------------
// Input
// ----------------------------------------------------------------
- (void)mouseMoved:(NSEvent *)ev {
    NSPoint pt = [self convertPoint:ev.locationInWindow fromView:nil];
    int col = [self colAt:pt];
    if (col != _hoverCol) { _hoverCol = col; [self setNeedsDisplay:YES]; }
}
- (void)mouseExited:(NSEvent *)__unused ev {
    _hoverCol = -1; [self setNeedsDisplay:YES];
}
- (void)updateTrackingAreas {
    [super updateTrackingAreas];
    for (NSTrackingArea *ta in self.trackingAreas) [self removeTrackingArea:ta];
    NSTrackingAreaOptions opts = NSTrackingMouseMoved | NSTrackingMouseEnteredAndExited
                               | NSTrackingActiveInKeyWindow;
    [self addTrackingArea:[[NSTrackingArea alloc] initWithRect:self.bounds
                             options:opts owner:self userInfo:nil]];
}

- (void)mouseDown:(NSEvent *)ev {
    if (_engineBusy) return;
    game_result_t gr = GAME_RESULT_NONE;
    if (_api->is_terminal(_st, &gr)) return;
    const c4_board_t *b = c4_state_as_board(_st);
    int stm = b->side_to_move;
    if (_mode == MODE_HVC && stm != _humanColor) return;
    NSPoint pt = [self convertPoint:ev.locationInWindow fromView:nil];
    int col = [self colAt:pt];
    if (col < 0 || b->height[col] >= C4_ROWS) return;
    [self applyMove:c4_move_make(col)];
}

- (void)keyDown:(NSEvent *)ev {
    (void)ev;
}

- (int)colAt:(NSPoint)pt {
    float rx = pt.x - BRD_X;
    if (rx < 0 || rx >= BRD_W || pt.y < BRD_Y - 10 || pt.y >= BRD_Y + BRD_H + 50) return -1;
    return (int)(rx / CELL);
}

// ----------------------------------------------------------------
// Game logic
// ----------------------------------------------------------------
- (void)applyMove:(game_move_t)mv {
    int stm = _api->side_to_move(_st);
    if (_undoTop < UNDO_MAX) {
        undo_entry_t *e = &_undo[_undoTop];
        e->mv = mv; e->hist_n = _histN; e->scoreWhite = _scoreWhite;
        _api->make_move(_st, mv, e->buf);
        _undoTop++;
    } else {
        void *tmp = calloc(1, _api->undo_size);
        _api->make_move(_st, mv, tmp); free(tmp);
    }
    _lastCol = c4_move_col(mv);
    char ms[4]; c4_move_to_str(mv, ms, sizeof(ms));
    if (_histN < UNDO_MAX) strncpy(_hist[_histN++], ms, 3);

    game_result_t post = GAME_RESULT_NONE;
    int terminal = _api->is_terminal(_st, &post);
    if (terminal && post == GAME_RESULT_LOSS)
        _message = [NSString stringWithFormat:@"Vince %@!",
                    (stm == C4_YELLOW) ? @"🟡 Giallo" : @"🔴 Rosso"];
    else if (terminal && post == GAME_RESULT_DRAW)
        _message = @"Pareggio — tabellone pieno!";
    else
        _message = [NSString stringWithFormat:@"Ultima mossa: colonna %s", ms];

    [self setNeedsDisplay:YES];
    if (!terminal) [self maybeRunEngine];
}

- (void)undoLastMove {
    if (_engineBusy) { _stopEngine = 1; _generation++; _engineBusy = NO; }
    if (_undoTop == 0) { _message = @"Nessuna mossa da annullare"; [self setNeedsDisplay:YES]; return; }
    int steps = (_mode == MODE_HVC && _undoTop >= 2) ? 2 : 1;
    for (int i = 0; i < steps && _undoTop > 0; i++) {
        undo_entry_t *e = &_undo[--_undoTop];
        _api->unmake_move(_st, e->mv, e->buf);
        _histN = e->hist_n; _scoreWhite = e->scoreWhite;
    }
    _lastCol = -1; _message = @"Mossa annullata";
    [self setNeedsDisplay:YES];
}

// ----------------------------------------------------------------
// Engine
// ----------------------------------------------------------------
- (void)maybeRunEngine {
    int stm = _api->side_to_move(_st);
    game_result_t gr = GAME_RESULT_NONE;
    if (_api->is_terminal(_st, &gr)) return;
    if (_mode == MODE_HVC && stm == _humanColor) return;
    if (_engineBusy) return;
    _engineBusy = YES; _stopEngine = 0;
    int gen = _generation;
    [self setNeedsDisplay:YES];
    memcpy(_eng_st, _st, _api->state_size);

    engine_job_t *job = calloc(1, sizeof(engine_job_t));
    job->api = _api; job->st = _eng_st; job->sp = _sp;
    job->completion = ^(search_result_t *out) {
        self->_engineBusy = NO;
        if (self->_generation != gen) { [self setNeedsDisplay:YES]; return; }
        if (!out->best_move) {
            // Fallback: pick a random legal move rather than leaving game stuck
            game_move_t lms[C4_COLS];
            int n = self->_api->generate_legal(self->_st, lms, C4_COLS);
            if (n > 0) {
                [self applyMove:lms[arc4random_uniform((uint32_t)n)]];
            } else {
                self->_message = @"Motore: nessuna mossa disponibile";
                [self setNeedsDisplay:YES];
            }
            return;
        }
        self->_scoreWhite   = (stm == C4_YELLOW) ? out->score : -out->score;
        self->_sinfoValid   = YES;
        self->_sinfoSide    = stm;
        self->_sinfoScore   = out->score;
        self->_sinfoDepth   = out->depth_searched;
        self->_sinfoNodes   = out->nodes;
        self->_sinfoNPS     = out->nps;
        self->_sinfoTimeSec = out->time_ns / 1e9;
        self->_sinfoPV[0] = '\0';
        int pvN = out->pv_len < 8 ? out->pv_len : 8;
        for (int i = 0; i < pvN; i++) {
            char ms[4]; c4_move_to_str(out->pv[i], ms, sizeof(ms));
            if (i) strncat(self->_sinfoPV, " ", sizeof(self->_sinfoPV)-strlen(self->_sinfoPV)-1);
            strncat(self->_sinfoPV, ms, sizeof(self->_sinfoPV)-strlen(self->_sinfoPV)-1);
        }
        [self applyMove:out->best_move];
    };

    pthread_attr_t attr; pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8u*1024u*1024u);
    pthread_t tid;
    pthread_create(&tid, &attr, engine_thread, job);
    pthread_attr_destroy(&attr); pthread_detach(tid);
}
@end

// ================================================================
// SplashView
// ================================================================
@interface SplashView : NSView
@property (copy) void (^onTap)(void);
@end
@implementation SplashView
- (void)drawRect:(NSRect)__unused dirty {
    NSRect b = self.bounds;
    float W = b.size.width, H = b.size.height;
    NSGradient *bg = [[NSGradient alloc] initWithColors:@[
        [NSColor colorWithRed:0.07 green:0.09 blue:0.22 alpha:1],
        [NSColor colorWithRed:0.02 green:0.02 blue:0.08 alpha:1]]];
    [bg drawInRect:b angle:130];

    // Mini board
    float SQ = 26.0, bw = SQ*COLS, bh = SQ*ROWS;
    float bx = (W-bw)/2, by = H-40-bh;
    [[NSColor colorWithRed:0.15 green:0.22 blue:0.55 alpha:1] setFill];
    [[NSBezierPath bezierPathWithRoundedRect:NSMakeRect(bx-4,by-4,bw+8,bh+8) xRadius:6 yRadius:6] fill];
    // Starting position — empty board with a few pieces for visual
    int preview[C4_COLS][C4_ROWS];
    memset(preview, 0, sizeof(preview));
    // Place a few pieces to make it look like a game in progress
    int yp[][3] = {{0,0,1},{1,0,1},{2,0,2},{3,0,1},{3,1,2},{4,0,2}};
    int rp[][3] = {{0,1,2},{1,1,2},{2,1,1},{3,2,1},{4,1,1}};
    for (int i=0;i<6;i++) preview[yp[i][0]][yp[i][1]]=yp[i][2];
    for (int i=0;i<5;i++) preview[rp[i][0]][rp[i][1]]=rp[i][2];
    for (int c=0;c<C4_COLS;c++) {
        for (int r=0;r<C4_ROWS;r++) {
            float cx=bx+c*SQ+SQ/2, cy=by+r*SQ+SQ/2, rad=SQ/2-3;
            NSColor *pc = preview[c][r]==1 ? [NSColor colorWithRed:0.98 green:0.82 blue:0.10 alpha:1]
                        : preview[c][r]==2 ? [NSColor colorWithRed:0.90 green:0.15 blue:0.15 alpha:1]
                        : [NSColor colorWithRed:0.05 green:0.07 blue:0.18 alpha:1];
            [pc setFill];
            [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(cx-rad,cy-rad,rad*2,rad*2)] fill];
        }
    }

    NSColor *gold = [NSColor colorWithRed:0.98 green:0.82 blue:0.10 alpha:1];
    NSDictionary *ta = @{ NSFontAttributeName:[NSFont boldSystemFontOfSize:36], NSForegroundColorAttributeName:gold };
    NSString *title = @"Vibe Forza 4";
    NSSize ts = [title sizeWithAttributes:ta];
    [title drawAtPoint:NSMakePoint((W-ts.width)/2, by-54) withAttributes:ta];

    NSColor *bright = [NSColor colorWithRed:0.80 green:0.80 blue:0.86 alpha:1];
    NSColor *dim    = [NSColor colorWithRed:0.50 green:0.50 blue:0.56 alpha:1];
    struct { NSString *t; float sz; NSColor *c; } lines[] = {
        { @"realizzato da sb – Haglard",                           13, bright },
        { @"between 2022 and 2026 · using ChatGPT and Claude",     11, bright },
        { @"with a swift acceleration in March 2026",              11, dim    },
        { @"not a single line of code is crafted by a human",      11, dim    },
    };
    float cy2 = by - 84;
    for (int i=0;i<4;i++) {
        NSDictionary *ca = @{ NSFontAttributeName:[NSFont systemFontOfSize:lines[i].sz],
                              NSForegroundColorAttributeName:lines[i].c };
        NSSize sz = [lines[i].t sizeWithAttributes:ca];
        [lines[i].t drawAtPoint:NSMakePoint((W-sz.width)/2, cy2) withAttributes:ca];
        cy2 -= lines[i].sz + 5;
    }
    NSDictionary *ta2 = @{ NSFontAttributeName:[NSFont systemFontOfSize:10],
                           NSForegroundColorAttributeName:[NSColor colorWithWhite:0.5 alpha:1]};
    NSString *hint = @"Clicca per continuare";
    NSSize hs = [hint sizeWithAttributes:ta2];
    [hint drawAtPoint:NSMakePoint((W-hs.width)/2, 14) withAttributes:ta2];
}
- (void)mouseDown:(NSEvent *)__unused ev { if (self.onTap) self.onTap(); }
@end

// ================================================================
// SplashController
// ================================================================
@interface SplashController : NSObject
- (void)showRelativeTo:(NSWindow *)parent completion:(void(^)(void))done;
- (void)dismiss;
@end
@implementation SplashController {
    NSPanel        *_panel;
    void           (^_done)(void);
    SplashController *_selfRetain; // keeps self alive until dismiss
}
- (void)showRelativeTo:(NSWindow *)parent completion:(void(^)(void))done {
    _done = done;
    _selfRetain = self;  // prevent ARC from deallocating before dismiss

    const CGFloat W=460, H=420;
    NSRect pf = parent.frame;
    NSRect fr = NSMakeRect(pf.origin.x+(pf.size.width-W)/2,
                            pf.origin.y+(pf.size.height-H)/2, W, H);
    _panel = [[NSPanel alloc] initWithContentRect:fr
        styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO];
    _panel.backgroundColor = [NSColor blackColor];
    _panel.level = NSFloatingWindowLevel;
    _panel.acceptsMouseMovedEvents = YES;

    SplashView *sv = [[SplashView alloc] initWithFrame:NSMakeRect(0,0,W,H)];
    sv.onTap = ^{ [self dismiss]; };   // strong capture: safe because _selfRetain holds self
    _panel.contentView = sv;
    [_panel makeKeyAndOrderFront:nil];

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,(int64_t)(60.0*NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{ [self dismiss]; });
}
- (void)dismiss {
    if (!_panel) return;
    [_panel orderOut:nil]; _panel = nil;
    void (^cb)(void) = _done; _done = nil;
    if (cb) cb();            // call before releasing self-retain
    _selfRetain = nil;       // release last strong ref — may dealloc self here
}
@end

// ================================================================
// AppDelegate
// ================================================================
@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) C4View   *gameView;
@end
@implementation AppDelegate
- (void)applicationDidFinishLaunching:(NSNotification *)__unused n {
    NSRect wr = NSMakeRect(120, 100, WIN_W, WIN_H);
    self.window = [[NSWindow alloc] initWithContentRect:wr
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|
                  NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskResizable
        backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"Vibe Forza 4";
    self.window.minSize = NSMakeSize(WIN_W, WIN_H);

    self.gameView = [[C4View alloc] initWithFrame:NSMakeRect(0,0,WIN_W,WIN_H)];
    self.window.contentView = self.gameView;
    [NSApp activateIgnoringOtherApps:YES];
    [self.window makeKeyAndOrderFront:nil];

    // Menu
    NSMenu *mb = [[NSMenu alloc] init];
    NSMenuItem *appItem = [[NSMenuItem alloc] init];
    [mb addItem:appItem];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"Vibe Forza 4"];
    [appMenu addItemWithTitle:@"Quit" action:@selector(terminate:) keyEquivalent:@"q"];
    appItem.submenu = appMenu;

    NSMenuItem *gameItem = [[NSMenuItem alloc] init];
    [mb addItem:gameItem];
    NSMenu *gameMenu = [[NSMenu alloc] initWithTitle:@"Partita"];
    NSMenuItem *newItem = [[NSMenuItem alloc] initWithTitle:@"Nuova Partita"
        action:@selector(newGame:) keyEquivalent:@"n"];
    newItem.target = self;
    [gameMenu addItem:newItem];
    NSMenuItem *undoItem = [[NSMenuItem alloc] initWithTitle:@"Annulla Mossa"
        action:@selector(undoMove:) keyEquivalent:@"z"];
    undoItem.target = self;
    [gameMenu addItem:undoItem];
    gameItem.submenu = gameMenu;
    [NSApp setMainMenu:mb];

    // Splash
    SplashController *sc = [[SplashController alloc] init];
    C4View *gv = self.gameView;
    [sc showRelativeTo:self.window completion:^{
        [gv showSetupDialog:YES];
    }];
}
- (void)newGame:(id)__unused s  { [self.gameView promptNewGame]; }
- (void)undoMove:(id)__unused s { [self.gameView undoLastMove]; }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)__unused a { return YES; }
@end

int main(void) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        app.activationPolicy = NSApplicationActivationPolicyRegular;
        AppDelegate *del = [[AppDelegate alloc] init];
        app.delegate = del;
        [app run];
    }
    return 0;
}
