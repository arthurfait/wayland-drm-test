wl-drm-test: wl-drm-test.c wayland-drm-client-protocol.h wayland-drm-protocol.c
	gcc -O0 -g3 -o wl-drm-test `pkg-config --cflags --libs wayland-client libdrm_intel` $^
clean:
	rm wl-drm-test
