#include <errno.h>
#include <fcntl.h>
#include <mtd/mtd-user.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#define EXPECTED_MTD_SIZE 0x60000u
#define EXPECTED_ERASE_SIZE 0x10000u
#define TARGET_OFFSET 0x50000u
#define BLOCK_SIZE 0x10000u

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void read_exact(int fd, void *buf, size_t len, off_t offset) {
    uint8_t *p = buf;
    size_t done = 0;

    while (done < len) {
        ssize_t n = pread(fd, p + done, len - done, offset + done);

        if (n < 0)
            die("pread");

        if (n == 0) {
            fprintf(stderr, "Unexpected EOF while reading\n");
            exit(EXIT_FAILURE);
        }

        done += (size_t)n;
    }
}

static void write_exact(int fd, const void *buf, size_t len, off_t offset) {
    const uint8_t *p = buf;
    size_t done = 0;

    while (done < len) {
        ssize_t n = pwrite(fd, p + done, len - done, offset + done);

        if (n < 0)
            die("pwrite");

        if (n == 0) {
            fprintf(stderr, "Zero-length write\n");
            exit(EXIT_FAILURE);
        }

        done += (size_t)n;
    }
}

static void load_file(const char *path, uint8_t *buf) {
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        die(path);

    struct stat st;
    if (fstat(fd, &st) < 0)
        die("fstat");

    if (st.st_size != BLOCK_SIZE) {
        fprintf(stderr, "%s: expected %u bytes, got %lld\n", path, BLOCK_SIZE,
                (long long)st.st_size);
        exit(EXIT_FAILURE);
    }

    read_exact(fd, buf, BLOCK_SIZE, 0);

    if (close(fd) < 0)
        die("close");
}

