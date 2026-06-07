#include <stdio.h>
#include "graphics.h"

char canvas[ROWS][COLS];

void clearCanvas()
{
    int i, j;

    for(i = 0; i < ROWS; i++)
    {
        for(j = 0; j < COLS; j++)
        {
            canvas[i][j] = '_';
        }
    }
}

void displayCanvas()
{
    int i, j;

    for(i = 0; i < ROWS; i++)
    {
        for(j = 0; j < COLS; j++)
        {
            printf("%c", canvas[i][j]);
        }
        printf("\n");
    }
}
void drawRectangle(int row, int col, int width, int height)
{
    int i, j;

    /* Top and Bottom Borders */
    for(j = col; j < col + width; j++)
    {
        canvas[row][j] = '*';
        canvas[row + height - 1][j] = '*';
    }

    /* Left and Right Borders */
    for(i = row; i < row + height; i++)
    {
        canvas[i][col] = '*';
        canvas[i][col + width - 1] = '*';
    }
}