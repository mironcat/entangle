# Camera control filtering

For Sony cameras libgphoto2's PTP driver exposes the `f-number` control as
a generic `f/1 ... f/64` radio list, regardless of the lens mounted on the
camera. Entangle now narrows this list down to the values the camera
actually accepts.

## How it works

The camera silently clamps an out-of-range aperture to the nearest value
supported by the lens. Entangle uses that behaviour:

1. It sets the first entry of the list (for example `f/1`) and reads the
   value back. The result is the widest aperture of the lens (`f/3.5` for
   the Sony E 30mm f/3.5 Macro).
2. It walks up from `f/22` until the value read back differs from the
   requested one. The last accepted value is the smallest aperture
   supported by the lens (`f/22` for the SEL30M35).
3. It restores the aperture the camera had before probing.

The resulting range is cached per camera serial in:

```
~/.cache/entangle/fnumber-<ID_SERIAL>
```

The file contains the minimum and maximum f-number, for example:

```
3.5 22
```

The first connection with a new lens takes a few seconds because the
probe has to move the aperture. Subsequent starts use the cache and load
the controls immediately.

## Resetting the cache

Delete the cache file and restart Entangle to probe the lens again, for
example after mounting a different lens:

```sh
rm ~/.cache/entangle/fnumber-*
```

## Manual focus

Sony cameras expose manual focus as a relative step range: the A7 III
reports `/main/actions/manualfocus` with a range of -7..+7. Entangle
shows a spring-loaded slider to the right of the live preview and a
small step field with `+` / `-` buttons:

* the slider is a continuous jog: press and hold it away from the
  centre and focus moves while it is held. The further from the centre,
  the faster it moves; a small deflection gives fine adjustment.
  Releasing the slider stops the lens and returns it to the centre;
* only one focus command is in flight at a time, so releasing the
  slider stops the lens immediately instead of draining a queue;
* the step field (1-7) sets the size of one command. It is used by the
  `+` / `-` buttons and by the mouse wheel; pressing Enter in the field
  applies one step;
* the mouse wheel moves one step per notch (smooth touchpad scrolling is
  accumulated), without moving the slider handle;
* the controls are enabled whenever the camera reports manual focus
  support, whether or not live preview is running.

The slider range is taken from the camera's manual focus widget, so a
camera with a different range gets a correctly sized slider.

PTP exposes manual focus as relative steps, not as an absolute focus
position: the A7 III does not return the Sony `Focal Position` property
(`0xD24C`). However, the camera writes `FocusPosition2` (0-255) and an
approximate distance into the EXIF of captured images, which can be
used to check the movement:

* on the test body one `+7` command moved `FocusPosition2` by about
  5-7 units;
* the useful range was roughly 80 (closest focus, about 0.09 m with the
  SEL30M35) to 255 (infinity), i.e. about 220 manual-focus units;
* therefore a single drag cannot cover the whole range, but holding
  the jog at full deflection traverses it in a few seconds.

The camera's own distance indicator is coarse (for example infinity,
0.7 m, 0.3 m); that does not limit the actual focus range.

## Save folder and filename prefix

Open the action menu and choose **Capture settings…**. The dialog has:

* **Save folder** — the directory where downloaded images are stored.
  The folder is created if it does not exist, and its contents are
  loaded into the session browser;
* **Filename prefix** — text placed before the automatically generated
  sequence number. For example, prefix `test_` and the camera's `.jpg`
  extension produce `test_000000.jpg`, `test_000001.jpg`, and so on.

The sequence number is six digits wide and restarts from the existing
files in the selected folder. The original camera file extension is
preserved. The prefix must not contain `/` or `X`, because `X` is the
sequence-number placeholder in the internal filename pattern.

The chosen folder and prefix are remembered in GSettings and used for
new sessions.

## Extending the filter

The same mechanism can be applied to other controls that libgphoto2
exposes with a generic list. The relevant code is in
`src/backend/entangle-camera.c`:

* `entangle_camera_parse_fnumber()` parses `f/3.5` style values;
* `entangle_camera_filter_fnumber_choices()` builds the filtered list;
* `entangle_camera_probe_fnumber_choices()` performs the probe and
  maintains the cache.
