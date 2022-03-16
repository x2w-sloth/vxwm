# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [v0.2.0] - 2022-03-16
### Added
- CHANGELOG.md
- Bottom status bar that displays page information, layout status, tab name, and root window WM_NAME (which is set to vxwm version string on startup).

## [v0.2.0-alpha] - 2022-03-03
### Added
- Experimental pixmap drawing with cairo-xcb extensions.
- Status bar fixed at bottom of monitor. Displays page and layout information.

## [v0.1.3] - 2022-02-24
### Added
- Set or toggle page tags on clients, allow a client (and therefore its tabs) to be accessed in multiple pages simultaneously. This is similar to dwm's tag/view system, except each page uses its own layout and parameters to arrange tiled windows, and also exactly one page is being viewed at any given time.

### Changed
- Client Tab Capacity: A client now hosts up to 64 tabs, this should be a reasonable upper bound and allows us to use `uint64_t` bitmasks to handle tab selections. A merge that results in a client hosting more than 64 tabs is simple ignored.

### Fixed
- Focusing tab resizes underlying window to the wrong height, fixed now.

## [v0.1.2] - 2022-02-19
### Added
- Client Fullscreen: Toggle focused client to be fullscreen.
- Set Page Parameters: Change parameters of current page. These parameters are interpreted by the page's layout function.
- Set Page Layout: Change the layout function of current page.

### Changed
- Kill Tab Behaviour: If a selection is present, kill all selected tabs, otherwise kill current tab in focused client.
- Split Client Behaviour: If no selection is present and the focused client has 2 or more tabs, the current tab is splitted.

### Fixed
- Tab Swapping: Remember to swap selection bits.

## [v0.1.1] - 2022-02-14
### Added
- Client Swapping: Swaps the position of two clients in the same page.
- Tab Swapping: Swaps the position of two tabs in the same client.

## [v0.1.0] - 2022-02-10
### Added
- README.md
- LICENSE under GPL v2.
- First Implementation of Core Window Manager Constructs.
  - Pages: Similiar to a Desktop, each page has a Layout to arrange tiled windows.
  - Layouts: Two default layouts: `column` and `stack` layout.
  - Clients & Tabs: A vxwm client consists of one or more tabs.
  - Selection: A tab window can be toggle-selected for further operations.
  - Merge: All selected tabs are merged into the current focused client.
  - Split: All selected tabs are splitted into single-tabbed clients.

[v0.2.0]: https://github.com/x2w-sloth/vxwm/compare/v0.2.0-alpha...v0.2.0
[v0.2.0-alpha]: https://github.com/x2w-sloth/vxwm/compare/v0.1.3...v0.2.0-alpha
[v0.1.3]: https://github.com/x2w-sloth/vxwm/compare/v0.1.2...v0.1.3
[v0.1.2]: https://github.com/x2w-sloth/vxwm/compare/v0.1.1...v0.1.2
[v0.1.1]: https://github.com/x2w-sloth/vxwm/compare/v0.1.0...v0.1.1
[v0.1.0]: https://github.com/x2w-sloth/vxwm/releases/tag/v0.1.0