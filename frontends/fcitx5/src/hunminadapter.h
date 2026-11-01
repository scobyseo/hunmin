/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef FCITX5_HUNMIN_ADAPTER_H_
#define FCITX5_HUNMIN_ADAPTER_H_

#include "config.h"
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <hunmin.h>
#include <string>

namespace fcitx {

class HunminState;

class HunminAdapter : public InputMethodEngine {
public:
    explicit HunminAdapter(Instance *instance);

    void activate(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &) override;
    void deactivate(const fcitx::InputMethodEntry &entry, fcitx::InputContextEvent &event) override;
    void keyEvent(const fcitx::InputMethodEntry &entry, fcitx::KeyEvent &keyEvent) override;

    // Layout for a given IM: the profile's per-item `Layout=` wins over the
    // addon-wide `conf/hunmin.conf` default. fcitx5 exposes this through
    // InputMethodGroup::layoutFor() -- InputMethodEntry has no layout
    // accessor, and the per-item value is otherwise only written back to
    // profile without ever being read.
    std::string layoutFor(const fcitx::InputMethodEntry &entry) const;
    void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &) override;
    void reloadConfig() override;

    /* Fill the layout picker's list from the core. Called before the first
     * getConfig() (which is when the annotation is dumped) and again on every
     * reloadConfig(). */
    void refreshLayoutList();

    const fcitx::Configuration *getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig &rawConfig) override;

    /* The automatic-switch row (issue #176, docs/SPEC-MODES.md §R13-정정).
     *
     * One value, two surfaces: this pushes the stored value into every live
     * layer and keeps what the ENGINE holds, which is what the dialog shows.
     * The engine's B3 data gate can refuse ON, so the answer is taken from the
     * engine rather than assumed from the config file.
     */
    int pushAutoswitch(int want);
    /* A rebuilt layer was given the stored value; `held` is what the engine
     * accepted. The row must not claim more than that. */
    void recordAutoswitch(int held);
    /* The key the engine reports for the function, and whether the data the
     * feature needs is installed: the row's label and its reason. */
    void refreshAutoswitchRow();
    unsigned autoswitchChord() const { return autoswitchChord_; }
    bool autoswitchDataPresent() const { return autoswitchDataPresent_; }

    HunminState *state(InputContext *ic);

    hunmin_ctx *ctx() const { return ctx_; }
    auto &config() { return config_; }

private:
    Instance *instance_;
    HunminConfig config_;
    FactoryFor<HunminState> factory_;
    hunmin_ctx *ctx_ = nullptr;
    /* The engine's answers for the row, refreshed at load and on a config
     * reload; see refreshAutoswitchRow(). */
    unsigned autoswitchChord_ = HUNMIN_MOD_CTRL;
    bool autoswitchDataPresent_ = false;
};

class HunminAdapterFactory : public AddonFactory {
public:
    AddonInstance *create(AddonManager *manager) override { return new HunminAdapter(manager->instance()); }
};
} // namespace fcitx

#endif // FCITX5_HUNMIN_ADAPTER_H_
