## About
vxwm is a non-compositing tiling window manager for the X window system. It aims to be highly customizable and extensible while providing diverse options for different users.

## Default Bindings

The default mod key is the windows key, below are some core key bindings to get started.

| Key Bind | Function |
| -------- | -------- |
| mod + enter | spawns a terminal, by default this is [st](https://git.suckless.org/st/) from suckless.org |
| mod + q | kills a tab, a vxwm client consists of one or more tabs. |
| mod + k | focus previous client. |
| mod + j | focus next client. |
| mod + h | focus previous tab in current client. |
| mod + l | focus next tab in current client. |
| mod + s | toggle-selects a tab in a client. |
| mod + m | selected tabs are merged into the current client. |
| mod + , | selected tabs are splitted into individual clients. |
| mod + ctrl + q | quits the window manager |

## Installation

Build and install from source with GNU Make:

```
git clone https://github.com/x2w-sloth/vxwm
cd vxwm
sudo make install
```

For people using `startx`, simply add `exec vxwm` to the end of `~/.xinitrc`.

## Configuration

Settings can be found and customized in `src/config.h`.

For the new config to take effect please recompile and install with `make install` again.

## Disclaimer

This project is in early-development and many key features are still being implemented, feel free to open issues or send pull requests. Development is done in branch `dev` and merged into `master` when new features or bug fixes are ready.

## License

vxwm is licensed under GPL v2.
