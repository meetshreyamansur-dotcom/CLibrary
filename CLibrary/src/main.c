#include "library.h"

#include <stdlib.h>

int main(void) {
    static Library library;
    if (!ui_startup(&library)) return EXIT_FAILURE;
    ui_run(&library);
    return EXIT_SUCCESS;
}
