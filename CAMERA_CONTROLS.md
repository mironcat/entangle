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
shows a spring-loaded slider to the right of the live preview:

* drag up/down to move focus; each unit of drag sends an individual
  relative `manualfocus` command, so a fast drag does not produce one
  large jump;
* releasing the slider returns it to the centre;
* the step field is a multiplier: 1 gives fine movement, 7 covers the
  focus range quickly. It is used by the slider and by the `+`/`-`
  buttons; pressing Enter in the field applies one step;
* the mouse wheel and clicks in the slider trough move by one unit;
* the slider is only enabled while live preview is running and the
  camera reports manual focus support.

The slider range is taken from the camera's manual focus widget, so a
camera with a different range gets a correctly sized slider.

Note that PTP exposes manual focus as relative steps, not as an absolute
focus position. The camera's own distance indicator is coarse (for
example infinity, 0.7 m, 0.3 m); that does not limit the actual focus
range, but reaching close focus may require several drags. Use a larger
step multiplier to move quickly and 1 for fine adjustment.

## Extending the filter

The same mechanism can be applied to other controls that libgphoto2
exposes with a generic list. The relevant code is in
`src/backend/entangle-camera.c`:

* `entangle_camera_parse_fnumber()` parses `f/3.5` style values;
* `entangle_camera_filter_fnumber_choices()` builds the filtered list;
* `entangle_camera_probe_fnumber_choices()` performs the probe and
  maintains the cache.
