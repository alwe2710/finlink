#include "finlink/protocol.h"
#include "finlink/endian.h"

#define VIDEO_HEADER_SIZE 9 /* type(1) + width(4) + height(4) */
#define AUDIO_HEADER_SIZE 6 /* type(1) + sampleRate(4) + channels(1) */

finlink_result finlink_peek_type(const uint8_t *data, size_t size, finlink_msg_type *out_type) {
    if (size < 1) {
        return FINLINK_ERR_TOO_SHORT;
    }

    switch (data[0]) {
        case FINLINK_MSG_VIDEO:
        case FINLINK_MSG_INPUT:
        case FINLINK_MSG_AUDIO:
            *out_type = (finlink_msg_type)data[0];
            return FINLINK_OK;
        default:
            return FINLINK_ERR_UNKNOWN_TYPE;
    }
}

finlink_result finlink_parse_video_header(const uint8_t *data, size_t size, finlink_video_header *out) {
    if (size < VIDEO_HEADER_SIZE) {
        return FINLINK_ERR_TOO_SHORT;
    }
    if (data[0] != FINLINK_MSG_VIDEO) {
        return FINLINK_ERR_UNKNOWN_TYPE;
    }

    out->width = finlink_read_u32le(data + 1);
    out->height = finlink_read_u32le(data + 5);
    out->compressed_data = data + VIDEO_HEADER_SIZE;
    out->compressed_size = size - VIDEO_HEADER_SIZE;
    return FINLINK_OK;
}

finlink_result finlink_parse_audio_frame(const uint8_t *data, size_t size, finlink_audio_frame *out) {
    if (size < AUDIO_HEADER_SIZE) {
        return FINLINK_ERR_TOO_SHORT;
    }
    if (data[0] != FINLINK_MSG_AUDIO) {
        return FINLINK_ERR_UNKNOWN_TYPE;
    }

    out->sample_rate = finlink_read_u32le(data + 1);
    out->channels = data[5];
    out->samples = data + AUDIO_HEADER_SIZE;
    out->sample_count = (size - AUDIO_HEADER_SIZE) / sizeof(int16_t);
    return FINLINK_OK;
}

size_t finlink_build_input_frame(uint16_t key_bitmask, uint8_t out_buf[FINLINK_INPUT_FRAME_SIZE]) {
    out_buf[0] = FINLINK_MSG_INPUT;
    finlink_write_u16le(out_buf + 1, key_bitmask);
    return FINLINK_INPUT_FRAME_SIZE;
}
