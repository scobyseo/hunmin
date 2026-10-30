/*
 * SPDX-FileCopyrightText: 2026 hunmin contributors
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Tier F1, the registration fragment (issue #107): DllRegisterServer and
 * DllUnregisterServer driven over the seam in src/registrationseam.h, with the
 * doubles from mock-tsf-registration.h recording what the flow actually does.
 *
 * What this asserts is the ADAPTER's call order and parameters - the CLSID,
 * LANGID 0x0412, the profile GUID, the three category GUIDs, the registry paths
 * and values. It does NOT assert what the real msctf servers
 * write to the registry: that stays the elevated-cycle measurement in
 * tests/manual-checklists/tsf-unregister-cleanup.md (TSF17), and nothing here
 * closes or downgrades it. It also asserts what the flow does NOT do: no file
 * is written, since the Start-menu entry belongs to the installer (ruling of
 * 2026-10-30, issue #146).
 *
 * Standalone TU: the groups share the one `tsf-test` registration (issue #109's
 * merged shape), selected by --gtest_filter where a caller wants one group.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>
#include <windows.h>

#include "../src/ids.h"
#include "../src/registration.h"
#include "../src/registrationseam.h"
#include "mock-tsf-registration.h"

namespace {

/* GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT {25504FB4-7BAB-4BC1-9C69-CF81890F0EF5}
 * ("TIP supports inclusion in the System Tray", Predefined Category Values).
 * A literal here on purpose: msctf.h declares the symbol and the data lives in
 * a library the TIP does not link, so registration.cpp carries its own private
 * constant -- the test must not share a symbol with the code it grades. */
const GUID kGuidSystraySupport = {0x25504fb4, 0x7bab, 0x4bc1, {0x9c, 0x69, 0xcf, 0x81, 0x89, 0x0f, 0x0e, 0xf5}};

/* The CLSID as the registration helpers stringify it (StringFromGUID2), so the
 * expected registry paths are derived, never typed. */
std::wstring ClsidString() {
    wchar_t buf[64] = L"";
    StringFromGUID2(CLSID_HunminTSF, buf, 64);
    return std::wstring(buf);
}

std::wstring ClsidKey() { return L"HKCR|CLSID\\" + ClsidString(); }
/* The recorder keys a handle by the path the ADAPTER passed, so the
 * subkey is its own full path, not ClsidKey() + a separator. */
std::wstring InprocKey() { return ClsidKey() + L"\\InprocServer32"; }
std::wstring TipKey() { return L"HKLM|SOFTWARE\\Microsoft\\CTF\\TIP\\" + ClsidString(); }

bool Contains(const std::vector<std::wstring> &v, const std::wstring &needle) {
    for (const std::wstring &s : v)
        if (s == needle)
            return true;
    return false;
}

bool ContainsSubstring(const std::vector<std::wstring> &v, const std::wstring &needle) {
    for (const std::wstring &s : v)
        if (s.find(needle) != std::wstring::npos)
            return true;
    return false;
}

} // namespace

/* The register flow, artifact by artifact: the two registry trees first, then
 * the COM calls in the order docs/WINDOWS-TSF-REGISTRATION.md describes, then
 * the Start-menu entry.
 *
 * Deliberately NOT asserted as "one big expected sequence": the register flow
 * also opens two HKLM keys to pin the icon index, and pinning the whole list
 * would make every future step a test edit. What is pinned is the ORDER of the
 * steps the design names (each later one after the earlier one) plus each
 * step's parameters. */
