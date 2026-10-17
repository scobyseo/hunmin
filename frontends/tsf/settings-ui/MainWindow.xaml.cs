// SPDX-FileCopyrightText: 2026 hunmin contributors
// SPDX-License-Identifier: LGPL-2.1-or-later
//
// The settings UI (issue #71), maintainer rulings 2026-10-17:
//   - a separate WinUI 3 exe (no engine linking, no candidate logic) that
//     edits the configuration store (frontends/tsf/src/config.h contract);
//   - the mode-toggle key row is ONE configurable binding set by key
//     capture ("한영 전환 키는 하나만 선택해도 되고, 복잡하게 입력하는 게
//     아니라 키 입력 방식을 사용하자") — the store keeps the
//     v1:<vk>:<mods> ';' list grammar, the UI manages one entry, and the
//     adapter appends the implicit bare Han/Eng platform alternate (the
//     unkillable physical key);
//   - compact sizing (~480-560 px wide); the measured window rect is in
//     tests/manual-checklists/tsf-settings-window.md.
using System.Runtime.InteropServices;
using System.Text.Json;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Input;
using Windows.System;
using Microsoft.Win32;

namespace Hunmin.Tsf.SettingsUi;
public partial class MainWindow : Window
{
    const string RegRoot = "Software\\Hunmin\\TSF";
    const string ValHangul = "HangulLayout";
    const string ValLatin = "LatinLayout";
    const string ValToggle = "HangulEngKeys";
    public record SlotRow(string Id, string Label);

    bool _capturing;
    int _capturedVk = -1;
    int _capturedMods = 0;
    bool _uncaptured; // capture cleared a previously stored binding

    public MainWindow()
    {
        InitializeComponent();
        Title = "Hunmin TSF settings";
        // Key capture rides the root panel's KeyDown: the window has focus
        // while capturing, no global hook (maintainer instruction).
        var root = (Panel)Content;
        root.KeyDown += Root_KeyDown;
        root.Loaded += (_, _) => root.Focus(FocusState.Programmatic);
        LoadLayouts();
        Restore();
    }

    // ---- compact sizing: measured post-layout, one shot ----
    // Called from App.OnLaunched via the DispatcherQueue after Activate;
    // ResizeClient only sticks down to the content's natural measure, so
    // the panel is pinned narrow here first.
    public void Compact()
    {
        try
        {
            var root = (Panel)Content;
            root.Width = 420;
            // ResizeClient input scales with the display's DPI factor
            // (measured on this host: (500,600) -> 625x750 client px);
            // 420x500 requests land the on-screen window inside the quoted
            // 480-560 px band (measured 2026-10-17: 545x~660 px with title bar).
            AppWindow.ResizeClient(new Windows.Graphics.SizeInt32(420, 500));
            var s = AppWindow.Size;
            LogDebug($"Compact client reserve w={s.Width} h={s.Height}");
        }
        catch (Exception ex)
        {
            LogDebug("Compact FAILED: " + ex.Message);
        }
    }

    static void LogDebug(string msg)
    {
        try
        {
            File.AppendAllText(Path.Combine(AppContext.BaseDirectory, "settings-ui-debug.log"),
                DateTime.Now.ToString("HH:mm:ss.fff ") + msg + Environment.NewLine);
        }
        catch { /* diagnostics only */ }
    }

    // ---- key capture ----
    void ToggleKeyChange_Click(object sender, RoutedEventArgs e)
    {
        _capturing = true;
        CaptureHint.Text = "키를 누르세요… (ESC 취소)";
    }

    static bool IsModifierKey(VirtualKey k) => k is
        VirtualKey.Shift or VirtualKey.LeftShift or VirtualKey.RightShift or
        VirtualKey.Control or VirtualKey.LeftControl or VirtualKey.RightControl or
        VirtualKey.Menu or VirtualKey.LeftMenu or VirtualKey.RightMenu or
        VirtualKey.LeftWindows or VirtualKey.RightWindows or VirtualKey.Application;

