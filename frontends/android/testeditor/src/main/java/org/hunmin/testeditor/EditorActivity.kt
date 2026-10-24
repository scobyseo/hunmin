// SPDX-License-Identifier: LGPL-2.1-or-later

package org.hunmin.testeditor

import android.app.Activity
import android.os.Bundle

class EditorActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_editor)
    }
}
