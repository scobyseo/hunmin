/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef FCITX5_HUNMIN_ENGINE_H_
#define FCITX5_HUNMIN_ENGINE_H_

#include "config.h"
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <hunmin.h>
#include <string>

namespace fcitx {

class HunminState;

class HunminEngine : public InputMethodEngine {
  public:
    explicit HunminEngine(Instance *instance);

    void activate(const fcitx::InputMethodEntry &,
                  fcitx::InputContextEvent &) override;
    void deactivate(const fcitx::InputMethodEntry &entry,
                    fcitx::InputContextEvent &event) override;
    void keyEvent(const fcitx::InputMethodEntry &entry,
                  fcitx::KeyEvent &keyEvent) override;

    // Layout for a given IM: the profile's per-item `Layout=` wins over the
    // addon-wide `conf/hunmin.conf` default. fcitx5 exposes this through
    // InputMethodGroup::layoutFor() -- InputMethodEntry has no layout
    // accessor, and the per-item value is otherwise only written back to
    // profile without ever being read.
    std::string layoutFor(const fcitx::InputMethodEntry &entry) const;
    void reset(const fcitx::InputMethodEntry &,
               fcitx::InputContextEvent &) override;
    void reloadConfig() override;

    /* Fill the layout picker's list from the core. Called before the first
     * getConfig() (which is when the annotation is dumped) and again on every
     * reloadConfig(). */
    void refreshLayoutList();

    const fcitx::Configuration *getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig &rawConfig) override;

    HunminState *state(InputContext *ic);

    hunmin_ctx *ctx() const { return ctx_; }
    auto &config() { return config_; }

  private:
    Instance *instance_;
    HunminConfig config_;
    FactoryFor<HunminState> factory_;
    hunmin_ctx *ctx_ = nullptr;
};

class HunminEngineFactory : public AddonFactory {
  public:
    AddonInstance *create(AddonManager *manager) override {
        return new HunminEngine(manager->instance());
    }
};
} // namespace fcitx

#endif // FCITX5_HUNMIN_ENGINE_H_