static size_t first_difference(const uint8_t *a, const uint8_t *b, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        if (a[i] != b[i])
            return i;
    }

    return len;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr,
                "Usage: %s /dev/mtd0 expected-original.bin replacement.bin\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    const char *device = argv[1];
    const char *expected_fn = argv[2];
    const char *new_fn = argv[3];

    /*
     * Deliberately refuse anything except /dev/mtd0.
     * This utility is for this particular Miyoo experiment,
     * not a general-purpose flash writer.
     */
    if (strcmp(device, "/dev/mtd0") != 0) {
        fprintf(stderr, "REFUSING: device must be exactly /dev/mtd0\n");
        return EXIT_FAILURE;
    }

    uint8_t *expected = malloc(BLOCK_SIZE);
    uint8_t *replacement = malloc(BLOCK_SIZE);
    uint8_t *current = malloc(BLOCK_SIZE);
    uint8_t *verify = malloc(BLOCK_SIZE);

    if (!expected || !replacement || !current || !verify) {
        fprintf(stderr, "malloc failed\n");
        return EXIT_FAILURE;
    }

    load_file(expected_fn, expected);
    load_file(new_fn, replacement);

    /*
     * Our intended modification is confined to the final 4 KiB.
     * Refuse if anything in the preceding 60 KiB differs.
     */
    if (memcmp(expected, replacement, 0xF000) != 0) {
        fprintf(stderr,
                "REFUSING: replacement modifies data before offset 0xF000 "
                "within erase block\n");
        return EXIT_FAILURE;
    }

    int fd = open(device, O_RDWR | O_SYNC);
    if (fd < 0)
        die("open MTD");

    struct mtd_info_user info;

    if (ioctl(fd, MEMGETINFO, &info) < 0)
        die("MEMGETINFO");

    printf("MTD information:\n");
    printf("  type:      %u\n", info.type);
    printf("  size:      0x%08x\n", info.size);
    printf("  erasesize: 0x%08x\n", info.erasesize);
    printf("  writesize: 0x%08x\n", info.writesize);

    if (info.type != MTD_NORFLASH) {
        fprintf(stderr, "REFUSING: device is not reported as NOR flash\n");
        return EXIT_FAILURE;
    }

    if (info.size != EXPECTED_MTD_SIZE) {
        fprintf(stderr, "REFUSING: expected MTD size 0x%x, got 0x%x\n",
                EXPECTED_MTD_SIZE, info.size);
        return EXIT_FAILURE;
    }

    if (info.erasesize != EXPECTED_ERASE_SIZE) {
        fprintf(stderr, "REFUSING: expected erase size 0x%x, got 0x%x\n",
                EXPECTED_ERASE_SIZE, info.erasesize);
        return EXIT_FAILURE;
    }

    if (TARGET_OFFSET + BLOCK_SIZE > info.size) {
        fprintf(stderr, "REFUSING: target block is outside MTD\n");
        return EXIT_FAILURE;
    }

    printf("\nReading current erase block at 0x%05x...\n", TARGET_OFFSET);

    read_exact(fd, current, BLOCK_SIZE, TARGET_OFFSET);

    if (memcmp(current, expected, BLOCK_SIZE) != 0) {
        size_t pos = first_difference(current, expected, BLOCK_SIZE);

        fprintf(stderr,
                "\nREFUSING TO ERASE.\n"
                "Current flash does not exactly match expected-original.bin.\n"
                "First difference:\n"
                "  block offset: 0x%04zx\n"
                "  flash:        0x%02x\n"
                "  expected:     0x%02x\n",
                pos, current[pos], expected[pos]);

        return EXIT_FAILURE;
    }

    printf("Current flash matches expected image exactly.\n");

    /*
     * Require an explicit confirmation. This is the point of no return:
     * after MEMERASE succeeds, power loss before programming completes
     * can leave the device unbootable.
     */
    printf("\nWARNING:\n");
    printf("  About to erase /dev/mtd0 range 0x50000-0x5ffff.\n");
    printf(
        "  Power loss during this operation may make the device unbootable.\n");
    printf("\nType exactly ERASE to continue: ");
    fflush(stdout);

    char answer[32];

    if (!fgets(answer, sizeof(answer), stdin)) {
        fprintf(stderr, "\nNo confirmation received.\n");
        return EXIT_FAILURE;
    }

    if (strcmp(answer, "ERASE\n") != 0 && strcmp(answer, "ERASE") != 0) {
        fprintf(stderr, "Aborted. Flash was NOT modified.\n");
        return EXIT_FAILURE;
    }

    struct erase_info_user erase = {.start = TARGET_OFFSET,
                                    .length = BLOCK_SIZE};

    printf("\nErasing 0x%05x-0x%05x...\n", TARGET_OFFSET,
           TARGET_OFFSET + BLOCK_SIZE - 1);

    if (ioctl(fd, MEMERASE, &erase) < 0)
        die("MEMERASE");

    printf("Erase complete.\n");
    printf("Programming replacement block...\n");

    write_exact(fd, replacement, BLOCK_SIZE, TARGET_OFFSET);

    /*
     * No fsync(): this MTD character device returns EINVAL for fsync().
     * Verification is performed by reading the programmed erase block
     * back from the device and comparing it byte-for-byte.
     */

    printf("Programming complete.\n");
    printf("Reading flash back for verification...\n");

    read_exact(fd, verify, BLOCK_SIZE, TARGET_OFFSET);

    if (memcmp(verify, replacement, BLOCK_SIZE) != 0) {
        size_t pos = first_difference(verify, replacement, BLOCK_SIZE);

        fprintf(stderr,
                "\n*** VERIFICATION FAILED ***\n"
                "First difference:\n"
                "  block offset: 0x%04zx\n"
                "  flash:        0x%02x\n"
                "  expected:     0x%02x\n"
                "\nDO NOT REBOOT THE DEVICE.\n",
                pos, verify[pos], replacement[pos]);

        return EXIT_FAILURE;
    }

    printf("\n========================================\n");
    printf("SUCCESS\n");
    printf("========================================\n");
    printf("Flash read-back matches replacement exactly.\n");
    printf("Modified range: 0x50000-0x5ffff\n");
    printf("U-Boot environment: 0x5f000-0x5ffff\n");
    printf("\nIt is now safe to reboot.\n");

    close(fd);

    free(expected);
    free(replacement);
    free(current);
    free(verify);

    return EXIT_SUCCESS;
}
