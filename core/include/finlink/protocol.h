#ifndef FINLINK_PROTOCOL_H
#define FINLINK_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

/* Pure message (de)serialization for the wire protocol in docs/protocol.md.
 * No I/O, no allocation: these functions only view into / write to buffers
 * the caller owns. The actual WebSocket transport (handshake, frame masking,
 * socket I/O) is platform-specific and lives in clients/<platform>/. */

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FINLINK_MSG_VIDEO = 1,
    FINLINK_MSG_INPUT = 2,
    FINLINK_MSG_AUDIO = 3,
} finlink_msg_type;

typedef enum {
    FINLINK_OK = 0,
    FINLINK_ERR_TOO_SHORT = -1,   /* buffer smaller than the message's fixed header */
    FINLINK_ERR_UNKNOWN_TYPE = -2 /* leading byte isn't a known finlink_msg_type */
} finlink_result;

/* Bit positions within the type=2 input keyBitmask, per docs/protocol.md. */
typedef enum {
    FINLINK_KEY_A = 1 << 0,
    FINLINK_KEY_B = 1 << 1,
    FINLINK_KEY_SELECT = 1 << 2,
    FINLINK_KEY_START = 1 << 3,
    FINLINK_KEY_RIGHT = 1 << 4,
    FINLINK_KEY_LEFT = 1 << 5,
    FINLINK_KEY_UP = 1 << 6,
    FINLINK_KEY_DOWN = 1 << 7,
    FINLINK_KEY_R = 1 << 8,
    FINLINK_KEY_L = 1 << 9
} finlink_key;

/* Video header (type=1). compressed_data points into the caller's buffer
 * (no copy) and is raw-deflate compressed RGB565, width*height*2 bytes once
 * inflated. Decompress with finlink_inflate_raw(). */
typedef struct {
    uint32_t width;
    uint32_t height;
    const uint8_t *compressed_data;
    size_t compressed_size;
} finlink_video_header;

/* Audio frame (type=3). samples points into the caller's buffer (no copy),
 * sample_count is the total number of s16 samples (i.e. frames * channels). */
typedef struct {
    uint32_t sample_rate;
    uint8_t channels;
    const uint8_t *samples; /* s16le, read with finlink_read_s16le() */
    size_t sample_count;
} finlink_audio_frame;

#define FINLINK_INPUT_FRAME_SIZE 3

/* Reads the leading type byte of a server->client message without consuming
 * the rest. `size` must be >= 1. */
finlink_result finlink_peek_type(const uint8_t *data, size_t size, finlink_msg_type *out_type);

/* Parses a type=1 message. `data` must start at the type byte. */
finlink_result finlink_parse_video_header(const uint8_t *data, size_t size, finlink_video_header *out);

/* Parses a type=3 message. `data` must start at the type byte. */
finlink_result finlink_parse_audio_frame(const uint8_t *data, size_t size, finlink_audio_frame *out);

/* Writes a type=2 message into out_buf (must have room for
 * FINLINK_INPUT_FRAME_SIZE bytes). Returns the number of bytes written. */
size_t finlink_build_input_frame(uint16_t key_bitmask, uint8_t out_buf[FINLINK_INPUT_FRAME_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* FINLINK_PROTOCOL_H */
