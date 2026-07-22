#include "finlink/protocol.h"
#include "finlink/endian.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond)                                                      \
    do {                                                                 \
        if (!(cond)) {                                                   \
            fprintf(stderr, "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            exit(1);                                                     \
        }                                                                \
    } while (0)

static void test_peek_type(void) {
    finlink_msg_type type;

    CHECK(finlink_peek_type(NULL, 0, &type) == FINLINK_ERR_TOO_SHORT);

    const uint8_t video_byte[] = {1};
    CHECK(finlink_peek_type(video_byte, sizeof(video_byte), &type) == FINLINK_OK);
    CHECK(type == FINLINK_MSG_VIDEO);

    const uint8_t bogus_byte[] = {42};
    CHECK(finlink_peek_type(bogus_byte, sizeof(bogus_byte), &type) == FINLINK_ERR_UNKNOWN_TYPE);
}

static void test_video_header(void) {
    /* type=1, width=240, height=160, followed by 2 dummy compressed bytes */
    const uint8_t frame[] = {
        1,
        240, 0, 0, 0,
        160, 0, 0, 0,
        0xAA, 0xBB
    };

    finlink_video_header hdr;
    CHECK(finlink_parse_video_header(frame, sizeof(frame), &hdr) == FINLINK_OK);
    CHECK(hdr.width == 240);
    CHECK(hdr.height == 160);
    CHECK(hdr.compressed_size == 2);
    CHECK(hdr.compressed_data[0] == 0xAA);
    CHECK(hdr.compressed_data[1] == 0xBB);

    CHECK(finlink_parse_video_header(frame, 8, &hdr) == FINLINK_ERR_TOO_SHORT);
}

static void test_audio_frame(void) {
    /* type=3, sampleRate=32000, channels=2, 2 samples (4 bytes): -1, 1000 */
    const uint8_t frame[] = {
        3,
        0x00, 0x7D, 0x00, 0x00, /* 32000 LE */
        2,
        0xFF, 0xFF, /* -1 */
        0xE8, 0x03  /* 1000 */
    };

    finlink_audio_frame audio;
    CHECK(finlink_parse_audio_frame(frame, sizeof(frame), &audio) == FINLINK_OK);
    CHECK(audio.sample_rate == 32000);
    CHECK(audio.channels == 2);
    CHECK(audio.sample_count == 2);
    CHECK(finlink_read_s16le(audio.samples) == -1);
    CHECK(finlink_read_s16le(audio.samples + 2) == 1000);
}

static void test_build_input_frame(void) {
    uint8_t buf[FINLINK_INPUT_FRAME_SIZE];
    uint16_t mask = FINLINK_KEY_A | FINLINK_KEY_START;

    CHECK(finlink_build_input_frame(mask, buf) == FINLINK_INPUT_FRAME_SIZE);
    CHECK(buf[0] == FINLINK_MSG_INPUT);
    CHECK(finlink_read_u16le(buf + 1) == mask);
}

int main(void) {
    test_peek_type();
    test_video_header();
    test_audio_frame();
    test_build_input_frame();
    printf("protocol: all tests passed\n");
    return 0;
}
