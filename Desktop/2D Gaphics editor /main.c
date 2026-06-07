#include <stdio.h>
#include "graphics.h"

int main()
{
    clearCanvas();

    drawRectangle(2, 5, 15, 6);

    displayCanvas();

    return 0;
}