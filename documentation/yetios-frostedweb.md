# YetiOS FrostedWeb Integration

Moonshine should be built with Wine's native Wayland driver enabled and should
be launched by FrostedWeb, not by FrostedGlass or an X11 session.

## Build Contract

Do not configure Moonshine with `--without-wayland`.

For the YetiOS desktop image, configure Moonshine with Wayland support present:

```sh
./configure --with-wayland
```

The driver lives in `dlls/winewayland.drv`. FrostedWeb depends on that driver
being available because the YetiOS shell path is:

```text
FrostedWeb Wayland socket -> Moonshine winewayland.drv -> explorer.exe
```

## Runtime Contract

FrostedWeb starts Moonshine with:

```text
WAYLAND_DISPLAY=<frostedweb socket>
DISPLAY unset
WINEWAYLAND=1
WINEDLLOVERRIDES=winex11.drv=d
```

FrostedWeb also imports its `moonshine-wayland.reg` preset, which sets
`HKCU\Software\Wine\Drivers\Graphics` to `wayland`.

Moonshine's Explorer desktop default is also set to `wayland`, so a fresh Wine
prefix does not choose X11 before FrostedWeb has applied the registry preset.
