# Introduction

This is a copy of gstrfbsrc.c from
[gst-plugins-bad](https://gitlab.freedesktop.org/gstreamer/gst-plugins-bad),
but using [libvncclient](https://libvnc.github.io/) instead of their
custom rfbdecoder, which seems to result in better behavior with some
servers.

# Building

The supported way to build this is via the
[Nix](https://nixos.org/nix) package manager, through the
[nix-remarkable](https://github.com/peter-sa/nix-remarkable)
expressions. To build just this project via `nix build` from this
repo, download it into the `pkgs/` directory of `nix-remarkable`.

For other systems, the commands needed to compile and link are
relatively simple, and given in [derivation.nix](./derivation.nix).

# Usage

This module provides an `rfbsrc` GStreamer element. For example, if
the produced `libgstrfbsrc.so` is in `result/lib`, a VNC server from
localhost could be rotated right 90 degrees and connected to
video-conferencing software expecting 1280x720 YUV420 video on a
[v4l2loopback](https://github.com/umlaeute/v4l2loopback) device
`/dev/video0` via the command:

    GST_PLUGIN_PATH_1_0=result/lib gst-launch-1.0 rfbsrc host=127.0.0.1,port=5900 ! videoconvert ! videoflip video-direction=90r ! videoscale ! video/x-raw,format=I420,width=1280,height=720,pixel-aspect-ratio=1/1 ! queue ! v4l2sink device=/dev/video0
