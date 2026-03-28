# FrameGen - Frame Generation DLL for DirectX 11 Games

## Description

FrameGen is a DLL for frame interpolation in DirectX 11 games. Similar to DLSS 3 Frame Generation and AMD FSR 3.

## Features

- **Frame Interpolation**: x2, x3, x4 multipliers
- **Settings Menu**: Press DEL to open
- **Minimal Latency**: Input lag compensation
- **Motion Vectors**: Quality interpolation using motion detection
- **FPS Overlay**: Display FPS and statistics

## Requirements

- Windows 10/11
- Visual Studio 2017/2019/2022 with:
  - Desktop development with C++
  - Windows 10/11 SDK
- DirectX 11 game

## Installation

### Automatic Compilation

1. Place the `FrameGen` folder in `E:\Code`
2. Run `build.bat`
3. The DLL will be created in `output\d3d11.dll`
4. Copy `d3d11.dll` to the game folder (where the .exe is)

### Usage

1. Run the game
2. Frame Generation starts automatically
3. Press **DEL** to open/close the menu
4. Press **INS** to toggle frame generation on/off

## Settings

### Frame Multiplier
- **x2**: 60 FPS -> 120 FPS (recommended)
- **x3**: 60 FPS -> 180 FPS
- **x4**: 60 FPS -> 240 FPS (for 240Hz monitors)

### Input Lag Compensation
- **Off**: No compensation, maximum smoothness
- **Low**: Slight compensation for faster response
- **Medium**: Balanced compensation
- **High**: Maximum responsiveness for competitive games

### Quality Settings
- **Fast**: Fast interpolation, minimal GPU load
- **Balanced**: Balance between quality and performance
- **Quality**: Maximum interpolation quality

## Hotkeys

| Key | Action |
|---------|----------|
| DEL | Open/close menu |
| INS | Enable/disable Frame Generation |

## Compatibility

### Works with:
- Most DirectX 11 games
- 64-bit games
- Games without anti-cheat (or with weak anti-cheat)

### Possible issues:
- Some games use their own D3D11 loader
- Anti-cheat systems may block DLL injection
- VSync should be disabled in game settings

## Troubleshooting

### DLL not loading
- Check that the game uses DirectX 11
- Temporarily disable antivirus
- Make sure the file is named `d3d11.dll`

### Menu not opening
- Make sure the game is in focus
- Check that DEL key is not used by the game
- Try recreating the config file

### Low FPS after enabling
- Reduce Frame Multiplier
- Set Quality to Fast
- Disable Motion Vectors

## Build Errors

If you get compilation errors, send the output from `build.bat` for fixing.

Common issues:
- **PowerShell not found**: Install PowerShell or manually download dependencies
- **Visual Studio not found**: Install VS with C++ desktop development
- **Cannot download files**: Check internet connection or download manually:
  - MinHook: https://github.com/TsudaKageyu/minhook
  - ImGui: https://github.com/ocornut/imgui

## License

This project is for educational purposes only.
