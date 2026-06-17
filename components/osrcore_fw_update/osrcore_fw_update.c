#include "osrcore_fw_update.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define MBEDTLS_DECLARE_PRIVATE_IDENTIFIERS
#include "mbedtls/private/sha256.h"

#define FW_LINE_MAX 384
#define FW_SHA_HEX_LEN 64

typedef struct {
    bool active;
    size_t expected_size;
    size_t written;
    uint32_t next_seq;
    char expected_sha[FW_SHA_HEX_LEN + 1];
    const esp_partition_t *partition;
    esp_ota_handle_t handle;
    mbedtls_sha256_context sha_ctx;
} fw_state_t;

static fw_state_t s_fw;
static bool s_task_started;

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool hex_to_bytes(const char *hex, uint8_t *out, size_t out_cap, size_t *out_len)
{
    size_t len = strlen(hex);
    if ((len & 1) != 0 || len / 2 > out_cap) return false;
    for (size_t i = 0; i < len; i += 2) {
        int hi = hex_value(hex[i]);
        int lo = hex_value(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    *out_len = len / 2;
    return true;
}

static void bytes_to_hex(const uint8_t *bytes, size_t len, char *out)
{
    static const char lut[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = lut[bytes[i] >> 4];
        out[i * 2 + 1] = lut[bytes[i] & 0x0f];
    }
    out[len * 2] = '\0';
}

static void fw_abort(void)
{
    if (s_fw.active) {
        esp_ota_abort(s_fw.handle);
        mbedtls_sha256_free(&s_fw.sha_ctx);
    }
    memset(&s_fw, 0, sizeof(s_fw));
}

static bool valid_sha_hex(const char *sha)
{
    if (strlen(sha) != FW_SHA_HEX_LEN) return false;
    for (size_t i = 0; i < FW_SHA_HEX_LEN; i++) {
        if (!isxdigit((unsigned char)sha[i])) return false;
    }
    return true;
}

static void handle_fw_begin(const char *line)
{
    size_t size = 0;
    char sha[FW_SHA_HEX_LEN + 1] = {0};
    if (sscanf(line, "fw begin %zu %64s", &size, sha) != 2 || size == 0 || !valid_sha_hex(sha)) {
        printf("ERROR fw begin invalid\n");
        return;
    }

    fw_abort();

    const esp_partition_t *partition = esp_ota_get_next_update_partition(NULL);
    if (!partition) {
        printf("ERROR fw no ota partition\n");
        return;
    }

    esp_err_t err = esp_ota_begin(partition, size, &s_fw.handle);
    if (err != ESP_OK) {
        printf("ERROR fw begin 0x%x\n", (unsigned)err);
        memset(&s_fw, 0, sizeof(s_fw));
        return;
    }

    s_fw.active = true;
    s_fw.expected_size = size;
    s_fw.partition = partition;
    strncpy(s_fw.expected_sha, sha, sizeof(s_fw.expected_sha) - 1);
    mbedtls_sha256_init(&s_fw.sha_ctx);
    mbedtls_sha256_starts(&s_fw.sha_ctx, 0);

    printf("OK fw begin partition=%s size=%zu\n", partition->label, size);
}

static void handle_fw_data(const char *line)
{
    if (!s_fw.active) {
        printf("ERROR fw not active\n");
        return;
    }

    unsigned long seq_ul = 0;
    char hex[260] = {0};
    if (sscanf(line, "fw data %lu %259s", &seq_ul, hex) != 2 || seq_ul > UINT32_MAX) {
        printf("ERROR fw data invalid\n");
        return;
    }
    uint32_t seq = (uint32_t)seq_ul;
    if (seq != s_fw.next_seq) {
        printf("ERROR fw seq expected=%lu got=%lu\n",
               (unsigned long)s_fw.next_seq, (unsigned long)seq);
        return;
    }

    uint8_t chunk[128];
    size_t chunk_len = 0;
    if (!hex_to_bytes(hex, chunk, sizeof(chunk), &chunk_len) || chunk_len == 0) {
        printf("ERROR fw data hex\n");
        return;
    }
    if (s_fw.written + chunk_len > s_fw.expected_size) {
        printf("ERROR fw data overflow\n");
        return;
    }

    esp_err_t err = esp_ota_write(s_fw.handle, chunk, chunk_len);
    if (err != ESP_OK) {
        printf("ERROR fw write 0x%x\n", (unsigned)err);
        return;
    }

    mbedtls_sha256_update(&s_fw.sha_ctx, chunk, chunk_len);
    s_fw.written += chunk_len;
    s_fw.next_seq++;
    printf("OK fw data %lu %zu/%zu\n",
           (unsigned long)seq, s_fw.written, s_fw.expected_size);
}

static void handle_fw_end(void)
{
    if (!s_fw.active) {
        printf("ERROR fw not active\n");
        return;
    }
    if (s_fw.written != s_fw.expected_size) {
        printf("ERROR fw size %zu/%zu\n", s_fw.written, s_fw.expected_size);
        return;
    }

    uint8_t digest[32];
    char digest_hex[65];
    mbedtls_sha256_finish(&s_fw.sha_ctx, digest);
    bytes_to_hex(digest, sizeof(digest), digest_hex);

    if (strcmp(digest_hex, s_fw.expected_sha) != 0) {
        printf("ERROR fw sha expected=%s got=%s\n", s_fw.expected_sha, digest_hex);
        fw_abort();
        return;
    }

    esp_err_t err = esp_ota_end(s_fw.handle);
    if (err != ESP_OK) {
        printf("ERROR fw end 0x%x\n", (unsigned)err);
        fw_abort();
        return;
    }

    err = esp_ota_set_boot_partition(s_fw.partition);
    if (err != ESP_OK) {
        printf("ERROR fw boot 0x%x\n", (unsigned)err);
        fw_abort();
        return;
    }

    printf("OK fw reboot\n");
    mbedtls_sha256_free(&s_fw.sha_ctx);
    memset(&s_fw, 0, sizeof(s_fw));
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_restart();
}

bool osrcore_fw_handle_line(const char *line)
{
    if (!line) return false;

    if (strcmp(line, "stream off") == 0 || strcmp(line, "stream legacy") == 0 ||
        strcmp(line, "stream sync") == 0 || strcmp(line, "stream") == 0) {
        printf("OK %s\n", line);
        return true;
    }
    if (strcmp(line, "status") == 0) {
        printf("STATUS fw_update=1 active=%d written=%zu size=%zu\n",
               s_fw.active ? 1 : 0, s_fw.written, s_fw.expected_size);
        return true;
    }
    if (strcmp(line, "fw abort") == 0) {
        fw_abort();
        printf("OK fw abort\n");
        return true;
    }
    if (strncmp(line, "fw begin ", 9) == 0) {
        handle_fw_begin(line);
        return true;
    }
    if (strncmp(line, "fw data ", 8) == 0) {
        handle_fw_data(line);
        return true;
    }
    if (strcmp(line, "fw end") == 0) {
        handle_fw_end();
        return true;
    }

    return false;
}

static void fw_update_task(void *arg)
{
    (void)arg;
    char line[FW_LINE_MAX];
    while (1) {
        if (!fgets(line, sizeof(line), stdin)) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        if (!osrcore_fw_handle_line(line)) {
            printf("ERROR unknown command\n");
        }
    }
}

void osrcore_fw_update_start(void)
{
    if (s_task_started) return;
    s_task_started = true;
    xTaskCreate(fw_update_task, "fw_update", 8192, NULL, 3, NULL);
}
