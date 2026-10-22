# SamFlash

A lightweight Qt6-based GUI flashing tool for Samsung devices on Linux. SamFlash bridges official firmware flashing via **Odin4** and custom recovery flashing via **Heimdall** in a single native desktop interface.

---

## Features

- **Odin Mode (Odin4)**:
  - Supports all standard firmware partition slots: `BL`, `AP`, `CP`, `CSC`, and `UMS`.
  - Automatic handling for `.tar.md5` archives without lengthy file copying.
  - Toggles for Auto Reboot, Erase NAND, and Re-download.
- **Recovery Mode (Heimdall)**:
  - Flash custom recoveries (e.g., TWRP, OrangeFox) and `vbmeta` images with one click.
  - Option to prevent automatic reboot after flashing.
- **Real-Time Monitoring**:
  - Live terminal output log.
  - Progress bar updated dynamically from the underlying flashing engine.
  - Auto-detection and status indicator for connected Samsung devices in Download Mode.

---

## Dependencies

### Runtime Dependencies
- **Qt6** (`qt6-base`)
- **odin4** (for Odin mode)
- **heimdall** (for Recovery mode)

### Build Dependencies
- **CMake** (>= 3.16)
- **C++17** compatible compiler (`g++` or `clang++`)

---

## Building and Installing

### Arch Linux (PKGBUILD)

A `PKGBUILD` is included for Arch Linux and derived distributions:

```bash
makepkg -si
```

### Manual Build with CMake

```bash
# Clone the repository
git clone https://github.com/Anand-Rift/samflash.git
cd samflash

# Configure and compile
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run directly
./build/samflash

# (Optional) Install system-wide
sudo cmake --install build
```

---

## Usage

1. **Put your Samsung device into Download Mode**:
   - Power off the device.
   - Hold `Volume Down` + `Volume Up` and connect the USB cable to your PC (or hold `Volume Down` + `Power` + `Home` on older models).
   - Press `Volume Up` to confirm entry into Download Mode.
2. Launch `samflash` from terminal or application launcher.
3. Choose **Odin mode** or **Recovery mode**:
   - For stock ROM: Select your `BL`, `AP`, `CP`, and `CSC` tar archives.
   - For custom recovery: Select your recovery `.img` and optional `vbmeta.img`.
4. Click **Start** to begin flashing.

> **Note on USB Permissions**: Ensure your user belongs to the `uucp` / `dialout` group or has appropriate udev rules set up to communicate with Samsung USB download mode interfaces.

---

## License

This project is licensed under the terms of the [GNU General Public License v3.0](LICENSE).
