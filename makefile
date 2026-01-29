.PHONY: clean wlr install uninstall

cc ?= gcc
pixman ?= /usr/include/pixman-1/
ffmpeg ?= /usr/lib/
wayland ?= /usr/lib/

compile_flags_general := -Wall -Wextra -Wpedantic -Werror

src := ./src/argparser.c ./src/wlr/screencopy.c ./src/select.c ./src/ffmpeg-converter.c
out := ./build/creenhot

wlr_screencopy := ./protocol/wlr-screencopy-unstable-v1.xml
xdg_shell := ./protocol/xdg-shell.xml

define wayscan_wlr_screencopy
	wayland-scanner client-header $(wlr_screencopy) ./build/wayscanned/wlr-screencopy-unstable-v1.h
	wayland-scanner private-code  $(wlr_screencopy) ./build/wayscanned/wlr-screencopy-unstable-v1.c
endef

define wayscan_xdg_shell
	wayland-scanner client-header $(xdg_shell) ./build/wayscanned/xdg-shell.h
	wayland-scanner private-code  $(xdg_shell) ./build/wayscanned/xdg-shell.c
endef

define makedirs
	mkdir build/
	mkdir build/wayscanned/
endef

clean: ./build
	rm -r ./build/

wlr_flags := -DWLR_USE_UNSTABLE
wlr_src := ./build/wayscanned/wlr-screencopy-unstable-v1.c ./build/wayscanned/xdg-shell.c 
wlr_src_main := ./src/wlr/creenhot.c 
wlr: $(pixman) $(src) $(wlr_src_main) $(wayland) $(ffmpeg) $(wlr_screencopy) $(xdg_shell) 
	$(makedirs)
	$(wayscan_wlr_screencopy)
	$(wayscan_xdg_shell)
	$(cc) -I$(pixman) $(wlr_flags) $(compile_flags_general) $(wlr_src) $(src) $(wlr_src_main) -L$(wayland) -lwayland-client -L$(ffmpeg) -lavutil -lavcodec -lswscale -o $(out)
	chmod 755 $(out)

wlr_debug:
	$(makedirs)
	$(wayscan_wlr_screencopy)
	$(wayscan_xdg_shell)
	$(cc) -g -I$(pixman) $(wlr_flags) $(compile_flags_general) $(wlr_src) $(src) $(wlr_src_main) -L$(wayland) -lwayland-client -L$(ffmpeg) -lavutil -lavcodec -lswscale -o $(out)
	chmod 755 $(out)

install: $(out)
	cp $(out) /usr/bin/creenhot

uninstall: /usr/bin/creenhot
	rm /usr/bin/creenhot
