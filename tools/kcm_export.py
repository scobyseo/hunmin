#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Export Android KCM (Key Character Map) files from hunmin layout JSON.

Usage:
    python3 tools/kcm_export.py --data libhunmin/data --out frontends/keymaps/res/raw
    python3 tools/kcm_export.py --data libhunmin/data --out /tmp/kcm --layout dubeolsik
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# Android KEYCODE_* values (from tools/keymap-fixture.py, verified against android.jar API 35)
ANDROID_KEYCODES = {
    # Digits: KEYCODE_0..9 = 7..16
    **{str(d): 7 + d for d in range(10)},
    # Letters: KEYCODE_A..Z = 29..54
    **{chr(ord("a") + i): 29 + i for i in range(26)},
    # Punctuation (named keycodes)
    "comma": 55, "period": 56,
    "grave": 68, "minus": 69, "equals": 70, "left_bracket": 71,
    "right_bracket": 72, "backslash": 73, "semicolon": 74, "apostrophe": 75,
    "slash": 76,
    # Named keys
    "space": 62, "tab": 61, "enter": 66, "del": 67, "escape": 111,
}

# Layout key ID -> Android keycode name mapping
# This maps the US-QWERTY key IDs used in layout JSON to Android KEYCODE names
KEY_ID_TO_ANDROID = {
    # Letters
    **{chr(ord("a") + i): chr(ord("A") + i) for i in range(26)},
    # Digits
    **{str(d): str(d) for d in range(10)},
    # Punctuation
    "`": "GRAVE",
    "-": "MINUS",
    "=": "EQUALS",
    "[": "LEFT_BRACKET",
    "]": "RIGHT_BRACKET",
    "\\": "BACKSLASH",
    ";": "SEMICOLON",
    "'": "APOSTROPHE",
    ",": "COMMA",
    ".": "PERIOD",
    "/": "SLASH",
    # Special keys
    "space": "SPACE",
    "tab": "TAB",
    "enter": "ENTER",
    "backspace": "DEL",
    "escape": "ESCAPE",
}

# Android keycode name -> keycode value
ANDROID_KEYCODE_VALUES = {
    **{str(d): 7 + d for d in range(10)},
    **{chr(ord("A") + i): 29 + i for i in range(26)},
    "COMMA": 55, "PERIOD": 56,
    "GRAVE": 68, "MINUS": 69, "EQUALS": 70, "LEFT_BRACKET": 71,
    "RIGHT_BRACKET": 72, "BACKSLASH": 73, "SEMICOLON": 74, "APOSTROPHE": 75,
    "SLASH": 76,
    "SPACE": 62, "TAB": 61, "ENTER": 66, "DEL": 67, "ESCAPE": 111,
}


def resolve_key_value(value: str, layout_id: str) -> Optional[str]:
    """Resolve a layout key value to a Unicode character or None.
    
    Layout values can be:
    - "=X" literal character X
    - "X" jamo character (conjoining)
    - "noop" (skip)
    - Complex case expressions (for h3 engine)
    
    For KCM export, we only handle simple literal values (=X).
    """
    if isinstance(value, str):
        if value.startswith("="):
            # Literal character
            return value[1:]
        if value == "noop":
            return None
        # Jamo or other direct character - use as-is
        return value
    elif isinstance(value, dict):
        # Complex case expression - not directly expressible in KCM
        # We could try to extract a default, but for now skip
        return None
    return None


def extract_simple_mappings(layout: Dict) -> Dict[str, Tuple[Optional[str], Optional[str]]]:
    """Extract plain/shift mappings for keys that have simple literal values.
    
    Returns: {key_id: (plain_char, shift_char)}
    """
    mappings = {}
    keys = layout.get("keys", {})
    
    for key_id, key_def in keys.items():
        plain_val = key_def.get("plain")
        shift_val = key_def.get("shift")
        
        plain_char = resolve_key_value(plain_val, layout["id"]) if plain_val else None
        shift_char = resolve_key_value(shift_val, layout["id"]) if shift_val else None
        
        # Only include if at least one mapping is a simple literal
        if plain_char is not None or shift_char is not None:
            mappings[key_id] = (plain_char, shift_char)
    
    return mappings


