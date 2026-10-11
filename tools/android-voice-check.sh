#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
#
# android-voice-check.sh — tier E4: the voice path, with a person in the loop.
#
# WHAT THIS IS, AND WHY IT IS NOT A GATE ON ACCURACY
# -------------------------------------------------
# The text comes from the platform recogniser (VoiceInput.kt picks the in-process
# SpeechRecognizer, with a fallback), and the input is live audio. Recorded audio
# cannot be replayed into SpeechRecognizer, so there is no deterministic way to
# assert "the words that came back were correct words" — a percentage threshold
# would be a test that fails on a bad network day and passes on a bad model day.
# Asking for one would mean replacing the recogniser in a test-only build, which
# measures something this APK does not ship.
#
# So the two halves are separated:
#
#   deterministic (this script, exit code)  — OUR contract: the key is found, a
#     tap reaches the voice path, the mic says LISTENING, the mic is PAINTED while
#     it listens, the session ends and the mic returns to idle, the result reaches
#     the editor, and nothing died.
#   recorded (the human answers)             — intelligibility: was the transcript
#     what was said. The script writes the row; the person marks it.
#
# The paint check is not decoration. "The state changed and the screen did not" is
# a real defect that shipped in the dock for a day: bind() was repainting an
# ImageView that was never added to the view tree, so every mic state change was
# invisible while the log said LISTENING. Nothing in tier E1 or E2 can see that.
#
# WHY IT IS NOT IN CTEST
# ----------------------
# It needs a person and a microphone, and a test that only passes when someone is
# at the keyboard is not a regression test. Tier E3 has the same property and is
# documented rather than registered; see docs/TESTING.md and
# tests/manual-checklists/tier-e4-voice.md.
#
# USAGE
#   tools/android-voice-check.sh [--device SERIAL] [--out FILE] [--only ID]
#
#   ANDROID_SERIAL is honoured when --device is not given. Before it starts, put
#   the cursor in any text field so the keyboard is up — the script checks that
#   and refuses to guess — then speak each phrase when its line appears. Non-
#   interactive runs (a cron, an agent) still gate every contract; the `heard`
#   column then reads `not-asked`, which is deliberately not a pass.

set -uo pipefail

ADB="${ADB:-adb}"
PKG="org.hunmin.ime"
IME="$PKG/.HunminInputMethodService"
OUT=""
ONLY=""
MIC_DESC="음성 입력"          # res/values/strings.xml voice_content_description
LISTENING_LOG="mic state -> LISTENING"
IDLE_LOG="mic state -> IDLE"
SESSION_TIMEOUT=15           # seconds to wait for the session to end
PAINT_TIMEOUT=3              # seconds to wait for LISTENING after the tap

failures=0
record_lines=()

usage() { sed -n '2,40p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

while [ $# -gt 0 ]; do
  case "$1" in
    --device) ADB="$ADB -s $2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --only) ONLY="$2"; shift 2 ;;
    -h|--help) usage 0 ;;
    *) echo "unknown option: $1" >&2; usage 2 ;;
  esac
done

fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
note() { echo "$*"; }

# The phrase list. Chosen for what OUR half does with the text, not for how well
# the model hears it: a phrase whose characters would compose differently if they
# reached the engine as keystrokes is worth speaking, because invariant 5 says they
# must not. See HunminEngineTest's `external text bypasses the jamo pipeline`.
#
# id | mode  | phrase            | what it is for
PHRASES=(
  "hangul-greeting|한글|안녕하세요|plain Hangul; the everyday case"
  "hangul-eojeol|한글|서울역에서 만나요|a phrase that would compose into something else as keystrokes"
  "hangul-short|한글|응|one syllable; guards against whitespace-only being taken as text"
  "latin-yes|영문|yes|Latin mode must ask the recogniser for en-US, not ko-KR"
  "silence|한글|(말하지 않음)|no speech: the session must still end and return to idle"
)

# --------------------------------------------------------------- environment

$ADB get-state >/dev/null 2>&1 || { echo "no device; pass --device or set ANDROID_SERIAL" >&2; exit 2; }
serial="$($ADB shell getprop ro.serialno 2>/dev/null | tr -d '\r')"
model="$($ADB shell getprop ro.product.model 2>/dev/null | tr -d '\r')"
note "device $serial ($model)"

current="$($ADB shell settings get secure default_input_method 2>/dev/null | tr -d '\r')"
if [ "$current" != "$IME" ]; then
  note "selecting $IME (was: ${current:-none})"
  $ADB shell ime enable "$IME" >/dev/null 2>&1
  $ADB shell ime set "$IME" >/dev/null 2>&1
  sleep 2
