About
-----
creenhot v0.1

A custom screenshot utility tool for wayland compositors.

Currently implemented for wlroots based compositors.

License
-------
Licensed under the [MIT License][licenselink]

Installing
----------
Make sure you see [Dependencies][dependslink] before building

Use the following commands:
```
git clone https://github.com/ramak1618/creenhot
cd creenhot
make wlr
sudo make install
```

Dependencies
------------
-- wayland-scanner (build-time)

-- wayland-client

-- libavutil

-- libavcodec

-- libswscale

Environment
-----------
A wlroots environment that supports `wlr_screencopy` is assumed.


Example Usage:
-------------
```
creenhot shot.png
```
Creates a file called shot.png, which is the full desktop screenshot.

Note that only PNG file format is supported as of now.

```
creenhot shot.png x y width height
```
Creates a file called shot.png, which takes a rectangular snap of given `width` and `height` with top-left corner as `(x, y)`

[licenselink]: LICENSE.md
[dependslink]: #dependencies