def generate_kcm(layout: Dict, mappings: Dict[str, Tuple[Optional[str], Optional[str]]]) -> str:
    """Generate KCM file content for a layout."""
    layout_id = layout["id"]
    name_en = layout["name"].get("en", layout_id)
    name_ko = layout["name"].get("ko", layout_id)
    
    lines = [
        f"# KCM for {layout_id} ({name_en} / {name_ko})",
        f"# Generated from libhunmin/data/layouts/{layout_id}.json",
        f"# Layout engine: {layout.get('engine', 'unknown')}",
        "",
        "type OVERLAY",
        "",
    ]
    
    # Sort keys by Android keycode value for consistent output
    sorted_keys = sorted(
        mappings.items(),
        key=lambda kv: ANDROID_KEYCODE_VALUES.get(KEY_ID_TO_ANDROID.get(kv[0], ""), 999)
    )
    
    for key_id, (plain_char, shift_char) in sorted_keys:
        android_name = KEY_ID_TO_ANDROID.get(key_id)
        if not android_name:
            continue
        
        keycode = ANDROID_KEYCODE_VALUES.get(android_name)
        if keycode is None:
            continue
        
        # Determine label - use the key ID (uppercase for letters)
        if len(key_id) == 1 and key_id.isalpha():
            label = key_id.upper()
        elif key_id == "space":
            label = "SPACE"
        elif key_id == "tab":
            label = "TAB"
        elif key_id == "enter":
            label = "ENTER"
        elif key_id == "backspace":
            label = "DEL"
        elif key_id == "escape":
            label = "ESCAPE"
        else:
            label = key_id.upper()
        
        lines.append(f"key {android_name} {{")
        lines.append(f"    label: '{label}'")
        
        if plain_char is not None:
            # Escape special characters
            escaped = plain_char.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n").replace("\t", "\\t")
            lines.append(f"    base: '{escaped}'")
        
        if shift_char is not None:
            escaped = shift_char.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n").replace("\t", "\\t")
            lines.append(f"    shift: '{escaped}'")
        
        # For overlay keyboards, other modifiers typically fall through
        lines.append("    ctrl, alt, meta: none")
        lines.append("}")
        lines.append("")
    
    return "\n".join(lines)


def load_layout(layout_path: Path) -> Optional[Dict]:
    """Load and validate a layout JSON file."""
    try:
        with open(layout_path, "r", encoding="utf-8") as f:
            layout = json.load(f)
        
        # Validate required fields
        if "id" not in layout or "keys" not in layout:
            print(f"Warning: {layout_path.name} missing required fields, skipping", file=sys.stderr)
            return None
        
        return layout
    except json.JSONDecodeError as e:
        print(f"Error: {layout_path.name} invalid JSON: {e}", file=sys.stderr)
        return None
    except Exception as e:
        print(f"Error: {layout_path.name}: {e}", file=sys.stderr)
        return None


def main():
    parser = argparse.ArgumentParser(
        description="Export Android KCM files from hunmin layout JSON"
    )
    parser.add_argument(
        "--data", "-d",
        type=Path,
        default=Path("libhunmin/data"),
        help="Path to libhunmin/data directory (default: libhunmin/data)"
    )
    parser.add_argument(
        "--out", "-o",
        type=Path,
        required=True,
        help="Output directory for .kcm files"
    )
    parser.add_argument(
        "--layout", "-l",
        type=str,
        help="Export only a specific layout by ID"
    )
    parser.add_argument(
        "--list", action="store_true",
        help="List available layouts and exit"
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        help="Generate keyboard_layouts.xml manifest at this path"
    )
    
    args = parser.parse_args()
    
    layouts_dir = args.data / "layouts"
    if not layouts_dir.exists():
        print(f"Error: layouts directory not found: {layouts_dir}", file=sys.stderr)
        return 1
    
    layout_files = sorted(layouts_dir.glob("*.json"))
    
    if args.list:
        for lf in layout_files:
            layout = load_layout(lf)
            if layout:
                print(f"  {layout['id']}: {layout['name'].get('en', '')} ({layout.get('engine', 'unknown')})")
        return 0
    
    args.out.mkdir(parents=True, exist_ok=True)
    
    exported = []
    skipped = []
    
    for lf in layout_files:
        layout = load_layout(lf)
        if not layout:
            continue
        
        if args.layout and layout["id"] != args.layout:
            continue
        
        mappings = extract_simple_mappings(layout)
        
        if not mappings:
            skipped.append(layout["id"])
            continue
        
        kcm_content = generate_kcm(layout, mappings)
        # Android resource names must use underscores, not hyphens
        res_name = layout["id"].replace("-", "_")
        output_file = args.out / f"{res_name}.kcm"
        output_file.write_text(kcm_content, encoding="utf-8")
        exported.append((layout["id"], layout["name"].get("en", layout["id"]), res_name))
        print(f"Exported: {output_file}")
    
    if args.manifest:
        generate_manifest(exported, args.manifest)
    
    print(f"\nExported {len(exported)} KCM files to {args.out}")
    if skipped:
        print(f"Skipped (no simple mappings): {', '.join(skipped)}")
    
    return 0 if exported else 1


def generate_manifest(exported: List[Tuple[str, str, str]], manifest_path: Path):
    """Generate keyboard_layouts.xml manifest for ACTION_QUERY_KEYBOARD_LAYOUTS."""
    manifest_path.parent.mkdir(parents=True, exist_ok=True)

    lines = [
        '<?xml version="1.0" encoding="utf-8"?>',
        '<keyboard-layouts xmlns:android="http://schemas.android.com/apk/res/android">',
    ]

    for layout_id, name_en, res_name in exported:
        lines.append(
            f'    <keyboard-layout '
            f'android:name="keyboard_layout_{res_name}" '
            f'android:label="@string/keyboard_layout_{res_name}_label" '
            f'android:keyboardLayout="@raw/{res_name}" />'
        )

    lines.append('</keyboard-layouts>')

    manifest_path.write_text("\n".join(lines), encoding="utf-8")
    print(f"Generated manifest: {manifest_path}")


if __name__ == "__main__":
    sys.exit(main())