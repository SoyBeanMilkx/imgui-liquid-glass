#ifndef TOUCHER_ABI_H
#define TOUCHER_ABI_H

#if defined(__KERNEL__)
#include <ktypes.h>
#define TOUCHER_ABI_ASSERT(condition, message) _Static_assert(condition, message)
#else
#include <stddef.h>
#include <stdint.h>
#if defined(__cplusplus)
#define TOUCHER_ABI_ASSERT(condition, message) static_assert(condition, message)
#else
#define TOUCHER_ABI_ASSERT(condition, message) _Static_assert(condition, message)
#endif
#endif

#define TOUCHER_MAGIC 0x544f5543U
#define TOUCHER_PRCTL_RPC 0x54554348ULL
#define TOUCHER_ABI_MAJOR 2U
#define TOUCHER_ABI_MINOR 1U

#define TOUCHER_RING_CAPACITY 4096U
#define TOUCHER_READ_BATCH_MAX 128U
#define TOUCHER_DEVICE_MAX 16U
#define TOUCHER_RPC_MAX 16384U
#define TOUCHER_DEVICE_NAME_MAX 64U

#define TOUCHER_FEATURE_TYPE_B (1ULL << 0)
#define TOUCHER_FEATURE_TYPE_A (1ULL << 1)
#define TOUCHER_FEATURE_SINGLE_TOUCH (1ULL << 2)
#define TOUCHER_FEATURE_STATELESS (1ULL << 3)
#define TOUCHER_FEATURE_INPUT_HOOK (1ULL << 4)
#define TOUCHER_FEATURE_EVDEV_HOOK (1ULL << 5)
#define TOUCHER_FEATURE_CAPTURE (1ULL << 6)

#define TOUCHER_REGION_MAX 32U
#define TOUCHER_CAPTURE_RING_CAPACITY 512U
#define TOUCHER_COORD_MAX 65535
#define TOUCHER_LEASE_DEFAULT_MS 2000U
#define TOUCHER_LEASE_MIN_MS 250U
#define TOUCHER_LEASE_MAX_MS 60000U
#define TOUCHER_CAPTURE_CANCEL_CURRENT (1U << 0)
#define TOUCHER_CAPTURE_OWNED (1U << 0)
#define TOUCHER_CAPTURE_DRAINING (1U << 1)
#define TOUCHER_CAPTURE_STOPPING (1U << 2)
#define TOUCHER_CAPTURE_READY (1U << 3)

#define TOUCHER_DEVICE_DIRECT (1U << 0)
#define TOUCHER_DEVICE_TYPE_B (1U << 1)
#define TOUCHER_DEVICE_TYPE_A (1U << 2)
#define TOUCHER_DEVICE_SINGLE_TOUCH (1U << 3)
#define TOUCHER_DEVICE_MAPPABLE (1U << 4)
#define TOUCHER_DEVICE_CAPTURABLE (1U << 5)

#define TOUCHER_READ_OVERFLOW (1U << 0)

enum toucher_operation {
	TOUCHER_OP_QUERY_CAPS = 1,
	TOUCHER_OP_LIST_DEVICES,
	TOUCHER_OP_READ_EVENTS,
	TOUCHER_OP_GET_STATS,
	TOUCHER_OP_CAPTURE_ACQUIRE,
	TOUCHER_OP_CAPTURE_CONFIGURE,
	TOUCHER_OP_CAPTURE_READ,
	TOUCHER_OP_CAPTURE_KEEPALIVE,
	TOUCHER_OP_CAPTURE_RELEASE,
	TOUCHER_OP_CAPTURE_STATUS,
	TOUCHER_OP_PREPARE_UNLOAD,
};

enum toucher_event_kind {
	TOUCHER_EVENT_INPUT = 1,
	TOUCHER_EVENT_DEVICE_ADDED,
	TOUCHER_EVENT_DEVICE_REMOVED,
	TOUCHER_EVENT_STATE_RESET,
};

struct toucher_request_header {
	uint32_t magic;
	uint16_t abi_major;
	uint16_t abi_minor;
	uint16_t operation;
	uint16_t flags;
	uint32_t struct_size;
};

struct toucher_response_header {
	uint32_t magic;
	uint16_t abi_major;
	uint16_t abi_minor;
	int32_t status;
	uint32_t struct_size;
};

struct toucher_wire_event {
	uint64_t sequence;
	uint64_t timestamp_ns;
	uint32_t device_id;
	uint32_t generation;
	uint16_t kind;
	uint16_t input_type;
	uint16_t input_code;
	uint16_t flags;
	int32_t value;
	uint32_t frame_id;
};

