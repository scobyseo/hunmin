/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Minimal activity for Espresso tests.
 */
package org.hunmin.ime

import android.app.Activity
import android.os.Bundle

class TestActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
    }
}