/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * LD_PRELOAD shim: capture libxcb-imdkit's internal DebugLog stream, and log
 * every event fcitx5 feeds to its XIM filter.
 *
 * WHY THIS EXISTS
 * ---------------
 * dkit's handler functions -- _xcb_im_filter_xconnect_message,
 * _xcb_im_handle_connect, _xcb_im_handle_create_ic -- are NOT exported, so they
 * cannot be breakpointed by name in gdb: a pending breakpoint on them never
 * resolves and silently never hits. That trap is recorded in
 * docs/FCITX5-ADDON-NOTES.md §11c and cost a round of debugging.
 *
 * Two things ARE exported and were enough to see the whole handshake:
 *
 *   xcb_im_set_log_handler -- DebugLog is compiled in and gated only on
 *       im->logger != NULL, and fcitx5 never installs a logger. So dkit's own
 *       trace of every opcode ("-- XIM_CONNECT", "-- XIM_CREATE_IC", ...) is
 *       present but dead. Installing a handler turns it on.
 *
 *   xcb_im_filter_event -- exported, so it can be interposed to log events.
 *
 * Together these answer "did the client send it" versus "did the server
 * receive and reject it", which is the distinction the whole D3 diagnosis
 * turned on.
 *
 * Build:  cc -shared -fPIC -o dkit-logshim.so dkit-logshim.c
 * Use:    LD_PRELOAD=./dkit-logshim.so fcitx5
 *         (only when debugging; run-d3.sh never preloads this)
 */
#define _GNU_SOURCE
#include <elf.h>
#include <link.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void xcb_im_t;
typedef void (*log_handler_t)(const char *, ...);
typedef void (*set_log_t)(xcb_im_t *, log_handler_t);
typedef int (*filter_event_t)(xcb_im_t *, void *);

/* Core X protocol response types. ClientMessage is 33; an earlier version of
 * this file used 4 (ButtonPress), which made a "no ClientMessage arrived"
 * reading wrong -- the filter simply was not matching the events it saw.
 * Others seen in practice: 28 SelectionClear, 30 SelectionRequest,
 * 31 ColormapNotify, 89/90 XGE. */
#define XCB_CLIENT_MESSAGE_ 33

static void shim_logger(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  fprintf(stderr, "[dkit] ");
  vfprintf(stderr, fmt, ap);
  fputc('\n', stderr);
  va_end(ap);
  fflush(stderr);
}

/* Look a dynamic symbol up in an already-mapped ELF. Iterating the in-memory
 * section headers rather than re-reading the file keeps this correct if the
 * file on disk has since been replaced. */
static uintptr_t elf_symbol(uintptr_t base, const char *want) {
  const ElfW(Ehdr) *eh = (const ElfW(Ehdr) *)base;
  if (memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 ||
      eh->e_ident[EI_CLASS] != ELFCLASS64) {
    return 0;
  }
  const ElfW(Shdr) *sh = (const ElfW(Shdr) *)(base + eh->e_shoff);
  for (unsigned i = 0; i < eh->e_shnum; i++) {
    if (sh[i].sh_type != SHT_DYNSYM || sh[i].sh_link >= eh->e_shnum) {
      continue;
    }
    const ElfW(Sym) *sym = (const ElfW(Sym) *)(base + sh[i].sh_offset);
    size_t n = sh[i].sh_entsize ? sh[i].sh_size / sh[i].sh_entsize : 0;
    const char *str = (const char *)(base + sh[sh[i].sh_link].sh_offset);
    for (size_t k = 0; k < n; k++) {
      if (strcmp(str + sym[k].st_name, want) == 0) {
        return base + sym[k].st_value;
      }
    }
  }
  return 0;
}

static uintptr_t dkit_base;

static int find_dkit(struct dl_phdr_info *info, size_t size, void *data) {
  (void)size;
  (void)data;
  if (info->dlpi_name && strstr(info->dlpi_name, "libxcb-imdkit.so")) {
    dkit_base = (uintptr_t)info->dlpi_addr;
    return 1;
  }
  return 0;
}