struct toucher_wire_device {
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
	uint64_t name_hash;
	char name[TOUCHER_DEVICE_NAME_MAX];
};

struct toucher_query_caps_request {
	struct toucher_request_header hdr;
};

struct toucher_query_caps_response {
	struct toucher_response_header hdr;
	uint64_t features;
	uint64_t layout_hash;
	uint64_t latest_cursor;
	uint32_t kernel_version;
	uint32_t life_state;
	uint32_t ring_capacity;
	uint32_t read_batch_max;
	uint32_t device_max;
	uint32_t reserved;
};

struct toucher_list_devices_request {
	struct toucher_request_header hdr;
	uint32_t capacity;
	uint32_t reserved;
};

struct toucher_list_devices_response {
	struct toucher_response_header hdr;
	uint32_t total_count;
	uint32_t copied_count;
	struct toucher_wire_device devices[];
};

struct toucher_read_events_request {
	struct toucher_request_header hdr;
	uint64_t cursor;
	uint32_t device_id;
	uint32_t generation;
	uint32_t capacity;
	uint32_t reserved;
};

struct toucher_read_events_response {
	struct toucher_response_header hdr;
	uint64_t oldest_cursor;
	uint64_t next_cursor;
	uint64_t latest_cursor;
	uint32_t count;
	uint32_t dropped;
	uint32_t flags;
	uint32_t reserved;
	struct toucher_wire_event events[];
};

struct toucher_get_stats_request {
	struct toucher_request_header hdr;
};

struct toucher_get_stats_response {
	struct toucher_response_header hdr;
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
	uint32_t reserved;
};

TOUCHER_ABI_ASSERT(sizeof(struct toucher_request_header) == 16,
	"request header ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_response_header) == 16,
	"response header ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_wire_event) == 40,
	"event ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_wire_device) == 112,
	"device ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_query_caps_response) == 64,
	"caps response ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_list_devices_request) == 24,
	"list request ABI drift");
TOUCHER_ABI_ASSERT(offsetof(struct toucher_list_devices_response, devices) == 24,
	"list response ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_read_events_request) == 40,
	"read request ABI drift");
TOUCHER_ABI_ASSERT(offsetof(struct toucher_read_events_response, events) == 56,
	"read response ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_get_stats_response) == 128,
	"stats response ABI drift");

enum toucher_wire_region_type {
	TOUCHER_WIRE_RECT = 1,
	TOUCHER_WIRE_ELLIPSE,
};

enum toucher_wire_pointer_type {
	TOUCHER_WIRE_MOVE = 1,
	TOUCHER_WIRE_DOWN,
	TOUCHER_WIRE_UP,
	TOUCHER_WIRE_CANCEL,
};

struct toucher_wire_region {
	uint32_t type;
	uint32_t reserved;
	int32_t left;
	int32_t top;
	int32_t right;
	int32_t bottom;
};

struct toucher_capture_request {
	struct toucher_request_header hdr;
	uint64_t token;
	uint64_t epoch;
	uint64_t cursor;
	uint64_t config_id;
	uint32_t device_id;
	uint32_t generation;
	uint32_t lease_ms;
	uint32_t flags;
	uint32_t region_count;
	uint32_t capacity;
	struct toucher_wire_region regions[TOUCHER_REGION_MAX];
};

struct toucher_wire_capture_event {
	uint64_t cursor;
	uint64_t timestamp_ns;
	uint64_t sequence_id;
	uint64_t config_id;
	uint32_t device_id;
	uint32_t generation;
	uint32_t type;
	int32_t x;
	int32_t y;
	uint32_t reserved;
};

struct toucher_capture_response {
	struct toucher_response_header hdr;
	uint64_t token;
	uint64_t epoch;
	uint64_t cursor;
	uint64_t config_id;
	uint64_t sequence_id;
	uint64_t deadline_ns;
	uint32_t state;
	uint32_t count;
	uint32_t flags;
	uint32_t lease_ms;
	struct toucher_wire_capture_event events[TOUCHER_READ_BATCH_MAX];
};

TOUCHER_ABI_ASSERT(sizeof(struct toucher_wire_region) == 24,
	"region ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_capture_request) == 840,
	"capture request ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_wire_capture_event) == 56,
	"capture event ABI drift");
TOUCHER_ABI_ASSERT(offsetof(struct toucher_capture_response, events) == 80,
	"capture response ABI drift");
TOUCHER_ABI_ASSERT(sizeof(struct toucher_capture_response) <= TOUCHER_RPC_MAX,
	"capture response exceeds RPC limit");

#undef TOUCHER_ABI_ASSERT

#endif
