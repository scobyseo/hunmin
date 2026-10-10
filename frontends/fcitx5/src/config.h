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

/* Layout picker.
 *
 * A plain Option<std::string> renders as a free-text field, so the user had to
 * know the layout id ("sin3-p2") and type it exactly -- wrong ids fail later,
 * at keystroke time, with only a log line. EnumAnnotation is the annotation
 * fcitx5 provides for exactly this: option.h:115-127 describes it as "those
 * type of string that should shown as a combobox, but the value is run time
 * based", and it makes configtool build an EnumOptionWidget (a QComboBox)
 * instead of a text field -- fcitx5-configtool's optionwidget.cpp:546 branches
 * on type()=="String" plus the IsEnum property this dumps. classicui's
 * ThemeAnnotation is the upstream example of the pattern.
 *
 * The list itself is not written here. Layout names live in the layout JSON
 * as {"en":..., "ko":...} (SPEC-SCHEMA.md, checked by tools/doctor.py) and come
 * from the core through hunmin_ctx_layout_{count,id,name}(), so adding a layout
 * adds a row without touching this file. That is the "layouts are data, never
 * code" rule holding at the UI boundary too.
 *
 * OptionWithAnnotation is <T, NoConstrain<T>, DefaultMarshaller<T>,
 * Annotation> (option.h:467), so the annotation is the only thing customising
 * the option. */
struct LayoutAnnotation : public EnumAnnotation {
    void setLayouts(std::vector<std::pair<std::string, std::string>> layouts) {
        layouts_ = std::move(layouts);
    }
    void dumpDescription(RawConfig &config) const {
        EnumAnnotation::dumpDescription(config);
        for (size_t i = 0; i < layouts_.size(); i++) {
            config.setValueByPath("Enum/" + std::to_string(i), layouts_[i].first);
            // Same text in both columns: the id is already the ko name in the
            // data's own terms, and a half-translated picker is worse than an
            // untranslated one. If a layout ever needs a real English label
            // the field is here for it.
            config.setValueByPath("EnumI18n/" + std::to_string(i), layouts_[i].second);
        }
    }

private:
    std::vector<std::pair<std::string, std::string>> layouts_;
};

FCITX_CONFIGURATION(
    HunminConfig,
    OptionWithAnnotation<std::string, LayoutAnnotation>
        layout{this, "Layout", _("Layout"), "sin3-p2"};
    /* R2's English mode: the Latin layout the R8 adapter replays a word onto
     * when composition ends (src/modeswitch.h). Default us-qwerty, which
     * exists as data; dvorak is not in the repo yet. Not a claim about what
     * English mode should be -- it is the layout the judgement replays onto. */
    Option<std::string> latinLayout{this, "LatinLayout",
                                    _("Latin layout for R8 judgement"),
                                    "us-qwerty"};);

} // namespace fcitx

#endif // FCITX5_HUNMIN_CONFIG_H_
