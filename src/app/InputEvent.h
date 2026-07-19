#pragma once

enum class InputEvent
{
    None,

    ShowHelp,
    PrintSettings,

    EnterApSetup,
    ShowStaSettings,
    ToggleSettingsDisplay,
    ExitCurrentView,

    ToggleUiLab,
    NextUiLabScene,
    PrintUiTuning,
    SaveUiTuning,
    ResetUiTuning,

    NextUiTheme,
    SwitchRange,
    ToggleGroundTraffic,
    CycleScheduleIdleDisplayMode,

    KeyUpShort,
    KeyUpLong,
    KeyUpDouble,
    KeyDownShort,
    KeyDownLong,
    KeyDownDouble,

    BootButtonShort,
    BootButtonDouble,
    BootButtonLong,

    SaveSettings,
    LoadSettings,
    ResetDefaults,

    PrintTimeStatus,
    PrintDeviceStatus,

    PrintApiAuthStatus,
    ClearAuthToken,

    Reboot
};
