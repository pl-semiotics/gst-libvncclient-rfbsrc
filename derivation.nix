{ stdenv, lib, gst_all_1, glib, libvncserver }:

stdenv.mkDerivation rec {
  pname = "gst-libvncclient-rfbsrc";
  version = "0.0.1";
  buildInputs = [
    gst_all_1.gstreamer.dev
    gst_all_1.gst-plugins-base
    libvncserver
  ];
  src = lib.cleanSource ./.;
  NIX_CFLAGS_COMPILE = [
    "-I${glib.dev}/include/glib-2.0"
    "-I${glib.out}/lib/glib-2.0/include"
    "-I${gst_all_1.gstreamer.dev}/include/gstreamer-1.0"
    "-I${gst_all_1.gst-plugins-base.dev}/include/gstreamer-1.0"
    "-DVERSION=\"${version}\""
    "-DPACKAGE=\"gst-libvncclient-rfbsrc\""
    "-DGST_LICENSE=\"LGPL\""
    "-DGST_PACKAGE_NAME=\"gst-libvncclient-rfbsrc\""
    "-DGST_PACKAGE_ORIGIN=\"https://github.com/peter-sa/gst-libvncclient-rfbsrc\""
  ];
  buildPhase = ''
    $CC -c gstrfbsrc.c -o gstrfbsrc.o
    $CC -o libgstrfbsrc.so -shared gstrfbsrc.o -Wl,--as-needed -Wl,--no-undefined -shared -fPIC -Wl,--start-group -lgstbase-1.0 -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lgstvideo-1.0 -lgio-2.0 -lvncclient -Wl,--end-group
  '';
  installPhase = ''
    mkdir -p $out/lib
    cp libgstrfbsrc.so $out/lib
  '';
}
