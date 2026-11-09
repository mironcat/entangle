# Preview rotation

This fork adds a rotation setting for the image display (live view preview
and captured images shown in Entangle).

## Usage

Open `Edit -> Preferences -> Image Viewer` and select a value in the
`Rotation` combo box: `0°`, `90°`, `180°` or `270°` (clockwise).

The button next to the live preview toggle in the header bar rotates the
preview 90° clockwise. The same action is available from the view menu
and with the keyboard:

* `Ctrl+R` - rotate clockwise
* `Ctrl+Shift+R` - rotate counterclockwise
* `Ctrl+Shift+0` - reset to 0°

Double-clicking a thumbnail in the session browser opens it with the
default application for that file type (the same application as the
"Open" item in the thumbnail context menu, i.e. the one configured in
the desktop's "Open With" settings).

The setting is stored in GSettings:

```sh
gsettings get org.entangle-photo.manager.img rotation
gsettings set org.entangle-photo.manager.img rotation 90
```

Only multiples of 90 degrees are supported. The value is normalized to
0, 90, 180 or 270.

The rotation affects the on-screen image only. Files on disk are not
modified; captured images still carry their normal EXIF orientation.

## Building

Dependencies (Arch/Manjaro names):

```sh
sudo pacman -S --needed meson ninja gcc pkgconf gobject-introspection \
    libpeas gtk3 libgphoto2 gexiv2 libraw lcms2 gstreamer \
    gst-plugins-base dbus-glib libgudev
```

Configure and build:

```sh
meson setup build -Denable-gtk-doc=false -Dintrospection=false
ninja -C build
```

`-Dintrospection=false` skips GObject introspection data, which is only
needed by third-party introspection-based plugins. `-Denable-gtk-doc=false`
skips the API documentation (and therefore the gtk-doc tools).

Run from the build directory:

```sh
meson devenv -C build ./src/entangle
```

## Implementation

* `src/org.entangle-photo.manager.gschema.xml`: new `rotation` key.
* `src/frontend/entangle-preferences.*`: read/write the setting.
* `src/frontend/entangle-preferences-display.*` and the `.ui` file:
  the `Rotation` combo box.
* `src/frontend/entangle-image-display.*`: swap the effective image
  dimensions for 90/270 and apply the rotation around the image centre
  in the cairo transform.
* `src/frontend/entangle-camera-manager.c`: apply the setting to the
  preview and react to changes.

See upstream issue <https://gitlab.com/entangle/entangle/-/issues/17>.
