#include <sqos.h>

int main() {
    sq_print("====================================");
    sq_print(" Hello World from Ring 3 C ELF App! ");
    sq_print("====================================");

    sq_print("Sleeping for 1 second...");
    sq_sleep(1000);

    /* Test user-space heap allocation */
    sq_print("Allocating memory in user-space heap...");
    char *buf = (char *)sq_malloc(128);
    if (buf) {
        sq_print("Successfully allocated 128 bytes!");
        
        /* Write to user-space buffer to confirm read/write access */
        buf[0] = 'S'; buf[1] = 'Q'; buf[2] = '-'; buf[3] = 'O'; buf[4] = 'S';
        buf[5] = '\0';
        sq_print(buf);
    } else {
        sq_print("Heap allocation failed!");
    }

    sq_print("Exiting cleanly...");
    sq_exit(0);
    return 0;
}
