#include "Limelight-internal.h"
#undef ListenerCallbacks
#include <stdio.h>
#include <string.h>
// Platform.h sets NDEBUG for production code; keep test assertions active.
#undef NDEBUG
#include <assert.h>

static int delivered;
static const char expected[] = "abcdef";
static void received(const LI_CLIPBOARD_ITEM* item) {
    assert(item->length == 0 || item->length == sizeof(expected) - 1);
    if (item->length != 0) assert(memcmp(item->data, expected, item->length) == 0);
    delivered++;
}
static void ignoreLog(const char* format, ...) { (void)format; }

static BYTE_BUFFER begin(char* packet, uint8_t kind) {
    BYTE_BUFFER bb;
    BbInitializeWrappedBuffer(&bb, packet, 0, 256, BYTE_ORDER_LITTLE);
    BbPut16(&bb, 0); // The dispatcher has already selected the clipboard packet type.
    BbPut8(&bb, kind);
    return bb;
}
static void start(PML_CONTROL_STREAM_CONTEXT ctx, uint8_t type, uint32_t length) {
    char packet[256]; BYTE_BUFFER bb = begin(packet, LI_CLIPBOARD_MSG_ITEM_START);
    BbPut8(&bb, 0); BbPut8(&bb, type); BbPut8(&bb, 0);
    BbPut64(&bb, 1); BbPut64(&bb, 0); BbPut32(&bb, length);
    BbPut16(&bb, 0); BbPut16(&bb, 0);
    processClipboardPacketCtx(ctx, packet, 30);
    assert(ctx->incomingClipboardTransfer.active);
}
static void chunk(PML_CONTROL_STREAM_CONTEXT ctx, uint32_t offset, uint16_t length, bool truncated) {
    char packet[256]; BYTE_BUFFER bb = begin(packet, LI_CLIPBOARD_MSG_ITEM_CHUNK);
    BbPut8(&bb, 0); BbPut16(&bb, length); BbPut64(&bb, 1); BbPut32(&bb, offset);
    if (length != 0) BbPutBytes(&bb, (const uint8_t*)expected + (offset < 6 ? offset : 0), length);
    processClipboardPacketCtx(ctx, packet, 18 + (truncated ? 0 : length));
}
static void end(PML_CONTROL_STREAM_CONTEXT ctx) {
    char packet[256]; BYTE_BUFFER bb = begin(packet, LI_CLIPBOARD_MSG_ITEM_END);
    BbPut64(&bb, 1); processClipboardPacketCtx(ctx, packet, 11);
}
int main(void) {
    ML_CONNECTION_CONTEXT connection = {0};
    ML_CONTROL_STREAM_CONTEXT ctx = {0};
    connection.ListenerCallbacks.clipboardItemReceived = received;
    connection.ListenerCallbacks.logMessage = ignoreLog;
    ctx.connectionContext = &connection;
    LiSetThreadConnectionContext(&connection);

    // A last chunk alone must never expose the uninitialized prefix to the callback.
    start(&ctx, LI_CLIPBOARD_ITEM_TYPE_TEXT, 6);
    chunk(&ctx, 3, 3, false);
    end(&ctx);
    assert(delivered == 0 && !ctx.incomingClipboardTransfer.active);
    for (uint8_t type = LI_CLIPBOARD_ITEM_TYPE_TEXT; type <= LI_CLIPBOARD_ITEM_TYPE_IMAGE; type++) {
        start(&ctx, type, 6); chunk(&ctx, 0, 3, false); chunk(&ctx, 3, 3, false); end(&ctx);
    }
    assert(delivered == 2);
    start(&ctx, LI_CLIPBOARD_ITEM_TYPE_TEXT, 6); chunk(&ctx, 0, 3, false);
    chunk(&ctx, 0, 3, false); end(&ctx); // Duplicate/overlapping data is invalid on this ordered channel.
    assert(delivered == 2 && !ctx.incomingClipboardTransfer.active);
    start(&ctx, LI_CLIPBOARD_ITEM_TYPE_TEXT, 6); chunk(&ctx, 0, 3, true); end(&ctx);
    assert(delivered == 2 && !ctx.incomingClipboardTransfer.active);
    start(&ctx, LI_CLIPBOARD_ITEM_TYPE_TEXT, 6); chunk(&ctx, 6, 1, false); end(&ctx);
    assert(delivered == 2 && !ctx.incomingClipboardTransfer.active);
    start(&ctx, LI_CLIPBOARD_ITEM_TYPE_NONE, 0); end(&ctx);
    assert(delivered == 3);
    processClipboardPacketCtx(&ctx, NULL, 0);
    LiSetThreadConnectionContext(NULL);
    puts("PASS: production clipboard receiver rejects gaps/overlap/truncation and delivers valid text/image/empty transfers");
    return 0;
}
