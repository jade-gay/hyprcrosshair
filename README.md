# hyprcrosshair

A simple crosshair overlay for Hyprland.

## Installation

### 
```bash
git clone https://github.com/jade-gay/hyprcrosshair.git
cd hyprcrosshair
makepkg -si
```
### Dependencies

Make sure you have the following dependencies installed:

- gtk4 (>= 4.10)
- libadwaita (>= 1.2)
- gtk4-layer-shell
- meson (build dependency)
- ninja (build dependency)
- gcc (build dependency)

On Arch Linux, you can install dependencies with:
```bash
sudo pacman -S gtk4 libadwaita gtk4-layer-shell meson ninja gcc
```
## Usage

After installation, run the `hyprcrosshair` binary from your terminal or your app launcher.

The `.desktop` file is installed to integrate with your desktop environment.

## Repository

https://github.com/jade-gay/hyprcrosshair