    static int ModsNow()
    {
        int m = 0;
        if (IsDown(0x10)) m |= 0x1; // VK_SHIFT
        if (IsDown(0x11)) m |= 0x2; // VK_CONTROL
        if (IsDown(0x12)) m |= 0x4; // VK_MENU (either Alt side)
        return m;
        [DllImport("user32.dll")]
        static extern short GetKeyState(int nVirtKey);
        static bool IsDown(int vk) => (GetKeyState(vk) & 0x8000) != 0;
    }

    void Root_KeyDown(object sender, KeyRoutedEventArgs e)
    {
        if (!_capturing)
            return;
        e.Handled = true;
        if (e.KeyStatus.WasKeyDown)
            return; // auto-repeat: swallow (keybinding.h repeat rule)
        if (e.Key == VirtualKey.Escape)
        {
            _capturing = false;
            CaptureHint.Text = "";
            Restore(); // dismissed capture keeps the stored binding
            return;
        }
        if (IsModifierKey(e.Key))
            return; // modifier-only: wait for the non-modifier key (Android rule)
        _capturedVk = (int)e.Key;
        _capturedMods = ModsNow();
        _capturing = false;
        _uncaptured = false;
        CaptureHint.Text = "";
        ToggleKeyLabel.Text = Describe(_capturedVk, _capturedMods)
            + $" (v1:{_capturedVk}:{_capturedMods})";
    }

    // vk+mods -> label (stored backtick: the label strings are the manual
    // checklist's expected text).
    static string Describe(int vk, int mods)
    {
        string prefix = "";
        if ((mods & 0x1) != 0) prefix += "Shift+";
        if ((mods & 0x2) != 0) prefix += "Ctrl+";
        if ((mods & 0x4) != 0) prefix += "Alt+";
        var baseName = vk switch
        {
            21 => "한/영",           // VK_HANGEUL (HangulMode)
            165 => "오른쪽 Alt",      // VK_RMENU
            164 => "왼쪽 Alt",       // VK_LMENU
            32 => "Space",
            112 => "F1", 113 => "F2", 114 => "F3", 115 => "F4",
            116 => "F5", 117 => "F6", 118 => "F7", 119 => "F8",
            120 => "F9", 121 => "F10", 122 => "F11", 123 => "F12",
            >= 48 and <= 57 => ((char)vk).ToString(),
            >= 65 and <= 90 => ((char)vk).ToString(),
            _ => null,
        };
        return baseName is null ? "?" : prefix + baseName;
    }

    // The store grammar stays a list; one UI entry == one entry in it (the
    // adapter still appends the bare Han/Eng platform alternate).
    static (int vk, int mods)? ParseSingle(string store)
    {
        foreach (var part in store.Split(';'))
        {
            var bits = part.Trim().Split(':');
            if (bits.Length == 3 && bits[0] == "v1"
                && int.TryParse(bits[1], out var vk) && int.TryParse(bits[2], out var mods))
                return (vk, mods);
        }
        return null;
    }

    // ---- layouts ----
    static string DataDir()
    {
        var args = Environment.GetCommandLineArgs();
        for (int i = 0; i + 1 < args.Length; i++)
            if (args[i] == "--data")
                return args[i + 1];

        var env = Environment.GetEnvironmentVariable("HUNMIN_DATA_DIR");
        if (env is { Length: > 0 })
            return env;

        var exedir = AppContext.BaseDirectory;
        var local = Path.Combine(exedir, "data");
        if (Directory.Exists(local))
            return local;
        return Path.Combine(exedir, "..", "data"); // deployed: <dll dir>\data
    }

    void LoadLayouts()
    {
        var (hangul, latin) = ParseLayouts(DataDir());
        HangulList.ItemsSource = hangul;
        LatinList.ItemsSource = latin;
    }

