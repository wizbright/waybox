# Waybox

A \*box-style (minimalist) Wayland compositor modeled largely on Openbox (WIP)

### Goals

The main goal of this project is to provide a similar feel to \*box-style window managers but on Wayland

### Contributing

[Details on
contributing.](https://github.com/wizbright/waybox/blob/master/CONTRIBUTING.md)

### Dependencies

* [Meson](https://mesonbuild.com/) or [muon](http://muon.build)
* [Wayland](https://wayland.freedesktop.org/)
* [libevdev](https://www.freedesktop.org/wiki/Software/libevdev/)
* [libinput](http://www.freedesktop.org/wiki/Software/libinput)
* [libxml2](http://xmlsoft.org/)
* [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots/)
* [xkbcommon](https://xkbcommon.org/)

### Build instructions
```
meson setup build
cd build
ninja
```

After that, you should have an executable as waybox/waybox

For those who don't want to use a Python-based build system, it's also possible
to use muon instead of Meson.

### Screenshots

![Plain desktop with wallpaper, panel, and dock](../../raw/master/screenshots/emptydesktop.png)

![Showing Firefox and some of the Waybox source code](../../raw/master/screenshots/work.png)

![All work and no play](../../raw/master/screenshots/play.png)

### Useful Programs

Because \*box-style compositors are minimalist, most functionality is left to external programs.  As such, Waybox only functions as a box in which you can put whatever you need.  Here are some useful programs to complement Waybox if you desire:

* Panel: You can use [Waybar](https://github.com/Alexays/Waybar) or [yambar](https://codeberg.org/dnkl/yambar), similar to tint2 or fbpanel in Openbox or Fluxbox.
* Dock: You can use [Cairo Dock](https://www.glx-dock.org/) just like you did on Openbox.  There's also a [port with Wayland-specific enhancements](https://github.com/dkondor/cairo-dock-core/) that you may want to try. A much more compact option is [LavaLauncher](https://sr.ht/~leon_plickat/LavaLauncher/), but it's much harder to configure.
* Wallpaper utility: There are various utilities to set your wallpaper, each with their own advantages, including [wpaperd](https://github.com/danyspin97/wpaperd) (can select a random wallpaper from a directory), [swaybg](https://github.com/swaywm/swaybg) (can set the background color as well well as a wallpaper), and [hyprpaper](https://github.com/hyprwm/hyprpaper) (can change the wallpaper dynamically during runtime through IPC).
* Notification client: [mako](https://wayland.emersion.fr/mako/)
* [wl-clipboard](https://wayland.emersion.fr/mako/): Access the clipboard in scripts (also used by [neovim](https://neovim.io/))
* Screenshots: [grim](https://git.sr.ht/~emersion/grim) and [slurp](https://github.com/emersion/slurp)
* Screen recording: [wf-recorder](https://github.com/ammen99/wf-recorder)
* Menu support: [rofi-wayland](https://github.com/lbonn/rofi)

### Contact
I can be found as wiz on Rizon and wizbright on Libera. 
Join [#waybox](https://libera.chat/guides/connect) for discussion
