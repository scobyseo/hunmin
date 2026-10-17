// SPDX-FileCopyrightText: 2026 hunmin contributors
// SPDX-License-Identifier: LGPL-2.1-or-later
using Microsoft.UI.Xaml;

namespace Hunmin.Tsf.SettingsUi;

public partial class App : Application
{
    MainWindow? _window;

    public App() => InitializeComponent();

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        _window = new MainWindow();
        _window.Activate();
        // Compact sizing after the first layout pass (a ResizeClient during
        // layout crashed the heap: see settings-ui-debug.log note). Fixed
        // 520x700 client == the maintainer's compact band.
        _window.DispatcherQueue.TryEnqueue(
            Microsoft.UI.Dispatching.DispatcherQueuePriority.Low,
            () => _window?.Compact());
    }
}
