#include <assert.h>
#include <stdint.h>
#include "Rtsp.h"

static int parse(const char* input, int length) {
    RTSP_MESSAGE message = {0};
    int result = parseRtspMessage(&message, (char*)input, length);
    if (result == RTSP_ERROR_SUCCESS) freeMessage(&message);
    return result;
}

int main(void) {
    const char* valid = "RTSP/1.0 200 OK\r\nCSeq: 1\r\nSession: abc;timeout=90\r\n\r\nbody";
    RTSP_MESSAGE message = {0};
    assert(parseRtspMessage(&message, (char*)valid, (int)strlen(valid)) == RTSP_ERROR_SUCCESS);
    assert(message.type == TYPE_RESPONSE && message.message.response.statusCode == 200);
    assert(message.sequenceNumber == 1);
    assert(strcmp(getOptionContent(message.options, "Session"), "abc;timeout=90") == 0);
    assert(message.payloadLength == 4 && memcmp(message.payload, "body", 4) == 0);
    freeMessage(&message);

    const char* truncated = "RTSP/1.0 200 OK\r\nCSeq: 1";
    assert(parse(truncated, (int)strlen(truncated)) == RTSP_ERROR_MALFORMED);
    assert(parse(valid, -1) == RTSP_ERROR_MALFORMED);
    assert(parse(NULL, 1) == RTSP_ERROR_MALFORMED);
    assert(parseRtspMessage(NULL, (char*)valid, (int)strlen(valid)) == RTSP_ERROR_MALFORMED);

    const char* cases[] = {
        valid,
        "RTSP/1.0 200 OK\r\nSession: ;;;\r\n\r\n",
        "RTSP/1.0 200 OK\r\nContent-Length: 20\r\n\r\nshort",
        "OPTIONS rtsp://127.0.0.1 RTSP/1.0\r\nCSeq: 1\r\n\r\n"
    };
    for (unsigned int m = 0; m < sizeof(cases) / sizeof(cases[0]); m++) {
        for (int n = 0; n <= (int)strlen(cases[m]); n++) parse(cases[m], n);
    }

    uint32_t state = 0x6a62u;
    char buffer[512];
    for (int i = 0; i < 20000; i++) {
        int length = i % (int)sizeof(buffer);
        for (int j = 0; j < length; j++) {
            state = state * 1664525u + 1013904223u;
            buffer[j] = (char)(state >> 24);
        }
        parse(buffer, length);
    }
    puts("PASS: RTSP response/payload, truncated headers, invalid arguments and 20,000 malformed inputs");
    return 0;
}