TEST(TsfRegistration, RegisterWritesTheDocumentedArtifactSet) {
    tsfmock::RegistrationDoubles d;
    d.Install();

    ASSERT_EQ(DllRegisterServer(), TRUE) << "the flow reports success";

    /* 1. HKCR\CLSID\{CLSID}\InprocServer32 = <module>, ThreadingModel=Apartment. */
    EXPECT_TRUE(Contains(d.trace.created, ClsidKey())) << "the COM CLSID key is created";
    EXPECT_TRUE(Contains(d.trace.created, InprocKey())) << "InprocServer32 under it";
    EXPECT_TRUE(ContainsSubstring(d.trace.writes, InprocKey() + L"|(default)|")) << "the module path lands in the default value";
    EXPECT_TRUE(Contains(d.trace.writes, InprocKey() + L"|ThreadingModel|Apartment"));

    /* 2. HKLM\SOFTWARE\Microsoft\CTF\TIP\{CLSID} = "Hunmin", written BEFORE any
     * CTF COM call (so a host that never got as far as the profile manager
     * still sees the TIP). */
    EXPECT_TRUE(Contains(d.trace.created, TipKey())) << "the HKLM TIP key is created";
    EXPECT_TRUE(Contains(d.trace.writes, TipKey() + L"|(default)|Hunmin")) << "the TIP name";
    EXPECT_LT(d.trace.indexOf(L"RegCreateKey"), d.trace.indexOf(L"Register")) << "the trees are written before the COM calls";

    /* 3. The profile: Register -> AddLanguageProfile -> EnableLanguageProfile
     * -> ActivateProfile -> GetActiveProfile, on CLSID + 0x0412 + profile. */
    EXPECT_EQ(d.trace.count(L"Register"), 1);
    EXPECT_TRUE(IsEqualGUID(d.profiles.registeredClsid, CLSID_HunminTSF));
    EXPECT_EQ(d.trace.count(L"AddLanguageProfile"), 1);
    EXPECT_EQ(d.profiles.addedLangid, (LANGID)0x0412) << "the Korean locale";
    EXPECT_TRUE(IsEqualGUID(d.profiles.addedProfile, HunminProfileGuid));
    EXPECT_EQ(d.profiles.addedDescription, std::wstring(L"Hunmin")) << "the name Windows Settings lists";
    EXPECT_FALSE(d.profiles.addedIconFile.empty()) << "the module path is the icon source";
    EXPECT_EQ(d.trace.count(L"EnableLanguageProfile"), 1);
    EXPECT_EQ(d.profiles.enabled, TRUE) << "the profile is registered ENABLED (the shipping model)";
    EXPECT_EQ(d.trace.count(L"ActivateProfile"), 1);
    EXPECT_EQ(d.profileMgr.activateFlags, (DWORD)(TF_IPPMF_FORSYSTEMALL | TF_IPPMF_ENABLEPROFILE))
        << "the flag pair measured in docs/WINDOWS-TSF-REGISTRATION.md";
    EXPECT_TRUE(IsEqualGUID(d.profileMgr.activateClsid, CLSID_HunminTSF));
    EXPECT_TRUE(IsEqualGUID(d.profileMgr.activateProfile, HunminProfileGuid));
    EXPECT_EQ(d.profileMgr.activateLangid, (LANGID)0x0412);
    EXPECT_EQ(d.trace.count(L"GetActiveProfile"), 1) << "the evidence-bearing read that decides the fallback";

    /* The registration order the doc states, as an order and not a list. */
    EXPECT_LT(d.trace.indexOf(L"Register"), d.trace.indexOf(L"AddLanguageProfile"));
    EXPECT_LT(d.trace.indexOf(L"AddLanguageProfile"), d.trace.indexOf(L"EnableLanguageProfile"));
    EXPECT_LT(d.trace.indexOf(L"EnableLanguageProfile"), d.trace.indexOf(L"ActivateProfile"));
    EXPECT_LT(d.trace.indexOf(L"ActivateProfile"), d.trace.indexOf(L"GetActiveProfile"));

    /* 4. The four categories, in this order: keyboard, TIP, display-attribute
     * provider (issue #98's provider category), and the tray capability
     * (measured 2026-10-29, issue #68: without GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT
     * the shell gives the TIP no entry of its own in the Win11 input indicator;
     * the sibling 날개셋 IME and the Microsoft Korean IME both carry it). The
     * value is a literal here because msctf.h declares the symbol and the data
     * lives in a library registration.cpp does not link. */
    ASSERT_EQ(d.categoryMgr.registered.size(), (size_t)4) << "four categories, not the doc's two";
    EXPECT_TRUE(IsEqualGUID(d.categoryMgr.registered[0], GUID_TFCAT_TIP_KEYBOARD));
    EXPECT_TRUE(IsEqualGUID(d.categoryMgr.registered[1], GUID_TFCAT_CATEGORY_OF_TIP));
    EXPECT_TRUE(IsEqualGUID(d.categoryMgr.registered[2], GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER));
    EXPECT_TRUE(IsEqualGUID(d.categoryMgr.registered[3], kGuidSystraySupport)) << "GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT, the tray entry";

    /* 5. The icon-index pin (measured 2026-10-18: a stored IconIndex of 1
     * rendered the language entry blank, so registration rewrites 0). Both the
     * 0x0412 key and the profile key are opened and set. */
    EXPECT_EQ(d.trace.count(L"RegOpenKey"), 2) << "the pin opens the language-profile key and the profile key";
    EXPECT_TRUE(ContainsSubstring(d.trace.writes, L"\\LanguageProfile\\0x00000412|IconIndex|0"));

    /* 6. No file written, nothing deleted. Since the ruling of 2026-10-30
     * (issue #146) the Start-menu entry belongs to the installer alone; see
     * RegistrationWritesNoFile for the guard that keeps it that way. */
    EXPECT_TRUE(d.trace.deletes.empty()) << "registration deletes nothing";
}

/* The mirror: every registered artifact is unregistered, in the documented
 * order, and the trees go last (AddLanguageProfile created subkeys under the
 * HKLM tree, so the COM calls have to run first). */
