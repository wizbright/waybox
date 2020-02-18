# waybox
An openbox clone on Wayland (WIP)

This is the basic-apps branch, in which I'm working to get simple apps to work.
Currently, I'm targeting wlroots 0.6.0, but after I get the basic functionality
worked out, I want to get it to compile with wlroots 0.10.0.  Right now, a
weston-terminal window will show, but it won't receive keyboard input.

### Goals
The main goal of this project is to provide a similar feel to openbox but on wayland

### Contributing

[Details on
contributing.](https://github.com/wizbright/waybox/blob/master/CONTRIBUTING.md)

### Dependencies

* meson
* wlroots
* wayland

### Build instructions

```
meson build
cd build
ninja
```

After that, you should have an executable in build/

### Contact
I can be found as wiz on Rizon and WizBright on Freenode. 
Join [#waybox](http://webchat.freenode.net/?channels=waybox) for discussion
