/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier-D3 client harness: a minimal XIM window that echoes every string the
 * input method commits to it as COMMIT:<utf8> on stdout. D3 cases are chosen
 * so every typed key is consumed by the engine (no pass-through), making the
 * COMMIT stream equal to the hunmin-sim commit stream (tier A).
 *
 * WHY THIS USES dkit AND NOT libX11's XOpenIM/XCreateIC
 * -----------------------------------------------------
 * The previous version of this file called XOpenIM()/XCreateIC() and linked
 * only X11::X11, so those resolved inside libX11's BUILT-IN XIM. fcitx5's XIM
 * server is libxcb-imdkit. Client and server were therefore different XIM
 * implementations for the whole of tier D3: no XIM_CONNECT, no CREATE_IC, no
 * input context (fcitx5-diagnose: "has 0 InputContext(s)"), and libX11's local
 * IM decoding the keys itself into the COMMIT lines the test compared against.
 * D3 was measuring nothing. See docs/D3-ROOT-CAUSE.md for the measurements.
 *
 * dkit exports the client half (xcb_xim_*), so this harness now speaks the
 * same implementation as the server.
 *
 * The dkit public headers are not installed on this host, so the small slice
 * of the API this file uses is declared here rather than vendored. Every
 * declaration below matches libxcb-imdkit 1.0.9's imclient.h/ximproto.h; the
 * struct member ORDER of xcb_xim_im_callback is part of the ABI and must not
 * be reordered.
 */
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <xkbcommon/xkbcommon.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------------------------------------------------------------- dkit API */

typedef struct _xcb_xim_t xcb_xim_t;
typedef uint16_t xcb_xic_t;

typedef void (*xcb_xim_set_event_mask_callback)(xcb_xim_t *, xcb_xic_t,
                                                uint32_t, uint32_t, void *);
typedef void (*xcb_xim_forward_event_callback)(xcb_xim_t *, xcb_xic_t,
                                               xcb_key_press_event_t *, void *);
typedef void (*xcb_xim_commit_string_callback)(xcb_xim_t *, xcb_xic_t,
                                               uint32_t, char *, uint32_t,
                                               uint32_t *, size_t, void *);
typedef void (*xcb_xim_geometry_callback)(xcb_xim_t *, xcb_xic_t, void *);
typedef void (*xcb_xim_preedit_start_callback)(xcb_xim_t *, xcb_xic_t, void *);
typedef void (*xcb_xim_preedit_draw_callback)(xcb_xim_t *, xcb_xic_t, void *,
                                             void *);
typedef void (*xcb_xim_preedit_caret_callback)(xcb_xim_t *, xcb_xic_t, void *,
                                               void *);
typedef void (*xcb_xim_preedit_done_callback)(xcb_xim_t *, xcb_xic_t, void *);
typedef void (*xcb_xim_status_start_callback)(xcb_xim_t *, xcb_xic_t, void *);
typedef void (*xcb_xim_status_draw_text_callback)(xcb_xim_t *, xcb_xic_t,
                                                  void *, void *);
typedef void (*xcb_xim_status_draw_bitmap_callback)(xcb_xim_t *, xcb_xic_t,
                                                    void *, void *);
typedef void (*xcb_xim_status_done_callback)(xcb_xim_t *, xcb_xic_t, void *);
typedef void (*xcb_xim_sync_callback)(xcb_xim_t *, xcb_xic_t, void *);
typedef void (*xcb_xim_disconnected_callback)(xcb_xim_t *, void *);

typedef struct _xcb_xim_im_callback {
    xcb_xim_set_event_mask_callback set_event_mask;
    xcb_xim_forward_event_callback forward_event;
    xcb_xim_commit_string_callback commit_string;
    xcb_xim_geometry_callback geometry;
    xcb_xim_preedit_start_callback preedit_start;
    xcb_xim_preedit_draw_callback preedit_draw;
    xcb_xim_preedit_caret_callback preedit_caret;
    xcb_xim_preedit_done_callback preedit_done;
    xcb_xim_status_start_callback status_start;
    xcb_xim_status_draw_text_callback status_draw_text;
    xcb_xim_status_draw_bitmap_callback status_draw_bitmap;
    xcb_xim_status_done_callback status_done;
    xcb_xim_sync_callback sync;
    xcb_xim_disconnected_callback disconnected;
} xcb_xim_im_callback;

