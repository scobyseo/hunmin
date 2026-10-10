/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * PermissionRequestActivity — the transparent window an IME uses to ask for a
 * runtime permission it cannot ask for itself.
 *
 * WHY THIS EXISTS, measured not inherited. `requestPermissions` and
 * `onRequestPermissionsResult` are methods on `Activity`; they are absent from
 * `InputMethodService`, which reaches permission APIs only through
 * `Service -> ContextWrapper -> Context`. Re-measured for this change with javap
 * on android-35/android.jar (API 35, 2026-10-09):
 *
 *   $ javap -cp android.jar android.app.Activity | grep -c requestPermissions
 *   2
 *   $ javap -cp android.jar android.content.Context | grep -c requestPermissions
 *   0
 *   $ javap -cp android.jar android.inputmethodservice.InputMethodService \
 *       | grep -ci permission
 *   0
 *
 * So the app needs a second component whose only job is to exist for the length
 * of the platform dialog. That is the whole design; there is no second activity
 * and no helper class.
 *
 * WHY IT CANNOT BE SEEN. The manifest entry carries four of the five properties
 * (theme, excludeFromRecents, noHistory, exported=false); the fifth is in
 * onCreate, which finishes immediately when the permission is already held.
 * Without that last one the window would flash on every tap after the first
 * grant, because `requestPermissions` on an already-held permission completes
 * synchronously and this activity would otherwise sit there for a frame.
 *
 * WHY IT HAS NO RESULT CHANNEL BACK. An activity started from a Service is not
 * started for result, and B5 records that an IME could not receive one anyway.
 * So the answer is read back by whoever asked — `Context.checkSelfPermission`,
 * which is authoritative — and never by a flag written here. That is why this
 * class writes nothing, and why `HunminTrace`'s sink stays the only thing in the
 * app that writes a file.
 *
 * ⚠️ WHAT NO TEST HERE CAN REACH, stated so no reader mistakes silence for a
 * pass. The dialog is a platform window: `adb shell pm grant`/`pm revoke` put
 * the permission into either state and tier E2 can then assert which branch the
 * IME takes, but the dialog itself, the tap inside it, the app-op background
 * state, and the "stopped asking" state the platform enters after two refusals
 * are the framework's and cannot be driven or observed from a test. See
 * docs/FRONTEND-CONTRACT.md B5.
 */
package org.hunmin.ime

import android.Manifest
import android.app.Activity
import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.util.Log

class PermissionRequestActivity : Activity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (canRecord(this)) {
            finish()
            return
        }
        try {
            requestPermissions(arrayOf(Manifest.permission.RECORD_AUDIO), REQUEST_CODE)
        } catch (e: Exception) {
            // Raised from a click listener on the IME's own view, so an
            // exception here has the same footing as a keystroke handler: it
            // can take the keyboard down. A prompt that cannot be shown is not
            // worth that.
            Log.w(TAG, "requestPermissions failed; finishing", e)
            finish()
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray,
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        // The answer goes nowhere on purpose; see the header. Finish as soon as
        // the platform is done with us.
        if (requestCode == REQUEST_CODE) finish()
    }

    companion object {
        private const val TAG = "HunminPerm"

        /**
         * Opaque to every caller, and deliberately so.
         *
         * A request code is a channel between an activity and whoever started
         * it *for result*. There is no such caller here, so any code that keyed
         * on it would be waiting for a delivery that never arrives.
         */
        private const val REQUEST_CODE = 0x48A0

        /** The authoritative answer. Re-read by the IME on the next mic tap. */
        fun canRecord(context: Context): Boolean = VoiceLauncher.hasPermission(context)

        /**
         * Whether this build declares the activity at all.
         *
         * This is a packaging check, and the ceiling on it should be stated
         * plainly: it can only tell us the component is missing from the
         * merged manifest. It cannot tell us whether the platform will still
         * *show* a dialog, because after two refusals Android stops showing one
         * and `requestPermissions` returns denied immediately without any
         * signal reaching us. `shouldShowRequestPermissionRationale` would
         * distinguish those two, and it is an `Activity` method, so this class
         * could report it — except there is no channel back (see the header).
         * Recorded as a known blind spot rather than worked around.
         */
        fun canRaise(context: Context): Boolean = try {
            Intent(context, PermissionRequestActivity::class.java)
                .resolveActivity(context.packageManager) != null
        } catch (e: Exception) {
            Log.w(TAG, "cannot resolve the permission activity", e)
            false
        }

        /**
         * Put the platform's own prompt in front of the user.
         *
         * Returns whether it was raised. `false` means this build could not ask
         * at all, which the caller reports as "not granted, and nobody was
         * asked" — never as a refusal, which is a different thing and has a
         * different VoiceResult.
         *
         * `FLAG_ACTIVITY_NEW_TASK` is mandatory, not decoration: a Service is
         * not in a task and `startActivity` throws without it.
         */
        fun raise(context: Context): Boolean = try {
            context.startActivity(
                Intent(context, PermissionRequestActivity::class.java)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
            )
            true
        } catch (e: Exception) {
            Log.w(TAG, "cannot raise the permission prompt", e)
            false
        }
    }
}
