#include "components/hardware_driver/include/esp_board_init.h"
#include <stdio.h>

int main(void) {
    // Call a representative init function; using dummy parameters
    esp_err_t err = esp_board_init(44100, 0, 16);
    if (err != 0) {
        printf("FAIL: esp_board_init returned %d\n", err);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
