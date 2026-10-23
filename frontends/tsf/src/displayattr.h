/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * displayattr.h - the composition display-attribute provider and the
 * preedit styling channel (S7). The shell forwards its two provider
 * methods here and hands the styling call to DisplayAttr::Apply; nothing
 * else in the shell knows the GUID atom or the underline shape.
 */
#ifndef HUNMIN_TSF_DISPLAYATTR_H_
#define HUNMIN_TSF_DISPLAYATTR_H_

#include <windows.h>
#include <msctf.h>

/* The styling channel: registers the attribute GUID with the process's
 * category manager once, then sets or clears the property over the given
 * range. Owns the TfGuidAtom; one instance per shell, because the atom is
 * per-process and the shell lives one per thread. */
class DisplayAttr {
  public:
    DisplayAttr();
    ~DisplayAttr();
    DisplayAttr(const DisplayAttr &) = delete;
    DisplayAttr &operator=(const DisplayAttr &) = delete;

    void Apply(ITfContext *ctx, TfEditCookie ec, ITfRange *range, bool clear);

  private:
    TfGuidAtom atom_;
};

/* The provider's answer, as com objects the caller owns. The enum yields
 * the one attribute; NewForGUID returns it only for HunminDisplayAttrGuid
 * (nullptr otherwise, so the shell can keep returning E_FAIL). */
IEnumTfDisplayAttributeInfo *HunminDisplayAttrEnumNew();
ITfDisplayAttributeInfo *HunminDisplayAttrNewForGUID(REFGUID guid);

#endif /* HUNMIN_TSF_DISPLAYATTR_H_ */
