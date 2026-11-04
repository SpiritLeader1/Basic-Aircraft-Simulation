# Basic Simulation and FlightGear:Run Guide (Windows & Linux)

A short guide to downloading, installing, and launching [FlightGear](https://www.flightgear.org/), the free, open-source flight simulator.



## Requirements

- Windows 10/11 (64-bit) or a modern 64-bit Linux distribution
- A GPU with up-to-date drivers that supports OpenGL 3.2 or newer
- At least 8 GB RAM (16 GB recommended)
- About 10 GB of free disk space (the default aircraft and scenery take a few GB, and more scenery can be added later)
- A joystick or yoke is optional, since keyboard work too

---

## Windows

### 1. Download

1. Go to the official download page: https://www.flightgear.org/download/
2. Under **Windows**, download the latest stable installer (a `.exe` file).
3. Only download from flightgear.org or its official links, and avoid third-party mirrors.

### 2. Install

1. Double-click the downloaded installer (for example `FlightGear-<version>.exe`).
2. If Windows SmartScreen appears, click **More info → Run anyway**.
3. Follow the setup wizard and keep the default install location unless you have a reason to change it.
4. Wait for installation to finish. It can take a few minutes.

### 3. Run

**Run the executable directly**
1. You can run `startFlightGear.bat or run the following commands in command prompt

```bat
@echo off
fgfs --fdm=external ^
    --native-fdm=socket,in,60,,5500,udp ^
    --aircraft=c172p ^
    --lat=39.6 ^
    --lon=32.4 ^
    --altitude=3281 ^
    --heading=90 ^
    --timeofday=noon ^
    --enable-hud ^
    --disable-hud-3d
```

2. After FlightGear opens, you can run `sim.exe` to start simulation.

---

## Linux

There are three common ways to install FlightGear. Pick one.

### Option 1: AppImage (works on most distributions)

1. Download the latest `.AppImage` from https://www.flightgear.org/download/
2. Make it executable and run it:
   ```bash
   chmod +x FlightGear-*.AppImage
   ./FlightGear-*.AppImage
   ```
3. If it fails to start, your system may need FUSE installed (for example `sudo apt install libfuse2` on Ubuntu/Debian).

### Option 2: Flatpak

```bash
flatpak install flathub org.flightgear.FlightGear
flatpak run org.flightgear.FlightGear
```

### Option 3: Distribution package manager

| Distribution | Command |
|---|---|
| Ubuntu / Debian | `sudo apt update && sudo apt install flightgear` |
| Fedora | `sudo dnf install flightgear` |
| Arch / Manjaro | `sudo pacman -S flightgear` |
| openSUSE | `sudo zypper install FlightGear` |

Distribution packages are sometimes a few versions behind. For the newest release, use the AppImage or Flatpak.

### Run (package manager install)

1. Start the launcher from the application menu, or from a terminal:

```bash
./startFlightGear.sh
```

Or launch directly with options:

```bash
fgfs --fdm=external \
    --native-fdm=socket,in,60,,5500,udp \
    --aircraft=c172p \
    --lat=39.6 \
    --lon=32.4 \
    --altitude=3281 \
    --heading=90 \
    --timeofday=noon \
    --enable-hud \
    --disable-hud-3d
```

2. After starting FlightGear, open another terminal and run the command `./sim` for starting simulation.

---

## Useful command-line options

| Option | Description |
|---|---|
| `--aircraft=c172p` | Choose the aircraft (Cessna 172P here) |
| `--airport=LTAC` | Start at an airport by ICAO code |
| `--runway=03` | Start on a specific runway |
| `--timeofday=noon` | Set the time of day |
| `--fg-scenery=<path>` | Add an extra scenery folder |
| `--enable-fullscreen` | Launch in fullscreen |

Full list: https://wiki.flightgear.org/Command_line_options

> The first launch can take a while because FlightGear loads aircraft and builds caches.

## Troubleshooting

- **Black screen or crash on start:** Update your graphics drivers. On Linux, make sure you are using the proper GPU driver (Mesa or proprietary NVIDIA) rather than a software renderer.
- **Low FPS:** Lower the rendering settings in the launcher (**Settings → Rendering**).
- **Missing scenery (ocean everywhere):** Use the built-in scenery downloader or install scenery manually. See https://wiki.flightgear.org/Howto:Install_scenery
- **Controls not detected:** Plug in the device before launching, then check the **Input/Output** settings in the launcher. On Linux, you may need to add your user to the `input` group.
- **Won't start after an update:** Reset your settings folder:
  - Windows: delete `%APPDATA%\flightgear.org`
  - Linux: rename or delete `~/.fgfs`
  - Linux (Flatpak): `~/.var/app/org.flightgear.FlightGear`

## Links

- Website: https://www.flightgear.org/
- Wiki and documentation: https://wiki.flightgear.org/
- Forum: https://forum.flightgear.org/

## License

FlightGear is released under the GNU GPL. This guide is provided as-is and is not affiliated with the FlightGear project.