fi

# --------------------------------------------------------------- the editor

# The PERSON puts the cursor in a text field; this script only checks that the
# keyboard is up. That is a deliberate difference from tools/android-e2e.sh,
# which drives the Settings search bar itself: that surface is an AVD fixture,
# its widget differs per OEM, and a tier that needs someone at the keyboard to
# speak does not need a script guessing where they were typing. E2 owns the
# automated surface dance; E4 owns the conversation.
#
# It also means this script never force-stops anything of the user's, which is
# the right default on real hardware for a check that has to run on real hardware.

ime_is_up() { $ADB shell dumpsys input_method 2>/dev/null | grep -q 'mInputShown=true'; }

wait_for_ime() {
  if ime_is_up; then return 0; fi
  if [ -t 0 ]; then
    note ""
    note "텍스트를 입력할 수 있는 곳에 커서를 놓고, 키보드가 올라오면 Enter 를 누르세요."
    note "(Settings 의 검색창, 메모장, 브라우저 주소창 — 무엇이든 좋습니다.)"
    read -r _ || true
  fi
  for _ in $(seq 1 30); do
    ime_is_up && return 0
    sleep 1
  done
  return 1
}

wait_for_ime || {
  echo "cannot see a focused editor (mInputShown=false after 30s)." >&2
  echo "Put the cursor in a text field so the keyboard is up, then run again." >&2
  exit 2
}

mic_bounds() {
  $ADB shell uiautomator dump --windows /sdcard/vc2.xml >/dev/null 2>&1
  # The dump uses `package="org.hunmin.ime"` not `pkg=`.
  $ADB shell cat /sdcard/vc2.xml 2>/dev/null | tr '>' '\n' \
    | grep -E 'package="org.hunmin.ime"' \
    | grep -m1 -F "content-desc=\"$MIC_DESC\"" \
    | grep -o 'bounds="[^"]*"' | head -1
}

field_text() {
  # The editor's text, read back through the platform's own hierarchy rather than
  # from anything this app holds — the same discipline as the rest of the suite.
  $ADB shell uiautomator dump --windows /sdcard/vc3.xml >/dev/null 2>&1
  $ADB shell cat /sdcard/vc3.xml 2>/dev/null | tr '>' '\n' \
    | grep -m1 -E 'class="android.widget.EditText"|class="android.widget.AutoCompleteTextView"' \
    | grep -o 'text="[^"]*"' | head -1 | sed 's/^text="//; s/"$//'
}

