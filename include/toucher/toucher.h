#ifndef TOUCHER_TOUCHER_H
#define TOUCHER_TOUCHER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct toucher_client toucher_client;

enum toucher_pointer_type {
	TOUCHER_POINTER_MOVE = 1,
	TOUCHER_POINTER_DOWN,
	TOUCHER_POINTER_UP,
	TOUCHER_POINTER_RESET,
	TOUCHER_POINTER_CANCEL,
};

enum toucher_surface_transform {
	TOUCHER_TRANSFORM_IDENTITY = 0,
	TOUCHER_TRANSFORM_ROTATE_90,
	TOUCHER_TRANSFORM_ROTATE_180,
	TOUCHER_TRANSFORM_ROTATE_270,
	TOUCHER_TRANSFORM_MIRROR_HORIZONTAL,
	TOUCHER_TRANSFORM_MIRROR_HORIZONTAL_ROTATE_90,
	TOUCHER_TRANSFORM_MIRROR_HORIZONTAL_ROTATE_180,
	TOUCHER_TRANSFORM_MIRROR_HORIZONTAL_ROTATE_270,
};

struct toucher_options {
	uint32_t selected_device;
	uint32_t selected_generation;
	uint32_t flags;
	uint32_t reserved;
};

struct toucher_surface_info {
	uint32_t display_width;
	uint32_t display_height;
	int32_t origin_x;
	int32_t origin_y;
	uint32_t width;
	uint32_t height;
	uint32_t transform;
	uint32_t flags;
};

struct toucher_device_info {
	uint32_t id;
	uint32_t generation;
	uint32_t match_quality;
	uint32_t flags;
	int32_t x_min;
	int32_t x_max;
	int32_t y_min;
	int32_t y_max;
	int32_t slot_min;
	int32_t slot_max;
	char name[64];
};

struct toucher_pointer_event {
	uint64_t timestamp_ns;
	float x;
	float y;
	uint32_t type;
	uint32_t button;
};

struct toucher_runtime_stats {
	uint64_t input_received;
	uint64_t input_written;
	uint64_t input_ignored;
	uint64_t ring_overwritten;
	uint64_t events_dropped;
	uint64_t device_connected;
	uint64_t device_disconnected;
	uint64_t device_failed;
	uint64_t rpc_accepted;
	uint64_t rpc_rejected;
	uint64_t rpc_invalid_address;
	uint64_t callback_active_high;
	uint64_t rpc_active_high;
	uint32_t device_count;
};

enum toucher_region_type {
	TOUCHER_REGION_RECT = 1,
	TOUCHER_REGION_ELLIPSE,
};

enum toucher_capture_state {
	TOUCHER_CAPTURE_STATE_OWNED = 1U << 0,
	TOUCHER_CAPTURE_STATE_DRAINING = 1U << 1,
	TOUCHER_CAPTURE_STATE_STOPPING = 1U << 2,
	TOUCHER_CAPTURE_STATE_READY = 1U << 3,
};

struct toucher_region {
	uint32_t type;
	float left;
	float top;
	float right;
	float bottom;
};

struct toucher_capture_event {
	uint64_t timestamp_ns;
	uint64_t sequence_id;
	uint64_t config_id;
	uint32_t device_id;
	uint32_t generation;
	uint32_t type;
	float x;
	float y;
};

struct toucher_capture_status {
	uint64_t epoch;
	uint64_t config_id;
	uint64_t sequence_id;
	uint64_t deadline_ns;
	uint32_t state;
	uint32_t lease_ms;
};

int toucher_create(const struct toucher_options *options,
	toucher_client **out_client);
int toucher_refresh_devices(toucher_client *client);
int toucher_get_devices(const toucher_client *client,
	struct toucher_device_info *devices, uint32_t capacity,
	uint32_t *count);
int toucher_select_device(toucher_client *client,
	uint32_t id, uint32_t generation);
int toucher_set_surface(toucher_client *client,
	const struct toucher_surface_info *surface);
int toucher_poll(toucher_client *client,
	struct toucher_pointer_event *events, uint32_t capacity,
	uint32_t *count);
int toucher_get_stats(toucher_client *client,
	struct toucher_runtime_stats *stats);
void toucher_reset(toucher_client *client);
void toucher_destroy(toucher_client *client);

int toucher_capture_acquire(toucher_client *client, uint32_t lease_ms);
int toucher_capture_set_regions(toucher_client *client,
	const struct toucher_region *regions, uint32_t count);
int toucher_capture_poll(toucher_client *client,
	struct toucher_capture_event *events, uint32_t capacity, uint32_t *count);
int toucher_capture_keepalive(toucher_client *client);
int toucher_capture_release(toucher_client *client);
int toucher_capture_get_status(toucher_client *client,
	struct toucher_capture_status *status);
int toucher_prepare_unload(toucher_client *client,
	struct toucher_capture_status *status);

#ifdef __cplusplus
}
#endif

#endif
