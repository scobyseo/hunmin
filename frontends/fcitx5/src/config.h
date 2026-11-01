/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef FCITX5_HUNMIN_CONFIG_H_
#define FCITX5_HUNMIN_CONFIG_H_

#include <fcitx-config/configuration.h>
#include <fcitx-config/option.h>
#include <fcitx-utils/i18n.h>
#include <hunmin.h>

namespace fcitx {

/* Layout picker annotation.
 *
 * EnumAnnotation makes configtool build an EnumOptionWidget (a QComboBox)
 * instead of a text field. The list is populated from the core at runtime. */
struct LayoutAnnotation : public EnumAnnotation {
    void setLayouts(std::vector<std::pair<std::string, std::string>> layouts) { layouts_ = std::move(layouts); }
    void dumpDescription(RawConfig &config) const {
        EnumAnnotation::dumpDescription(config);
        for (size_t i = 0; i < layouts_.size(); i++) {
            config.setValueByPath("Enum/" + std::to_string(i), layouts_[i].first);
            config.setValueByPath("EnumI18n/" + std::to_string(i), layouts_[i].second);
        }
    }

private:
    std::vector<std::pair<std::string, std::string>> layouts_;
};

/* Preedit/commit unit picker (issue #128): one of the two policies the shared
 * layer implements, chosen per user instead of being fixed in the code.
 *
 *  word      commit the whole word at the boundary; the finished syllables stay
 *            visible as preedit until then (issue #74's shape)
 *  syllable  commit each syllable as the engine finalises it; the preedit is
 *            only the syllable in flight
 *
 * The TEXT is identical either way (docs/FRONTEND-EQUIVALENCE.md §1), which is
 * what makes this a safe preference rather than a behaviour fork. */
struct CommitBehaviorAnnotation : public EnumAnnotation {
    void dumpDescription(RawConfig &config) const {
        EnumAnnotation::dumpDescription(config);
        config.setValueByPath("Enum/0", "word");
        config.setValueByPath("EnumI18n/0", _("단어 단위"));
        config.setValueByPath("Enum/1", "syllable");
        config.setValueByPath("EnumI18n/1", _("음절 단위"));
    }
};

/* The automatic-switch row: `docs/SPEC-MODES.md` §R13-정정's surface B, issue
 * #176. The row must not spell either half of what it shows itself:
 *
 *   the VALUE  `hunmin_buffered_preedit_{get,set}_option(layer, "autoswitch", …)`
 *   the KEY    `hunmin_buffered_preedit_special_chord(layer, HUNMIN_SPECIAL_AUTOSWITCH)`
 *
 * so the adapter hands this annotation the chord number the engine reports and
 * whether the data the feature needs is installed. A changed assignment or a
 * missing package changes the row without an edit here; a literal typed into
 * the addon is exactly what would let the three frontends advertise three
 * different keys.
 *
 * "Disabled with a reason" is TEXT on this frontend, and that is a measured
 * limit rather than a taste: fcitx5's config protocol has no disabled state to
 * dump — `OptionBase` carries no enable flag (only the compile-time
 * `HiddenOption`/`ConditionalHidden`), and `libFcitx5Config` knows `Description`,
 * `Type` and `DefaultValue` only. So the row explains itself in its description
 * and its tooltip instead of greying a control out.
 */
struct AutoswitchAnnotation {
    /* HUNMIN_MOD_* as hunmin_buffered_preedit_special_chord() answers it. */
    void setChord(unsigned mods) { mods_ = mods; }
    /* hunmin_ctx_autoswitch_data_present(): the pair issue #169 ruled the row
     * draws from — value 0 WITH a clear value means "off by choice", value 0
     * with this false means "off because the data is missing". */
    void setDataPresent(bool present) { present_ = present; }

    bool skipDescription() const { return false; }
    bool skipSave() const { return false; }
    void dumpDescription(RawConfig &config) const {
        const std::string name = _("자동 한/영 전환");
        if (!present_) {
            config.setValueByPath("Description", name + _(" (사용할 수 없음: 사전 없음)"));
            config.setValueByPath("ToolTip", _("자동 전환에 쓰는 사전(modes/en-US.json)이 설치되어 있지 않아 켤 수 없습니다. 데이터를 설치한 뒤 다시 열어 주세요."));
            return;
        }
        const std::string chord = chordText(mods_);
        config.setValueByPath("Description", chord.empty() ? name : name + " (" + chord + _(" + 한/영") + ")");
        config.setValueByPath("ToolTip", _("한/영 키를 누르면 지금 언어가 바뀝니다. 이 기능은 그 위에서, 같은 키 조합으로도 켜고 끌 수 있습니다."));
    }

private:
    /* The modifier is the engine's; "한/영" is this platform's name for the
     * language key (fcitx5's Hangul keysym), and the frontend is the side that
     * knows what its own key is called (docs/FRONTEND-EQUIVALENCE.md §3). */
    static std::string chordText(unsigned mods) {
        std::string text;
        auto add = [&text](const char *part) {
            if (!text.empty()) {
                text += "+";
            }
            text += part;
        };
        if (mods & HUNMIN_MOD_CTRL) add("Ctrl");
        if (mods & HUNMIN_MOD_ALT) add("Alt");
        if (mods & HUNMIN_MOD_GUI) add("Super");
        return text;
    }

    unsigned mods_ = HUNMIN_MOD_CTRL;
    bool present_ = true;
};

FCITX_CONFIGURATION(HunminConfig,
                    /* Hangul layout picker: layouts with mode="hangul" (두벌식, 세벌식, etc.) */
                    OptionWithAnnotation<std::string, LayoutAnnotation> hangulLayout{this, "HangulLayout", _("한글 배열"), "sin3-p2"};
                    /* Latin layout picker: layouts with mode="latin" (QWERTY, DVORAK, COLEMAK) */
                    OptionWithAnnotation<std::string, LayoutAnnotation> latinLayout{this, "LatinLayout", _("영어 배열"), "us-qwerty"};
                    OptionWithAnnotation<std::string, CommitBehaviorAnnotation> commitBehavior{this, "CommitBehavior", _("커밋 단위"), "word"};
                    /* R13-정정's one on/off state (issue #176). The engine holds
                     * the value and the key; this item is the addon's store and
                     * the dialog's face for them, and the adapter writes it
                     * through `hunmin_buffered_preedit_set_option(…, "autoswitch", …)`
                     * so the engine's B3 data gate is the thing that decides
                     * whether ON can be held at all. OFF is the ruling of
                     * 2026-10-11 (B3) and therefore the default here too. */
                    OptionWithAnnotation<bool, AutoswitchAnnotation> autoswitch{this, "AutoSwitch", _("자동 한/영 전환"), false};);

} // namespace fcitx

#endif // FCITX5_HUNMIN_CONFIG_H_
