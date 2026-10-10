// SPDX-License-Identifier: LGPL-2.1-or-later
package org.hunmin.keymaps

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.res.XmlResourceParser
import android.hardware.input.InputManager
import android.util.Log
import org.xmlpull.v1.XmlPullParserException
import java.io.IOException

/**
 * Broadcast receiver for ACTION_QUERY_KEYBOARD_LAYOUTS.
 * 
 * This minimal receiver reads the keyboard_layouts.xml resource and returns
 * the keyboard layout descriptors to the InputManagerService.
 */
class KeyboardLayoutReceiver : BroadcastReceiver() {

    private companion object {
        const val TAG = "HunminKeymaps"
    }

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != InputManager.ACTION_QUERY_KEYBOARD_LAYOUTS) {
            return
        }

        Log.d(TAG, "Received QUERY_KEYBOARD_LAYOUTS broadcast")

        val layouts = parseKeyboardLayouts(context)
        Log.d(TAG, "Parsed ${layouts.size} keyboard layouts")
        
        val resultData = layouts.joinToString(",") { it.descriptor }
        Log.d(TAG, "Returning result: $resultData")
        setResultData(resultData)
    }

    private fun parseKeyboardLayouts(context: Context): List<KeyboardLayoutInfo> {
        val layouts = mutableListOf<KeyboardLayoutInfo>()
        var parser: XmlResourceParser? = null
        
        try {
            parser = context.resources.getXml(R.xml.keyboard_layouts)
            var eventType = parser.eventType
            while (eventType != XmlResourceParser.END_DOCUMENT) {
                if (eventType == XmlResourceParser.START_TAG && parser.name == "keyboard-layout") {
                    // Use namespace-aware attribute lookup
                    val name = parser.getAttributeValue(
                        "http://schemas.android.com/apk/res/android", "name"
                    )
                    val labelRes = parser.getAttributeResourceValue(
                        "http://schemas.android.com/apk/res/android", "label", 0
                    )
                    val keyboardLayoutRes = parser.getAttributeResourceValue(
                        "http://schemas.android.com/apk/res/android", "keyboardLayout", 0
                    )
                    val locale = parser.getAttributeValue(
                        "http://schemas.android.com/apk/res/android", "keyboardLocale"
                    )
                    val layoutType = parser.getAttributeValue(
                        "http://schemas.android.com/apk/res/android", "keyboardLayoutType"
                    )
                    
                    Log.d(TAG, "Parsing keyboard-layout: name=$name, labelRes=$labelRes, keyboardLayoutRes=$keyboardLayoutRes")
                    
                    if (name != null && keyboardLayoutRes != 0) {
                        val label = if (labelRes != 0) {
                            context.getString(labelRes)
                        } else {
                            name
                        }
                        Log.d(TAG, "Found layout: name=$name, label=$label, res=$keyboardLayoutRes")
                        layouts.add(KeyboardLayoutInfo(
                            descriptor = name,
                            label = label,
                            keyboardLayoutRes = keyboardLayoutRes,
                            locale = locale ?: "",
                            layoutType = layoutType ?: ""
                        ))
                    }
                }
                eventType = parser.next()
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing keyboard layouts", e)
        } finally {
            parser?.close()
        }
        
        return layouts
    }

    private data class KeyboardLayoutInfo(
        val descriptor: String,
        val label: String,
        val keyboardLayoutRes: Int,
        val locale: String,
        val layoutType: String
    )
}