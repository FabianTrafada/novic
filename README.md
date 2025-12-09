# Novic

A sleek, Dynamic Island-style media player widget for Wayland compositors.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20(Wayland)-lightgrey)

## Features

- 🎵 **Real-time Audio Visualizer** - Animated waveform bars that react to your music
- 🎨 **Minimal Design** - Clean, floating widget that stays out of your way
- 🖱️ **Hover to Expand** - See full track info, album art, and controls on hover
- ⏯️ **Media Controls** - Play/pause, skip, and previous track buttons
- 📊 **Progress Bar** - Visual playback progress indicator
- 🔌 **MPRIS Support** - Works with Spotify, Firefox, VLC, and any MPRIS-compatible player

## Preview

```
┌─────────────────────────────────────┐
│  🎵  Song Title        |||||||      │  ← Collapsed (normal)
└─────────────────────────────────────┘

┌─────────────────────────────────────┐
│  ┌──────┐                           │
│  │ Art  │  Song Title               │  ← Expanded (on hover)
│  │      │  Artist Name    |||||||   │
│  └──────┘                           │
│  ◄◄    ▶    ►►     ───●────────    │
└─────────────────────────────────────┘
```

## Installation

### From Releases (Recommended)

Download the latest package from [Releases](https://github.com/FabianTrafada/novic/releases):

**Debian/Ubuntu:**
```bash
sudo dpkg -i novic_*_amd64.deb
```

**Fedora/RHEL:**
```bash
sudo dnf install novic-*.rpm
```

### Building from Source

#### Dependencies

**Debian/Ubuntu:**
```bash
sudo apt install cmake g++ pkg-config libgtkmm-3.0-dev libpulse-dev
# gtk-layer-shell may need to be built from source
```

**Fedora:**
```bash
sudo dnf install cmake gcc-c++ pkg-config gtkmm30-devel pulseaudio-libs-devel gtk-layer-shell-devel
```

**Arch Linux:**
```bash
sudo pacman -S cmake gtkmm3 libpulse gtk-layer-shell
```

#### Build

```bash
git clone https://github.com/FabianTrafada/novic.git
cd novic
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

#### Install

```bash
sudo make install
```

Or run directly:
```bash
./novic
```

### Building Packages Locally

**DEB Package:**
```bash
./scripts/build-deb.sh 0.1.0
sudo dpkg -i novic_0.1.0_amd64.deb
```

**RPM Package:**
```bash
./scripts/build-rpm.sh 0.1.0
sudo dnf install novic-0.1.0-1.*.rpm
```

## Usage

Simply run:
```bash
novic
```

The widget will appear at the top of your screen. It automatically detects media players via MPRIS.

### Supported Players

Any MPRIS-compatible media player, including:
- Spotify
- Firefox / Chrome (web media)
- VLC
- Rhythmbox
- Clementine
- MPD (with mpDris2)
- And many more...

## Requirements

- **Wayland** compositor with layer-shell support (Sway, Hyprland, etc.)
- **PulseAudio** or PipeWire (with PulseAudio compatibility)
- **GTK 3** and **gtkmm 3.0**
- **gtk-layer-shell**

## Configuration

Currently, Novic uses sensible defaults. Configuration file support is planned for future releases.

## Contributing

Contributions are welcome! Feel free to:
- Report bugs
- Suggest features
- Submit pull requests

## License

MIT License - see [LICENSE](LICENSE) for details.

## Acknowledgments

- Inspired by Apple's Dynamic Island
- Built with [GTK](https://gtk.org/) and [gtkmm](https://gtkmm.org/)
- Layer shell support via [gtk-layer-shell](https://github.com/wmww/gtk-layer-shell)

