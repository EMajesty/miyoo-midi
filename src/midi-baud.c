#include <asm/ioctls.h>
#include <asm/termbits.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s /dev/ttySx\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    struct termios2 tio;

    if (ioctl(fd, TCGETS2, &tio) < 0) {
        perror("TCGETS2");
        return 1;
    }

    tio.c_cflag &= ~(CBAUD | CSIZE | PARENB | CSTOPB);
    tio.c_cflag |= BOTHER | CS8 | CLOCAL | CREAD;

    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_lflag = 0;

    tio.c_ispeed = 31250;
    tio.c_ospeed = 31250;

    if (ioctl(fd, TCSETS2, &tio) < 0) {
        perror("TCSETS2");
        return 1;
    }

    if (ioctl(fd, TCGETS2, &tio) < 0) {
        perror("TCGETS2 verify");
        return 1;
    }

    printf("requested 31250\n");
    printf("reported input:  %u\n", tio.c_ispeed);
    printf("reported output: %u\n", tio.c_ospeed);

    uint8_t note_on[] = {0x90, 0x3C, 0x7F};
    uint8_t note_off[] = {0x80, 0x3C, 0x00};

    for (;;) {
        write(fd, note_on, sizeof(note_on));
        usleep(500000);
        write(fd, note_off, sizeof(note_off));
        usleep(500000);
    }

    close(fd);
    return 0;
}
