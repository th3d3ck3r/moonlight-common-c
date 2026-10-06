#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "Limelight-internal.h"
#include "Input.h"
#include <rs.h>

static void testFec(int data, int parity, bool audio) {
    enum { SIZE = 1024, MAX_SHARDS = 12 };
    unsigned char storage[MAX_SHARDS][SIZE], original[MAX_SHARDS][SIZE];
    unsigned char *shards[MAX_SHARDS], marks[MAX_SHARDS] = {0};
    reed_solomon *rs = reed_solomon_new(data, parity);
    assert(rs != NULL);
    if (audio) {
        const unsigned char nvidiaParity[] = {0x77, 0x40, 0x38, 0x0e, 0xc7, 0xa7, 0x0d, 0x6c};
        memcpy(rs->p, nvidiaParity, sizeof(nvidiaParity));
    }
    memset(storage, 0, sizeof(storage));
    for (int i = 0; i < data + parity; i++) {
        shards[i] = storage[i];
        if (i < data) for (int j = 0; j < SIZE; j++) storage[i][j] = (unsigned char)(i * 37 + j);
    }
    assert(reed_solomon_encode(rs, shards, data + parity, SIZE) == 0);
    memcpy(original, storage, sizeof(storage));
    marks[0] = marks[2] = 1;
    memset(storage[0], 0, SIZE);
    memset(storage[2], 0, SIZE);
    assert(reed_solomon_reconstruct(rs, shards, marks, data + parity, SIZE) == 0);
    for (int i = 0; i < data; i++) assert(memcmp(original[i], storage[i], SIZE) == 0);
    reed_solomon_release(rs);
}

static void testCrypto(void) {
    unsigned char key[16] = {1}, iv[12] = {2}, tag[16];
    unsigned char plain[64], encrypted[96], decoded[96];
    int encryptedLength = 0, decodedLength = 0;
    for (unsigned int i = 0; i < sizeof(plain); i++) plain[i] = (unsigned char)i;
    PPLT_CRYPTO_CONTEXT enc = PltCreateCryptoContext(), dec = PltCreateCryptoContext();
    assert(enc && dec);
    assert(PltEncryptMessage(enc, ALGORITHM_AES_GCM, CIPHER_FLAG_RESET_IV, key, sizeof(key), iv, sizeof(iv), tag, sizeof(tag), plain, sizeof(plain), encrypted, &encryptedLength));
    assert(PltDecryptMessage(dec, ALGORITHM_AES_GCM, CIPHER_FLAG_RESET_IV, key, sizeof(key), iv, sizeof(iv), tag, sizeof(tag), encrypted, encryptedLength, decoded, &decodedLength));
    assert(decodedLength == sizeof(plain) && memcmp(decoded, plain, sizeof(plain)) == 0);
    tag[0] ^= 1;
    assert(!PltDecryptMessage(dec, ALGORITHM_AES_GCM, CIPHER_FLAG_RESET_IV, key, sizeof(key), iv, sizeof(iv), tag, sizeof(tag), encrypted, encryptedLength, decoded, &decodedLength));
    PltDestroyCryptoContext(enc);
    PltDestroyCryptoContext(dec);
    PltDestroyCryptoContext(NULL);
}

int main(void) {
    reed_solomon_init();
    testFec(4, 2, true);
    testFec(8, 4, false);
    testCrypto();
    ML_INPUT_STREAM_CONTEXT ctx = {0};
    assert(LiSendControllerTouchEventCtx(&ctx, 0, 0, 0, 0, 0, 0) == -2);
    assert(LiSendControllerTouchEvent2Ctx(&ctx, 0, 0, 1, 0, 0, 0, 0) == -2);
    assert(LiSendKeyboardEventCtx(&ctx, 0x800D, KEY_ACTION_DOWN, MODIFIER_EXTENDED) == -2);
    assert(sizeof(SS_CONTROLLER_TOUCH_PACKET) == sizeof(NV_INPUT_HEADER) + 20);
    puts("PASS: audio/video FEC recovery, AES-GCM authentication, null-safe crypto teardown, context input APIs, controller wire layout");
    return 0;
}
