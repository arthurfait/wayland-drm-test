/* 
 * Copyright © 2013 Intel Corporation
 * 
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that\n the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 */

#include <intel_bufmgr.h>
#include <stdlib.h>
#include <drm.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <cairo.h>

#include <wayland-client.h>
#include "wayland-drm-client-protocol.h"

static struct wl_drm *drm = NULL;
static struct wl_compositor *compositor = NULL;
static struct wl_shell *shell = NULL;
static struct wl_output *output = NULL;
static drm_intel_bufmgr *bufmgr = NULL;
int fd = 0;

static void
drm_handle_device(void *data, struct wl_drm *drm, const char *device)
{
	drm_magic_t magic;

	fd = open(device, O_RDWR | O_CLOEXEC);
	if (fd == -1) {
		fprintf(stderr, "could not open %s: %m\n", device);
		exit(-1);
	}

	drmGetMagic(fd, &magic);
	wl_drm_authenticate(drm, magic);
}

static void
drm_handle_format (void *data,
		   struct wl_drm *drm,
		   uint32_t format)
{
}

static void
drm_handle_authenticated (void *data, 
			  struct wl_drm *drm)
{
   bufmgr = drm_intel_bufmgr_gem_init (fd, 4096);
}

static const struct wl_drm_listener drm_listener = {
	drm_handle_device,
	drm_handle_format,
	drm_handle_authenticated
};

static void
registry_handle_global (void *data,
			struct wl_registry *registry,
			uint32_t name,
			const char *interface,
			uint32_t version)
{
	if (strcmp (interface, "wl_compositor") == 0)
	{
		compositor = wl_registry_bind (registry,
					       name,
					       &wl_compositor_interface,
					       1);
	} else if (strcmp (interface, "wl_drm") == 0)
	{
		drm = wl_registry_bind (registry,
					name,
					&wl_drm_interface,
					1);
		wl_drm_add_listener (drm, &drm_listener, NULL); 
	} else if (strcmp (interface, "wl_shell") == 0)
	{
		shell = wl_registry_bind (registry,
					  name,
					  &wl_shell_interface,
					  1);
	} else if (strcmp (interface, "wl_output") == 0)
	{
		output = wl_registry_bind (registry,
					   name,
					   &wl_output_interface,
					   1);
	}

}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	NULL,
};

int
main (int argc, char **argv)
{
	drm_intel_bo *bo;
	int pitch, height, width;	
	int i;
	uint32_t name;
	struct wl_display *display;
	struct wl_registry *registry;
	int ret;
	uint32_t *p;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_buffer *buffer;
	cairo_surface_t *cairo_surface;
	unsigned char *data;

	if (argc < 2)
		exit (-1);

	cairo_surface = cairo_image_surface_create_from_png (argv[1]);

	if (!cairo_surface)
		exit (-1);

	display = wl_display_connect (NULL);
	registry = wl_display_get_registry (display);
	wl_registry_add_listener (registry,
				  &registry_listener,
				  NULL);
	while (!drm && !compositor && !shell)
		wl_display_dispatch (display);

	surface = wl_compositor_create_surface (compositor);
	shell_surface = wl_shell_get_shell_surface (shell, surface);
	wl_shell_surface_set_toplevel (shell_surface);

	while (!bufmgr)
		wl_display_dispatch (display);

	bufmgr = drm_intel_bufmgr_gem_init (fd, 4096);
	pitch = cairo_image_surface_get_stride (cairo_surface);
	width = cairo_image_surface_get_width (cairo_surface);
	height = cairo_image_surface_get_height (cairo_surface);
	data = cairo_image_surface_get_data (cairo_surface);

	bo = drm_intel_bo_alloc (bufmgr,
				 "wl-drm-test",
				 pitch * height,
				 4096);
	drm_intel_bo_map (bo, 1);

	memcpy (bo->virtual, data, pitch * height);
	drm_intel_bo_unmap (bo);

	drm_intel_bo_flink (bo, &name);

	buffer = wl_drm_create_buffer (drm,
				       name,
				       1280,
				       720,
				       width,
				       height,
				       pitch,
				       WL_DRM_FORMAT_XRGB8888,
				       WL_DRM_FLAGS_S3D_FP);
	wl_surface_attach (surface, buffer, 0, 0);
	wl_surface_damage (surface, 0, 0, width, height);
	wl_shell_surface_set_fullscreen (shell_surface,
					 WL_SHELL_SURFACE_FULLSCREEN_METHOD_DRIVER,
					 0,
					 output);
	wl_surface_commit (surface);
 
	while (ret != -1)
	{
		ret = wl_display_dispatch (display);
	}
}
