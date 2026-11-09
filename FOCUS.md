# Live preview autofocus

This fork adds two ways to focus the camera while live preview is active:

* an **Autofocus** toolbar button (also `a`, and the hamburger menu item),
  which emulates a half-press of the shutter release;
* **click-to-focus** on the preview image, which moves the focus point to
  the clicked position and triggers autofocus there.

## Usage

Start live preview, then either:

* press the autofocus button (the frame icon next to the rotate button),
  press `a`, or use *Autofocus* in the menu;
* click anywhere on the preview image.

The red focus point overlay shows the selected focus position. It is
controlled by the *Focus point* option in
`Edit -> Preferences -> Image Viewer` (enabled by default).

Click-to-focus only works while live preview is running and only when the
camera exposes a movable focus point. It is ignored for previously
captured images.

## Sony behaviour

Sony cameras expose the shutter half-press as
`/main/actions/autofocus` and the focus point as
`/main/actions/spotfocusarea`, using a `x,y` text value in a 640x480
grid. Sony only honours the position in one of the Flexible Spot focus
area modes, so Entangle switches `/main/capturesettings/focusarea` to a
Flexible Spot mode automatically when needed.

The click position is mapped through the same transform used to draw
the preview, so it accounts for scaling, letterboxing, 90/180/270 degree
preview rotation and horizontal/vertical flips.

Nikon, Canon and Fuji cameras keep using the existing
`/main/actions/autofocusdrive` control for the toolbar button. They do
not currently support click-to-focus.

## Implementation

* `src/backend/entangle-camera.c/.h`: Sony half-press support,
  `entangle_camera_set_focus_area[_async/_finish]()`,
  `has-autofocus`/`has-focus-area` capability queries.
* `src/frontend/entangle-image-display.c/.h`: preview-to-widget matrix
  tracking, mouse click handling, `focus-point-selected` signal and
  focus point overlay position.
* `src/frontend/entangle-camera-manager.c`: `win.focus` action,
  autofocus buttons, focus point signal handling and the 640x480
  coordinate conversion.
* `help/C/camera-focusing.page`: user documentation.

## Debugging

Run Entangle with application debugging enabled to see the click
mapping and camera operations:

```sh
~/.local/bin/entangle-rotation --debug-entangle --debug-gphoto
```

The image display logs lines such as:

```
Focus click at 512.0,341.0 -> 512.0,341.0 of 1024x680 (0.500,0.501)
```

and the camera manager logs the converted 640x480 coordinates.
