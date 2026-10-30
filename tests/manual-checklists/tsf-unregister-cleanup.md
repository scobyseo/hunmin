# TSF unregister measurement — elevated register/unregister cycle (issue #84)

**Why this checklist is a manual tier.** `DllRegisterServer` writes HKLM and HKCR
(`RegCreateKeyExW` on `HKEY_CLASSES_ROOT`, `HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\CTF\TIP\{CLSID}`),
so `regsvr32` needs an elevated shell — and `DllUnregisterServer` now mirrors
every write it makes. No ctest tier can claim the registry state; the tier-F1
mock harness has no registry. Every step below is a real registry state change
on the host, so it is written down instead of assumed.

Run it (one elevated PowerShell/CMD session; worktree paths absolute):

```text
1. regsvr32.exe C:\...\frontends\tsf\build-msvc\Release\hunmin-tsf.dll
2. reg query "HKCR\CLSID\{F2C70001-0000-0000-0000-000000000001}" /s
3. reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}" /s
4. reg query "HKCU\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-0000-0000-0000-000000000001}" /s
5. reg query "HKCU\Software\Microsoft\CTF\SortOrder\AssemblyItem" /s   (record slots)
6. dir "%ProgramData%\Microsoft\Windows\Start Menu\Programs\Hunmin IME"
   (the installer's entry; the DLL writes no shortcut since the 2026-10-30
   ruling, issue #146, so the per-user folder must NOT gain one)
7. regsvr32.exe /u C:\...\frontends\tsf\build-msvc\Release\hunmin-tsf.dll
8. repeat 2-6
9. reg query "HKCR\CLSID\{F2C70001-...-0001}"  -> absent
   reg query "HKLM\SOFTWARE\Microsoft\CTF\TIP\{F2C70001-...-0001}"  -> absent
   HKCU TIP tree: record what remains under LanguageProfile (the `Enable`
   value is per-user and written by EnableLanguageProfile, so it is the
   registration-adjacent key the HKLM deletion cannot reach).
   CTF SortOrder/Assemblies: record exactly what remains and why.
```

Steps 4 and 6 were added 2026-10-30 (issue #146): without the HKCU TIP tree
snapshot the round cannot say whether the unregister calls remove the per-user
`Enable` value, and without the Start-menu paths it cannot say which writer
fired — which since the 2026-10-30 ruling (option C, `docs/OPTIONS-MEASURED.md`
§URM-3) must be the installer alone.

## What to look for, and why each probe is there

| step | expected | what it isolates |
|---|---|---|
| 3 after 1 | `TIP\{CLSID}` with default `= Hunmin`, `LanguageProfile\0x00000412\{F2C70002-...-0002}` `Description`/`IconFile`/`IconIndex`; the `Enable` value is NOT here | the keys registration writes directly / through `AddLanguageProfile` |
| 4 after 1 | `HKCU\...\TIP\{CLSID}\LanguageProfile\0x00000412\{...}\Enable = 0x1` | the per-user half `EnableLanguageProfile` writes — the key the HKLM tree deletion cannot reach |
| 5 after 1 | SortOrder slot for `{F2C70001-...-0001}` may appear (Windows' own store) | the deliberately-untouched state: unregister must NOT delete it |
| 6 after 1 | the installer's entry in the all-users folder only when the installer ran; the per-user folder gains NOTHING (the DLL stopped writing a shortcut on 2026-10-30, issue #146) | which Start-menu writer fired on this run |
| 9 after 7 | both `{CLSID}` trees gone; log `hunmin-reg-debug.log` (module dir, opened append) shows `pProfileMgr->UnregisterProfile hr=`, `pProfile->Unregister hr=`, `UnregisterCategory keyboard=`, `UnregisterCategory category=`, `RegDeleteTreeW(HKLM ...) st=0x0`, `RegDeleteTreeW(HKCR ...) st=0x0` | which COM call removed which piece of state — the measurement the implementation comment in `frontends/tsf/src/tsf.cpp` (`DllUnregisterServer`) still owes |
| 4 after 7 | the per-user `Enable` value is the one artefact whose removal the tree deletions cannot explain; the per-user Start-menu folder must still hold no `Hunmin IME 설정.lnk` | if `Enable` is gone, one of the two COM unregister calls removed it (name which, from the log order); a `.lnk` reappearing means a pre-ruling build is still registered |
| log absent-state | if a COM unregister hr is non-zero but the state is gone, record which single call actually did it | the mirror is exact, not more: do not add calls that remove state registration never wrote |

## Worse-case edges, recorded rather than hidden

- If `regsvr32 /u` is run TWICE, or `/u` runs on a never-registered DLL: every
  call must still return TRUE (log the non-zero hr values, no crash). The
  mirror is of a possibly-absent state.
- Windows Settings' HKCU `SortOrder` slot for the Hunmin CLSID is owned by the
  CTF service: if it is stale after unregistering, record it — it is
  registration-adjacent per-user state, but registration never wrote it, so the
  mirror must not clean it. Measurement only; the ruling on stale slots is the
  maintainer's (docs/REMAINING-WORK.md row TSF-UNREG-84).

Status: **measured elevated 2026-10-30/31 against the shipped DLL** — the results,
per call, are in `docs/WINDOWS-TSF-REGISTRATION.md` §"Unregister state". What the
round showed, in one line each: `Unregister` alone deletes the HKLM TIP tree;
`UnregisterProfile` removes the profile's values; `RemoveLanguageProfile` is
callable (`S_OK` present, `E_FAIL` absent) and adds nothing; the per-user
`Enable` value survives the whole flow; the shipped DLL writes no Start-menu
entry.
