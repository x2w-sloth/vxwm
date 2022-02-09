vxwm is a non-compositing tiling window manager for the X window system.

It aims to be highly customizable and extensible while providing diverse options for different users.

## Configuration

Settings can be found and customized in `src/config.h`.

By default the Mod key is the windows key, below are some core key bindings.

- Mod + Ctrl + q : quits the window manager.
- Mod + Enter : spawns a terminal, by default this is [st](https://git.suckless.org/st/) from suckless.org
- Mod + q : kills a tab, a vxwm client consists of one or more tabs.
- Mod + h : focus previous tab in current client.
- Mod + l : focus next tab in current client.
- Mod + k : focus previous client.
- Mod + j : focus next client.
- Mod + s : toggle-selects a tab in a client.
- Mod + m : selected tabs are merged into the focus client.
- Mod + , : selected tabs are splitted into individual clients.


## Installation

Build and install from source with GNU Make:

```
git clone https://github.com/x2w-sloth/vxwm
cd vxwm
sudo make install
```

For people using `startx`, simply add `exec vxwm` to the end of `~/.xinitrc`.

## Disclaimer

This project is in early-development and many key features are still being implemented, feel free to open issues or send pull requests. Development is done in branch `dev` and merged into `master` when new features or bug fixes are ready.

## License

vxwm is licensed under GPL v2.