    public static (List<SlotRow> hangul, List<SlotRow> latin) ParseLayouts(string dataDir)
    {
        var hangul = new List<SlotRow>();
        var latin = new List<SlotRow>();
        var layouts = Path.Combine(dataDir, "layouts");
        if (!Directory.Exists(layouts))
            return (hangul, latin);

        foreach (var file in Directory.EnumerateFiles(layouts, "*.json"))
        {
            JsonDocument doc;
            try
            {
                doc = JsonDocument.Parse(File.ReadAllText(file));
            }
            catch (JsonException)
            {
                continue; // malformed layout file: skip, not a crash
            }

            using (doc)
            {
                string id = doc.RootElement.TryGetProperty("id", out var idEl)
                    ? idEl.GetString() ?? "" : "";
                if (id.Length == 0) continue;

                string nameKo = doc.RootElement.TryGetProperty("name", out var nameEl)
                    && nameEl.ValueKind == JsonValueKind.Object
                    && nameEl.TryGetProperty("ko", out var koEl)
                    ? koEl.GetString() ?? "" : "";

                string mode = doc.RootElement.TryGetProperty("mode", out var modeEl)
                    ? modeEl.GetString() ?? "" : "";

                string label = string.Concat(
                    nameKo.Length > 0 ? nameKo : id, " (", id, ")");
                if (mode == "hangul") hangul.Add(new SlotRow(id, label));
                else if (mode == "latin") latin.Add(new SlotRow(id, label));
            }
        }
        hangul.Sort((a, b) => string.CompareOrdinal(a.Id, b.Id));
        latin.Sort((a, b) => string.CompareOrdinal(a.Id, b.Id));
        return (hangul, latin);
    }

    void Restore()
    {
        _capturedVk = -1;
        _capturedMods = 0;
        _storedHasToggle = false;
        _storedToggle = "";
        using var key = Registry.CurrentUser.OpenSubKey(RegRoot);
        Select(HangulList, key?.GetValue(ValHangul) as string ?? "sin3-p2");
        Select(LatinList, key?.GetValue(ValLatin) as string ?? "us-qwerty");
        _storedToggle = (key?.GetValue(ValToggle) as string) ?? "";
        var store = ParseSingle(_storedToggle);
        _storedHasToggle = _storedToggle.Trim().Length > 0;
        if (store is null)
        {
            ToggleKeyLabel.Text = "설정 없음 (물리 한/영 키)";
        }
        else
        {
            _capturedVk = store.Value.vk;
            _capturedMods = store.Value.mods;
            ToggleKeyLabel.Text = Describe(store.Value.vk, store.Value.mods)
                + $" (v1:{store.Value.vk}:{store.Value.mods})";
        }
        StatusText.Text = "";
    }

    static void Select(ListView list, string id)
    {
        for (int i = 0; i < list.Items.Count; i++)
            if (((SlotRow)list.Items[i]).Id == id)
                { list.SelectedIndex = i; return; }
        list.SelectedIndex = -1; // unknown stored id: no selection, not an error
    }

    // ---- the store write (config.h contract: REG_SZ values) ----
    void Apply_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            using var key = Registry.CurrentUser.CreateSubKey(RegRoot);
            Set(key, ValHangul, ((HangulList.SelectedItem as SlotRow)?.Id) ?? "");
            Set(key, ValLatin, ((LatinList.SelectedItem as SlotRow)?.Id) ?? "");
            if (_capturedVk >= 0)
            {
                Set(key, ValToggle, $"v1:{_capturedVk}:{_capturedMods}");
            }
            else if (_uncaptured || !_storedHasToggle)
            {
                key.DeleteValue(ValToggle, false); // nothing configured: value out
            }
            else
            {
                Set(key, ValToggle, _storedToggle); // untouched: rewrite the stored one
            }
            StatusText.Text = "저장했습니다.";
        }
        catch
        {
            StatusText.Text = "저장하지 못했습니다.";
        }
    }

    string _storedToggle = "";
    bool _storedHasToggle = false;
    void Set(RegistryKey key, string name, string value)
    {
        if (value.Length > 0)
            key.SetValue(name, value, RegistryValueKind.String);
        else
            key.DeleteValue(name, false);
    }
}