TEST(TsfRegistration, UnregisterMirrorsEveryRegisteredArtifact) {
    tsfmock::RegistrationDoubles d;
    d.Install();
    ASSERT_EQ(DllRegisterServer(), TRUE);

    const std::vector<GUID> registered = d.categoryMgr.registered;
    const size_t mark = d.trace.events.size();

    ASSERT_EQ(DllUnregisterServer(), TRUE);

    /* The unregister tail, exactly: the profile's own enablement entry, then
     * Unregister, then the four category mirrors, then the two trees. */
    std::vector<std::wstring> tail(d.trace.events.begin() + (std::ptrdiff_t)mark, d.trace.events.end());
    const std::vector<std::wstring> expected = {L"UnregisterProfile",  L"Unregister",    L"UnregisterCategory", L"UnregisterCategory",
                                                L"UnregisterCategory", L"UnregisterCategory", L"RegDeleteTree", L"RegDeleteTree"};
    ASSERT_EQ(tail.size(), expected.size()) << "no unregister step is missing and none is extra";
    for (size_t i = 0; i < expected.size(); ++i)
        EXPECT_EQ(tail[i], expected[i]) << "unregister step " << i;

    /* Parameters mirror the register side. */
    EXPECT_TRUE(IsEqualGUID(d.profileMgr.unregisterProfileClsid, CLSID_HunminTSF));
    EXPECT_TRUE(IsEqualGUID(d.profileMgr.unregisterProfileGuid, HunminProfileGuid));
    EXPECT_EQ(d.profileMgr.unregisterProfileFlags, (DWORD)0) << "no flags: the profile itself, not a session form";
    EXPECT_TRUE(IsEqualGUID(d.profiles.unregisteredClsid, CLSID_HunminTSF));

    EXPECT_EQ(d.categoryMgr.unregistered.size(), registered.size()) << "one mirror per registered category";
    for (size_t i = 0; i < registered.size() && i < d.categoryMgr.unregistered.size(); ++i)
        EXPECT_TRUE(IsEqualGUID(d.categoryMgr.unregistered[i], registered[i])) << "category " << i << " mirrors its registration";

    /* Both trees, HKLM first. */
    ASSERT_EQ(d.trace.deletes.size(), (size_t)2);
    EXPECT_EQ(d.trace.deletes[0], TipKey()) << "the HKLM TIP tree (AddLanguageProfile wrote under it)";
    EXPECT_EQ(d.trace.deletes[1], ClsidKey()) << "then the COM CLSID tree";

    /* Recorded, not fixed: AddLanguageProfile's own inverse
     * (ITfInputProcessorProfiles::RemoveLanguageProfile) is never called. The
     * profile entry goes with UnregisterProfile/Unregister and the tree
     * deletion; whether that is complete is the elevated measurement's
     * question. */
    EXPECT_EQ(d.trace.count(L"RemoveLanguageProfile"), 0)
        << "AddLanguageProfile has no direct mirror call - recorded as a finding in the PR, not changed here";
}

/* The ruled shape (2026-10-30, issue #146): the DLL writes NO file, so the
 * Start-menu entry has one writer — the installer. The seam carries neither a
 * Programs folder nor a shell link any more, so a shortcut re-added to this
 * flow surfaces as an activation the doubles do not recognise. */
TEST(TsfRegistration, RegistrationWritesNoFile) {
    tsfmock::RegistrationDoubles d;
    d.Install();
    ASSERT_EQ(DllRegisterServer(), TRUE);
    EXPECT_EQ(d.trace.count(L"CoCreateUnknown"), 0) << "no activation outside the two msctf services (a shell link would land here)";

    ASSERT_EQ(DllUnregisterServer(), TRUE);
    EXPECT_EQ(d.trace.count(L"CoCreateUnknown"), 0) << "nor on the way out";
}

/* The activation branch: a host that answers GetActiveProfile with OUR profile
 * stops there, and the legacy call never runs. */
TEST(TsfRegistration, ModernActivationStopsWhenTheActiveProfileIsOurs) {
    tsfmock::RegistrationDoubles d;
    d.Install();
    d.profileMgr.activeClsid = CLSID_HunminTSF;
    d.profileMgr.activeProfile = HunminProfileGuid;
    d.profileMgr.activeLangid = 0x0412;

    ASSERT_EQ(DllRegisterServer(), TRUE);
    EXPECT_FALSE(d.profiles.activateLanguageProfileCalled) << "the legacy ActivateLanguageProfile is a fallback, not a second attempt";
}

/* ... and the fallback branch: a host that reports nothing active (S_FALSE)
 * sends the adapter to ITfInputProcessorProfiles::ActivateLanguageProfile with
 * the same three identifiers. */
TEST(TsfRegistration, LegacyFallbackRunsWhenTheHostReportsNothingActive) {
    tsfmock::RegistrationDoubles d;
    d.Install(); /* default activeClsid: nothing active, the pre-selection state */

    ASSERT_EQ(DllRegisterServer(), TRUE);
    ASSERT_TRUE(d.profiles.activateLanguageProfileCalled) << "no active profile -> the legacy call is the only thing left";
    EXPECT_TRUE(IsEqualGUID(d.profiles.activatedClsid, CLSID_HunminTSF));
    EXPECT_EQ(d.profiles.activatedLangid, (LANGID)0x0412);
    EXPECT_TRUE(IsEqualGUID(d.profiles.activatedProfile, HunminProfileGuid));
}