typedef void (*xcb_xim_open_callback)(xcb_xim_t *, void *);
typedef void (*xcb_xim_create_ic_callback)(xcb_xim_t *, xcb_xic_t, void *);

xcb_xim_t *xcb_xim_create(xcb_connection_t *, int, const char *);
void xcb_xim_set_log_handler(xcb_xim_t *, void (*)(const char *, ...));
void xcb_xim_set_use_compound_text(xcb_xim_t *, bool);
void xcb_xim_set_use_utf8_string(xcb_xim_t *, bool);
bool xcb_xim_open(xcb_xim_t *, xcb_xim_open_callback, bool, void *);
void xcb_xim_set_im_callback(xcb_xim_t *, const xcb_xim_im_callback *, void *);
bool xcb_xim_filter_event(xcb_xim_t *, xcb_generic_event_t *);
bool xcb_xim_create_ic(xcb_xim_t *, xcb_xim_create_ic_callback, void *, ...);
bool xcb_xim_set_ic_focus(xcb_xim_t *, xcb_xic_t);
bool xcb_xim_unset_ic_focus(xcb_xim_t *, xcb_xic_t);
bool xcb_xim_forward_event(xcb_xim_t *, xcb_xic_t, xcb_key_press_event_t *);
void xcb_xim_close(xcb_xim_t *);
void xcb_xim_destroy(xcb_xim_t *);

/* Attribute names and input styles, from dkit's ximproto.h. */
#define XNInputStyle "inputStyle"
#define XNClientWindow "clientWindow"
#define XNFocusWindow "focusWindow"
#define IM_PreeditNothing 0x0008
#define IM_StatusNothing 0x0400

/* ------------------------------------------------------------------ state */

static xcb_xim_t *im;
static xcb_xic_t ic;
static xcb_window_t win;
static xcb_connection_t *conn;
static int screen_id;

/* ------------------------------------------------------------------ DEBUG */

static void logger(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[harness] ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    fflush(stderr);
}

/* ---------------------------------------------------------------- handlers */

/* The whole point of the test: this text comes from fcitx5, via libhunmin,
 * over the XIM protocol. Nothing here decodes a keysym. */
static void on_commit(xcb_xim_t *im_, xcb_xic_t ic_, uint32_t flag, char *str,
                      uint32_t length, uint32_t *keysym, size_t nKeySym,
                      void *user_data) {
    (void)im_;
    (void)ic_;
    (void)flag;
    (void)keysym;
    (void)nKeySym;
    (void)user_data;
    printf("COMMIT:%.*s\n", (int)length, str);
}

static void on_disconnected(xcb_xim_t *im_, void *user_data) {
    (void)im_;
    (void)user_data;
    fprintf(stderr, "harness: disconnected from the input method server\n");
    ic = 0;
}

