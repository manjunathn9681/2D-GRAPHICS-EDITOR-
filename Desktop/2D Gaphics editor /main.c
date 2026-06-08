#include <stdio.h>
#include "graphics.h"

/* main() has been moved into graphics.c so graphics.c can be built standalone. */
#if 0
int main()
{
    clearCanvas();
    drawRectangle(2, 5, 15, 6);
    displayCanvas();
    return 0;
}
#endif