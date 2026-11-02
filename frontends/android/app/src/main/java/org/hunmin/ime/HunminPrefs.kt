/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * HunminPrefs — the one file and the keys shared by the IME service and the
 * settings screen.
 *
 * The automatic-switch value is the engine's (docs/SPEC-MODES.md §R13-정정), and
 * the service records only the read-back so the next editor's fresh layer can
 * be given the same answer. The system settings screen writes that same
 * read-back; a second literal here would be a second home for a value two
 * processes already have to agree about.
 */
package org.hunmin.ime

internal object HunminPrefs {
    const val FILE = "hunmin"

    /** The last read-back of the engine's `autoswitch` option. */
    const val AUTOSWITCH = "autoswitch"
}