/* ---------------------------------------------------------------- forward
 *
 * WHY THIS EXISTS. The input method decides, per key, whether it consumed it.
 * A key it did NOT consume must reach the application -- that is the whole
 * mechanism by which space, and every unmapped row, gets typed (FRONTEND-
 * CONTRACT.md 5 rule 3). Over XIM the server hands such a key back to the
 * client as XIM_FORWARD_EVENT; fcitx5's XIM frontend does exactly that
 * (fcitx5-5.1.19 src/frontend/xim/xim.cpp:426, forwardKeyImpl ->
 * xcb_im_forward_event).
 *
 * This harness set no forward_event callback, so dkit dropped every forwarded
 * key on the floor (dkit clientprotocolhandler.c:502 tests the callback for
 * NULL before calling it). The consequence was not cosmetic: a pass-through
 * key was invisible, so the D3 corpus had to EXCLUDE space -- cases.tsv says
 * so in as many words ("no pass-through keys ... so the XIM COMMIT stream
 * equals the sim commit stream"). The tier that is supposed to be the
 * full-stack one could not see the one key whose ownership is disputed
 * between the engine and the adapter.
 *
 * WHAT IS MODELLED, AND WHAT IS MEASURED. A real toolkit (GTK's XIM module,
 * Qt's) answers a forwarded key by running it through its own default input
 * context, which inserts the character. This harness does the same thing and
 * prints the character. So the line is NOT an assertion that hunmin committed
 * a space -- it is an assertion that the space was handed to the client, plus
 * the toolkit's documented half of the round trip performed locally. Where the
 * space becomes text on a real desktop is GTK/Qt's behaviour, not ours; this
 * is the last point at which it is still ours to observe.
 *
 * XIM_FORWARD_EVENT MEANS EXACTLY ONE THING HERE, and it took reading fcitx5 to
 * establish it, because the name suggests otherwise. There are two places in
 * fcitx5 5.1.19 that can send a key back to an XIM client:
 *   - src/frontend/xim/xim.cpp:664 -- `if (!result) xcb_im_forward_event(...)`,
 *     where result is `ic->keyEvent(event)`. The IM declined the key. This is
 *     the pass-through we care about.
 *   - src/lib/fcitx/instance.cpp:1672 -- the KeyEventOrderFix re-forward, which
 *     re-sends an ACCEPTED key so the client sees it after the commit.
 * The second cannot happen to this harness: it is gated on
 * `CapabilityFlag::KeyEventOrderFix`, and the XIM server only ever sets
 * ReportKeyRepeat, Preedit and FormattedPreedit (xim.cpp:270). So every forward
 * observed here is a decline, with no need to tell the two apart.
 *
 * RELEASES ARE NOT FORWARDS WORTH READING. tests/e2e/xtest-type.py sends a
 * press AND a release for every character, hunmin declines every release (it
 * only acts on presses), and so every key produced two forwards -- one real,
 * one noise. Measured: `jk1` produced exactly three forwards for three keys,
 * which is the release count, not the press count. Key releases are filtered
 * here for the same reason a toolkit ignores them: a release inserts nothing.
 * Without this filter the pass-through of a space is indistinguishable from the
 * pass-through of the release that follows every key. */
static struct xkb_context *xkb_ctx;
static struct xkb_keymap *xkb_keymap;
static struct xkb_state *xkb_state;

