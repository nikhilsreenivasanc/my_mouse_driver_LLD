// my_mouse_user.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>

struct mouse_event {
    int8_t dx;
    int8_t dy;
    uint8_t buttons;
} __attribute__((packed));

int main(void)
{
    int fd = open("/dev/my_mouse", O_RDONLY);
    if (fd < 0) {
        perror("open /dev/my_mouse");
        return 1;
    }

    struct mouse_event ev;
    while (1) {
        ssize_t r = read(fd, &ev, sizeof(ev));
        if (r < 0) {
            perror("read");
            break;
        } else if (r == 0) {
            printf("Device removed or EOF\n");
            break;
        } else {
            printf("X-axis value: %d    Y-axis value: %d    Buttons=0x %02x", ev.dx, ev.dy, ev.buttons);
            if (ev.buttons & 0x01) printf(" [LEFT]");
            if (ev.buttons & 0x02) printf(" [RIGHT]");
            if (ev.buttons & 0x04) printf(" [MIDDLE]");
            
            printf("\n");
        }
    }
    close(fd);
    return 0;
}
