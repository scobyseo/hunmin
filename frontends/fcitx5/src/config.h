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

namespace fcitx {

/* Layout picker annotation.
 *
 * EnumAnnotation makes configtool build an EnumOptionWidget (a QComboBox)
 * instead of a text field. The list is populated from the core at runtime. */
struct LayoutAnnotation : public EnumAnnotation {
    void setLayouts(std::vector<std::pair<std::string, std::string>> layouts) {
        layouts_ = std::move(layouts);
    }
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

FCITX_CONFIGURATION(
    HunminConfig,
    /* Hangul layout picker: layouts with mode="hangul" (두벌식, 세벌식, etc.) */
    OptionWithAnnotation<std::string, LayoutAnnotation>
        hangulLayout{this, "HangulLayout", _("한글 배열"), "sin3-p2"};
    /* Latin layout picker: layouts with mode="latin" (QWERTY, DVORAK, COLEMAK) */
    OptionWithAnnotation<std::string, LayoutAnnotation>
        latinLayout{this, "LatinLayout", _("영어 배열"), "us-qwerty"};);

} // namespace fcitx

#endif // FCITX5_HUNMIN_CONFIG_H_