static void on_forward(xcb_xim_t *im_, xcb_xic_t ic_,
                       xcb_key_press_event_t *ev, void *user_data) {
    (void)im_;
    (void)ic_;
    (void)user_data;
    /* detail bit 7 is the send_event flag in XCB; the low 7 bits are the
     * keycode. fcitx5's forwardKeyImpl masks it when synthesising its own
     * events, but a forwarded event that crossed a real X boundary carries it. */
    uint8_t keycode = ev->detail & 0x7F;
    if (ev->response_type == XCB_KEY_RELEASE) {
        return; /* a release produces no character; see the comment above */
    }
    if (!xkb_state || !keycode) {
        /* A forwarded key with no keycode cannot be resolved to a character.
         * Say so rather than printing an empty FORWARD, which would compare
         * equal to a stream that never had one. */
        printf("FORWARD:<unresolved>\n");
        fflush(stdout);
        return;
    }
    xkb_state_update_mask(xkb_state, keycode, ev->state, 0, 0, 0, 0);
    xkb_keysym_t sym = xkb_state_key_get_one_sym(xkb_state, keycode);
    uint32_t cp = xkb_keysym_to_utf32(sym);
    /* A forwarded key that resolves to no character is not a character: the
     * shift and control presses that xtest-type.py sends around an uppercase
     * key are declined by the engine and come back here, and a toolkit inserts
     * nothing for them. Printing them produced a FORWARD with an empty payload,
     * which read as "something arrived but was invisible" -- measured as
     * `FORWARD:` immediately before `COMMIT:각` in sin3-p2 `kfcQ`, where the
     * key in question was Shift_L and not Q at all. */
    if (!cp) return;
    char utf8[8];
    int n = 0;
    if (cp) {
        /* Encode by hand: the point of this harness is to print exactly the
         * bytes the application would receive, and pulling in a conversion
         * library to do it would be more machinery than the measurement. */
        if (cp < 0x80) {
            utf8[n++] = (char)cp;
        } else if (cp < 0x800) {
            utf8[n++] = (char)(0xC0 | (cp >> 6));
            utf8[n++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            utf8[n++] = (char)(0xE0 | (cp >> 12));
            utf8[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            utf8[n++] = (char)(0x80 | (cp & 0x3F));
        } else {
            utf8[n++] = (char)(0xF0 | (cp >> 18));
            utf8[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            utf8[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            utf8[n++] = (char)(0x80 | (cp & 0x3F));
        }
    }
    printf("FORWARD:%.*s\n", n, utf8);
    fflush(stdout);
}

static const xcb_xim_im_callback callbacks = {
    .forward_event = on_forward,
    .commit_string = on_commit,
    .disconnected = on_disconnected,
};

static void on_ic_created(xcb_xim_t *im_, xcb_xic_t new_ic, void *user_data) {
    (void)user_data;
    ic = new_ic;
    if (!ic) {
        fprintf(stderr, "harness: server refused to create an input context\n");
        fflush(stderr);
        exit(79);
    }
    logger("input context created: id=%u", ic);
    xcb_xim_set_ic_focus(im_, ic);
    /* READY is printed only once the IC exists. The previous version printed
     * it after XCreateIC, which also succeeded via libX11's local IM and so
     * asserted readiness for a client that was never connected to anything. */
    printf("READY\n");
    fflush(stdout);
}

/* ---------------------------------------------------------------- case marker
 *
 * WHY A SIGNAL AND NOT A KEY. run-d3.sh reuses ONE harness process across every
 * case of a layout group, so it needs a way to say "the case just finished"
 * that cannot be confused with engine output. A synthetic keystroke would be
 * wrong twice over: fcitx5 might consume it (so no COMMIT line, and the case
 * looks truncated) or forward it (so a spurious FORWARD line the expectation
 * never asked for). Either way a reused harness would compare the previous
 * case's tail against the next case's head.
 *
 * SIGUSR1 carries no keystroke through the X server at all -- it is process to
 * process, so the engine never sees it. The handler only writes an async-signal
 * -safe, already-formatted marker; the case id arrives via a fixed buffer the
 * caller fills before signalling. */
/* stdin is NOT a control channel here and must never be read. The case id
 * travels through HUNMIN_CASE_ID_FILE, rewritten by run-d3.sh before each
 * SIGUSR1; poll() has only to fold the X connection and the self-pipe into one
 * wait. */
static const char *g_id_file;
static int g_sigpipe_write = -1;

static void on_case_marker(int sig) {
    (void)sig;
    /* Async-signal-safe: write a single byte to the self-pipe to wake the
     * poll loop. The poll loop will read the case id from the file and emit
     * the marker. */
    if (g_sigpipe_write >= 0) {
        char byte = 1;
        write(g_sigpipe_write, &byte, 1);
    }
}

static void on_open(xcb_xim_t *im_, void *user_data) {
    (void)user_data;
    logger("connected to the input method server; creating an input context");
    /* PreeditNothing | StatusNothing: this harness draws nothing, so it asks
     * the server not to send preedit/status callbacks it does not implement. */
    uint32_t input_style = IM_PreeditNothing | IM_StatusNothing;
    xcb_xim_create_ic(im_, on_ic_created, NULL, XNInputStyle, &input_style,
                      XNClientWindow, &win, XNFocusWindow, &win, NULL);
}

int main(int argc, char **argv) {
    /* stdout is a redirect (the test greps this stream for READY and COMMIT),
     * so it is fully buffered by default and READY would sit in the buffer
     * until exit. The run-d3.sh readiness gate polls the file, so it must be
     * unbuffered. The previous harness had this; it is easy to lose in a
     * rewrite and the symptom is a misleading "harness never became ready". */
    setbuf(stdout, NULL);

    /* Self-pipe for case markers: SIGUSR1 handler writes a byte here; the
     * poll loop reads it and emits the CASESTART marker. This avoids
     * async-signal-unsafe operations in the handler. */
    int sigpipe[2];
    if (pipe(sigpipe) < 0) {
        fprintf(stderr, "xim-harness: pipe failed\n");
        return 77;
    }
    /* Make the read end non-blocking so we can drain it in the poll loop. */
    fcntl(sigpipe[0], F_SETFL, O_NONBLOCK);
    fcntl(sigpipe[1], F_SETFL, O_NONBLOCK);

    struct sigaction sa = {0};
    sa.sa_handler = on_case_marker;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
    g_id_file = getenv("HUNMIN_CASE_ID_FILE");
    g_sigpipe_write = sigpipe[1];

    int screen = 0;
    conn = xcb_connect(NULL, &screen);
    if (!conn || xcb_connection_has_error(conn)) {
        return 77;
    }
    screen_id = screen;
    const xcb_setup_t *setup = xcb_get_setup(conn);
    xcb_screen_iterator_t it = xcb_setup_roots_iterator(setup);
    for (int i = 0; i < screen_id; i++) {
        xcb_screen_next(&it);
    }
    xcb_screen_t *scr = it.data;
    if (!scr) {
        fprintf(stderr, "xim-harness: no screen %d\n", screen_id);
        return 77;
    }

    /* The window must exist before the IC names it as the client window. */
    win = xcb_generate_id(conn);
    xcb_create_window(conn, XCB_COPY_FROM_PARENT, win, scr->root, 0, 0, 400,
                      200, 1, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      scr->root_visual, XCB_CW_BACK_PIXEL, &scr->white_pixel);
    uint32_t mask = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
                    XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_STRUCTURE_NOTIFY;
    xcb_change_window_attributes(conn, win, XCB_CW_EVENT_MASK, &mask);
    xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win, XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING, 8, 15, "hunmin-d3-harness");
    xcb_map_window(conn, win);
    xcb_flush(conn);

    const char *ximod = getenv("XMODIFIERS");
    fprintf(stderr, "DEBUG: XMODIFIERS=%s\n", ximod ? ximod : "(unset)");
    fflush(stderr);

    im = xcb_xim_create(conn, screen_id, NULL);
    if (!im) {
        fprintf(stderr, "xim-harness: xcb_xim_create failed\n");
        return 78;
    }
    /* The keymap is used only to turn a forwarded keycode into the character the
     * application would receive.
     *
     * It is compiled as us rather than read back from the server, and the
     * assumption is stated rather than implied because getting it wrong is
     * silent: `xkb_keymap_new_from_names(ctx, NULL, ...)` compiled a keymap
     * under which keycode 45 resolved to 'K' instead of 'k', so every forwarded
     * letter came back wrongly cased and the comparison against tier A would
     * have been arguing about a fiction.
     *
     * us is not a guess here. tests/e2e/xtest-type.py already encodes the same
     * assumption -- it resolves each character with XKeysymToKeycode and sends
     * an explicit Shift_L only for uppercase, which is exactly a us layout --
     * and both run against the Xvfb that tests/e2e/run-d3.sh starts. So the
     * harness and the injector agree by construction, and if that Xvfb is ever
     * given a different layout both halves must change together. The D3 case
     * that would catch it is any pass-through case: a FORWARD whose character
     * is not the character that was typed. */
    struct xkb_rule_names rn;
    memset(&rn, 0, sizeof rn);
    rn.rules = "evdev";
    rn.model = "pc105";
    rn.layout = "us";
    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (xkb_ctx) {
        xkb_keymap = xkb_keymap_new_from_names(xkb_ctx, &rn,
                                               XKB_KEYMAP_COMPILE_NO_FLAGS);
        if (xkb_keymap) xkb_state = xkb_state_new(xkb_keymap);
    }
    if (!xkb_state)
        fprintf(stderr, "harness: no keymap; forwarded keys will be "
                        "unresolved (FORWARD:<unresolved>)\n");

    xcb_xim_set_im_callback(im, &callbacks, NULL);
    xcb_xim_set_log_handler(im, logger);
    /* UTF-8 only: COMPIUND_TEXT would need a conversion round-trip, and the
     * contract is UTF-8 bytes on stdout. */
    xcb_xim_set_use_compound_text(im, false);
    xcb_xim_set_use_utf8_string(im, true);

    if (!xcb_xim_open(im, on_open, true, NULL)) {
        fprintf(stderr, "xim-harness: xcb_xim_open failed\n");
        return 78;
    }

    /* No window manager runs on the test display, so take the focus directly
     * and tell the server about it. */
    xcb_set_input_focus(conn, XCB_INPUT_FOCUS_PARENT, win, XCB_CURRENT_TIME);
    xcb_flush(conn);

    /* The X connection and the self-pipe are the only two things this loop
     * waits on. stdin is deliberately absent: it used to be a third entry,
     * whose own comment said no case-id traffic was expected on it and which
     * drained it anyway.
     *
     * That was not harmless. run-d3.sh fed its case list to the main loop on
     * stdin, this process inherited the same descriptor, and the drain ate
     * 256 bytes of cases.tsv per poll iteration until the file was at EOF and
     * the runner's `while read` loop stopped. Measured on 2026-10-10:
     * engine-d3-x11 reported Passed having run exactly ONE of its eight cases.
     * The same arm reads 0 from /dev/null, which breaks out of this loop, so
     * closing the runner's stdin instead kills the harness before READY --
     * which is how the cause was pinned down. Neither symptom had anything to
     * do with the engine.
     *
     * A harness has no business reading a stream it does not own, so the arm
     * is gone rather than guarded. run-d3.sh now also hands the harness
     * </dev/null and reads its own case list on fd 3. */
    int xfd = xcb_get_file_descriptor(conn);
    xcb_generic_event_t *ev = NULL;

    /* Give the XIM server a moment to process the open request before we
     * start polling. Without this the first poll iteration can spin before
     * the server's reply arrives (measured: removing this sleep makes the
     * harness miss the XIM_OPEN_REPLY and never print READY). */
    usleep(100000);

    for (;;) {
        struct pollfd pfds[2] = {
            {.fd = xfd, .events = POLLIN},
            {.fd = sigpipe[0], .events = POLLIN},
        };
        if (poll(pfds, 2, -1) < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (pfds[1].revents & POLLIN) {
            /* Case marker arrived via self-pipe. Read the case id from the
             * file and emit the marker. Drain the pipe first. */
            char sink[16];
            while (read(sigpipe[0], sink, sizeof sink) > 0) {}
            char id[128] = "";
            if (g_id_file) {
                int fd = open(g_id_file, O_RDONLY);
                if (fd >= 0) {
                    ssize_t n = read(fd, id, sizeof id - 1);
                    if (n > 0) {
                        id[n] = 0;
                        size_t m = strcspn(id, "\r\n");
                        id[m] = 0;
                    }
                    close(fd);
                }
            }
            char buf[160];
            int n = snprintf(buf, sizeof buf, "CASESTART:%s\n", id);
            if (n > 0) {
                write(STDOUT_FILENO, buf, (size_t)n);
            }
        }
        if (pfds[0].revents & (POLLIN | POLLHUP | POLLERR)) {
            while ((ev = xcb_poll_for_event(conn))) {
                uint8_t rt = ev->response_type & ~0x80;
                /* XIM protocol traffic first; it may consume it. */
                if (!xcb_xim_filter_event(im, ev) && ic) {
                    if (rt == XCB_KEY_PRESS) {
                        xcb_xim_forward_event(im, ic, (xcb_key_press_event_t *)ev);
                    } else if (rt == XCB_KEY_RELEASE) {
                        xcb_xim_forward_event(im, ic, (xcb_key_press_event_t *)ev);
                    } else if (rt == XCB_FOCUS_IN) {
                        xcb_xim_set_ic_focus(im, ic);
                    } else if (rt == XCB_FOCUS_OUT) {
                        xcb_xim_unset_ic_focus(im, ic);
                    }
                }
                free(ev);
                if (xcb_connection_has_error(conn)) {
                    break;
                }
            }
            if (pfds[0].revents & (POLLHUP | POLLERR)) break;
        }
    }

    xcb_xim_close(im);
    xcb_xim_destroy(im);
    xcb_disconnect(conn);
    return 0;
}