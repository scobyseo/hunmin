/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * The GUIDs more than one component names: the service CLSID (the shell and
 * the tier-F1 langbar shape test), the language-profile GUID (registration
 * enables it and unregistration removes it, issue #84) and the display
 * attribute the composition text carries (issue #98). One definition in
 * ids.cpp so the three cannot drift.
 */
#ifndef HUNMIN_TSF_IDS_H_
#define HUNMIN_TSF_IDS_H_

#include <guiddef.h>

extern const CLSID CLSID_HunminTSF;
extern const GUID HunminProfileGuid;
extern const GUID HunminDisplayAttrGuid;

#endif /* HUNMIN_TSF_IDS_H_ */
