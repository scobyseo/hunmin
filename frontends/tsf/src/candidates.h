/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The F9/configured-key Hanja candidate flow (issue #67). The shell's
 * read seam (CandBackend) and the flow interface (Candidates) live in
 * tsf.h - the shell owns the abstraction (DIP); this header declares the
 * one implementation and its factory, nothing of CTextService.
 */
#ifndef HUNMIN_TSF_CANDIDATES_H_
#define HUNMIN_TSF_CANDIDATES_H_

#include "candwindow.h"
#include "tsf.h"

/* Build the flow: one page constant (kMaxCandidatePageSize, the value
 * fcitx5's label ceiling forces, ruled for issue #45), the lookup state,
 * the open/close/motion handlers and the commit that replaces the anchor.
 * The caller (the shell) owns the instance. */
Candidates *hunmin_tsf_hanja_candidates_new(CandBackend &backend);

#endif /* HUNMIN_TSF_CANDIDATES_H_ */