# Is the mic key painted as the listening capsule? A pixel inside the key but
# outside the glyph, tested for greenness rather than for an exact colour, so it
# survives the light and dark palettes (Green 600 and Green 400).
mic_is_green() {
  command -v ffmpeg >/dev/null 2>&1 || { echo "SKIPPED"; return 2; }
  $ADB exec-out screencap -p > /tmp/hunmin-vc.png 2>/dev/null
  [ -s /tmp/hunmin-vc.png ] || return 1
  local x=$(( $1 - 34 )) y=$2 rgb
  rgb="$(ffmpeg -hide_banner -loglevel error -i /tmp/hunmin-vc.png \
    -vf "crop=1:1:$x:$y" -f rawvideo -pix_fmt rgb24 - 2>/dev/null | od -An -tu1 | tr -s ' ')"
  [ -n "$rgb" ] || return 1
  # shellcheck disable=SC2086
  set -- $rgb
  [ $# -ge 3 ] || return 1
  [ $(( $2 - $1 )) -gt 15 ] && [ $(( $2 - $3 )) -gt 15 ]
}

# --------------------------------------------------------------- the loop

note ""
note "Each line prompts, then waits. Speak when the line appears."
note ""

for row in "${PHRASES[@]}"; do
  IFS='|' read -r id mode phrase purpose <<<"$row"
  [ -n "$ONLY" ] && [ "$ONLY" != "$id" ] && continue

  note "── $id  [$mode]  $purpose"
  note "   발음: $phrase"
  [ "$phrase" = "(말하지 않음)" ] && note "   (아무 말도 하지 마세요)"

  # Clear the editor. 60 DELs is not a clever way to do this; it is the only one
  # that does not depend on a selection API this script has no business using.
  # (The field may not be this script's focus either — it is whatever the person
  # focused. If they focused something that ignores synthetic keys, the committed
  # text column comes back empty and the D contract fails loudly rather than
  # quietly scoring a phrase that was never spoken.)
  $ADB shell input keyevent KEYCODE_MOVE_END >/dev/null 2>&1
  $ADB shell input keyevent $(printf '67 %.0s' $(seq 1 60)) >/dev/null 2>&1

  bounds="$(mic_bounds || true)"
  [ -n "$bounds" ] || { fail "$id: no mic key (content-desc '$MIC_DESC') in the IME view"; continue; }
  # shellcheck disable=SC2086
  set -- $(printf '%s' "$bounds" | sed 's/[^0-9]/ /g' | tr -s ' ')
  [ $# -eq 4 ] || { fail "$id: cannot parse mic bounds '$bounds'"; continue; }
  mx=$(( ($1 + $3) / 2 )); my=$(( ($2 + $4) / 2 ))

  $ADB logcat -c >/dev/null 2>&1
  $ADB shell input tap "$mx" "$my" >/dev/null 2>&1

  # Contract A: the state machine says LISTENING.
  got_listening=0
  for _ in $(seq 1 $(( PAINT_TIMEOUT * 10 )) ); do
    $ADB logcat -d -s HunminIme:I 2>/dev/null | grep -qF "$LISTENING_LOG" && { got_listening=1; break; }
    sleep 0.1
  done
  [ "$got_listening" = 1 ] || fail "$id: the mic never reported LISTENING after a tap"

  # Contract B: the screen agrees. Sampled inside the 5-second session, which is
  # why this is a screencap and not a uiautomator dump — a dump takes longer than
  # the session lasts and reports the state after it has already ended.
  paint="SKIPPED"
  if [ "$got_listening" = 1 ]; then
    sleep 0.4
    if mic_is_green "$mx" "$my"; then paint="PAINTED"
    else
      paint="NOT PAINTED"
      if command -v ffmpeg >/dev/null 2>&1; then
        fail "$id: LISTENING but the mic key is not painted as the listening capsule"
      fi
    fi
  fi
  note "   state=$([ "$got_listening" = 1 ] && echo LISTENING || echo none)  paint=$paint"

  # Contract C: the session ends by itself and the key returns to idle.
  got_idle=0
  for _ in $(seq 1 $(( SESSION_TIMEOUT * 4 )) ); do
    $ADB logcat -d -s HunminIme:I 2>/dev/null | grep -qF "$IDLE_LOG" && { got_idle=1; break; }
    sleep 0.25
  done
  [ "$got_idle" = 1 ] || fail "$id: the session never returned to IDLE within ${SESSION_TIMEOUT}s"

  # Contract D: something reached the editor, and it is text.
  sleep 1
  text="$(field_text || true)"
  note "   커밋됨: ${text:-<nothing>}"
  if [ "$id" = "silence" ]; then
    [ -z "$text" ] || fail "$id: silence produced '$text' in the editor"
  else
    [ -n "$text" ] || fail "$id: the recogniser's text never reached the editor"
    case "$text" in
      *�*) fail "$id: the committed text contains U+FFFD: $text" ;;
    esac
  fi

  # The half that is not ours to assert: a person says whether it was heard.
  # No terminal means no answer, and "no answer" is recorded as no answer —
  # `not-asked` is deliberately not a pass, so a non-interactive run cannot be
  # quoted as an intelligibility result.
  if [ -t 0 ]; then
    printf '   들린 내용 맞나요? [y/N] '
    read -r verdict || verdict=""
  else
    verdict=""
  fi
  case "$verdict" in
    y|Y) heard="understood" ;;
    *)   heard="not-asked" ;;
  esac
  note "   heard=$heard"
  record_lines+=("$id|$mode|$phrase|$text|$heard")

  # Contract E: nothing died.
  $ADB shell pidof "$PKG" >/dev/null 2>&1 || fail "$id: the IME process is gone"
  note ""
done

# --------------------------------------------------------------- the record

report="${OUT:-/tmp/hunmin-voice-check-$(date +%Y%m%d-%H%M%S).txt}"
{
  echo "# tier E4 voice check"
  echo "date    $(date -Is)"
  echo "device  $model ($serial)"
  echo "ime     $current"
  echo
  printf '%-18s %-4s %-26s %-22s %s\n' id mode phrase committed heard
  for line in "${record_lines[@]}"; do
    IFS='|' read -r id mode phrase text heard <<<"$line"
    printf '%-18s %-4s %-26s %-22s %s\n' "$id" "$mode" "$phrase" "$text" "$heard"
  done
} > "$report"
note "record: $report"

if [ "$failures" -gt 0 ]; then
  note ""
  note "FAIL: $failures contract failure(s). The record above is what to attach to the issue."
  exit 1
fi
note ""
note "PASS: every contract held. The 'heard' column is the human's judgement and is not a gate."