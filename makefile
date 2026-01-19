.PHONE: clean wlr install uninstall

cc ?= gcc
pixman ?= /usr/include/pixman-1/

compile_flags_general := -Wall -Wextra -Wpedantic -Werror

src = ./src/argparser.c ./src/ffmpeg-converter.c
out = ./build/creenhot

define wayscan
	wayland-scanner client-header ./protocol/$(1).xml ./build/wayscanned/$(1).h
	wayland-scanner private-code ./protocol/$(1).xml ./build/wayscanned/$(1).c
endef

define makedirs
	mkdir build/
	mkdir build/wayscanned/
endef

clean: ./build
	rm -r ./build/

wlr: $(pixman) $(src)
	$(makedirs)
	$(call wayscan,wlr-screencopy-unstable-v1)
	$(call wayscan,xdg-shell)
	$(cc) -I$(pixman) -DWLR_USE_UNSTABLE $(compile_flags_general) ./build/wayscanned/wlr-screencopy-unstable-v1.c ./build/wayscanned/xdg-shell.c $(src) ./src/wlr/creenhot.c -lwayland-client -lavutil -lavcodec -lswscale -o $(out)
	chmod 755 $(out)

install: $(out)
	cp $(out) /usr/bin/creenhot

uninstall: /usr/bin/creenhot
	rm /usr/bin/creenhot