/* Find dkit's load base.
 *
 * Four approaches were tried and measured; the reasons they fail are worth
 * keeping, because each looks reasonable:
 *
 *   dlopen("libxcb-imdkit.so.1") -- we are called from inside
 *       AddonManager::load -> dlopen(libxim), so the loader lock is already
 *       held and a recursive dlopen of dkit segfaults inside the real
 *       xcb_im_create.
 *   RTLD_DEFAULT -- libxim loads dkit with RTLD_LOCAL, so its symbols are not
 *       in the global scope and the lookup returns NULL.
 *   RTLD_NEXT -- the shim is preloaded but dkit is not yet in the link chain
 *       (libxim dlopens it later), so RTLD_NEXT finds nothing.
 *   /proc/self/maps -- the first executable mapping of a shared object is not
 *       at vaddr 0, so this yields the r-x segment address. Measured ~0x25000
 *       above the true base, and no dynsym entry resolved.
 *   dladdr(&our_own_symbol) -- returns THIS shim, since we are the definition
 *       fcitx5 actually calls.
 *
 * dl_iterate_phdr reports dlpi_addr, the load base by definition, and takes no
 * loader lock of its own. That is what works.
 */
static uintptr_t dkit_lookup(void) {
  if (!dkit_base) {
    dl_iterate_phdr(find_dkit, 0);
  }
  return dkit_base;
}

/* xcb_im_create's signature MUST be reproduced exactly: it takes 12 arguments.
 * An earlier version of this shim declared only (im, conn) and forwarded those
 * two; the real function then read %rcx, %r8 and stack slots as its remaining
 * arguments and segfaulted at xcb_im_create+0xa9. A preload shim that calls
 * through is a tail call, so every argument has to be forwarded. */
typedef xcb_im_t *(*create_t)(void *, int, uint32_t, const char *, const char *,
                              const void *, const void *, const void *,
                              const void *, uint32_t, void *, void *);

xcb_im_t *xcb_im_create(void *conn, int screen, uint32_t serverWindow,
                        const char *serverName, const char *locale,
                        const void *inputStyles, const void *onKeysList,
                        const void *offKeysList, const void *encodingList,
                        uint32_t event_mask, void *callback, void *user_data) {
  static create_t real_create;
  static set_log_t real_set_log;
  if (!real_create) {
    uintptr_t base = dkit_lookup();
    if (base) {
      real_create = (create_t)elf_symbol(base, "xcb_im_create");
      real_set_log = (set_log_t)elf_symbol(base, "xcb_im_set_log_handler");
    }
    fprintf(stderr, "[dkit-shim] dkit base=%p create=%p set_log=%p\n",
            (void *)base, (void *)real_create, (void *)real_set_log);
    fflush(stderr);
  }
  if (!real_create) {
    /* Never return NULL here: fcitx5 would die on the spot and take the
         * whole stack with it. Pass through untraced instead. */
    fprintf(stderr, "[dkit-shim] unresolved; returning NULL\n");
    fflush(stderr);
    return NULL;
  }
  xcb_im_t *ret = real_create(conn, screen, serverWindow, serverName, locale,
                              inputStyles, onKeysList, offKeysList,
                              encodingList, event_mask, callback, user_data);
  fprintf(stderr, "[dkit-shim] xcb_im_create -> %p\n", (void *)ret);
  fflush(stderr);
  if (ret && real_set_log) {
    real_set_log(ret, shim_logger);
    fprintf(stderr, "[dkit-shim] log handler installed\n");
    fflush(stderr);
  }
  return ret;
}

/* Log every event fcitx5 hands to dkit. This is what separates "the client
 * never sent XIM_CONNECT" from "it sent it and the server rejected it". */
int xcb_im_filter_event(xcb_im_t *im, void *event) {
  static filter_event_t real;
  if (!real) {
    uintptr_t base = dkit_lookup();
    real = base ? (filter_event_t)elf_symbol(base, "xcb_im_filter_event")
                : 0;
  }
  if (!real || !event) {
    return real ? real(im, event) : 0;
  }
  uint8_t *bytes = event;
  unsigned rt = bytes[0] & ~0x80u;
  if (rt == XCB_CLIENT_MESSAGE_) {
    /* xcb_client_message_event_t: type(1) pad(1) seq(2) window(4) type(4)
         * then 20 bytes of data. */
    uint32_t window, mtype;
    memcpy(&window, bytes + 4, 4);
    memcpy(&mtype, bytes + 8, 4);
    fprintf(stderr,
            "[dkit-ev] ClientMessage win=0x%x type=%u fmt=%d d0=0x%x\n",
            window, mtype, bytes[1],
            (unsigned)(bytes[12] | (bytes[13] << 8) |
                       (bytes[14] << 16) | ((unsigned)bytes[15] << 24)));
  } else {
    fprintf(stderr, "[dkit-ev] event type=%u\n", rt);
  }
  fflush(stderr);
  return real(im, event);
}