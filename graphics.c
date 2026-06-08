#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "graphics.h"

char canvas[ROWS][COLS];
GraphicObject objects[MAX_OBJECTS];
int objectCount = 0;
static int nextId = 1;

static int inBounds(int row, int col)
{
    return row >= 0 && row < ROWS && col >= 0 && col < COLS;
}

static void setPixel(int row, int col)
{
    if (inBounds(row, col))
    {
        canvas[row][col] = '*';
    }
}

void clearCanvas(void)
{
    for (int i = 0; i < ROWS; i++)
    {
        for (int j = 0; j < COLS; j++)
        {
            canvas[i][j] = '_';
        }
    }
}

void displayCanvas(void)
{
    for (int i = 0; i < ROWS; i++)
    {
        for (int j = 0; j < COLS; j++)
        {
            putchar(canvas[i][j]);
        }
        putchar('\n');
    }
}

void drawRectangle(int row, int col, int width, int height)
{
    if (width < 1 || height < 1)
        return;

    for (int j = col; j < col + width; j++)
    {
        setPixel(row, j);
        setPixel(row + height - 1, j);
    }
    for (int i = row; i < row + height; i++)
    {
        setPixel(i, col);
        setPixel(i, col + width - 1);
    }
}

void drawCircle(int row, int col, int radius)
{
    if (radius < 1)
        return;

    for (int dr = -radius; dr <= radius; dr++)
    {
        for (int dc = -radius; dc <= radius; dc++)
        {
            double distance = sqrt((double)dr * dr + (double)dc * dc);
            if (fabs(distance - radius) <= 0.5)
            {
                setPixel(row + dr, col + dc);
            }
        }
    }
}

void drawLine(int row1, int col1, int row2, int col2)
{
    int dr = abs(row2 - row1);
    int dc = abs(col2 - col1);
    int sr = row1 < row2 ? 1 : -1;
    int sc = col1 < col2 ? 1 : -1;
    int err = (dr > dc ? dr : -dc) / 2;
    int e2;

    while (1)
    {
        setPixel(row1, col1);
        if (row1 == row2 && col1 == col2)
            break;
        e2 = err;
        if (e2 > -dr)
        {
            err -= dc;
            row1 += sr;
        }
        if (e2 < dc)
        {
            err += dr;
            col1 += sc;
        }
    }
}

void drawTriangle(int row, int col, int base, int height)
{
    if (base < 1 || height < 1)
        return;

    int halfBase = base / 2;
    for (int i = 0; i < height; i++)
    {
        int left = col - (i * halfBase) / (height - 1 == 0 ? 1 : height - 1);
        int right = col + (i * halfBase) / (height - 1 == 0 ? 1 : height - 1);
        setPixel(row + i, left);
        setPixel(row + i, right);
    }
    for (int j = col - halfBase; j <= col + halfBase; j++)
    {
        setPixel(row + height - 1, j);
    }
}

void drawObject(const GraphicObject *object)
{
    switch (object->type)
    {
        case SHAPE_RECTANGLE:
            drawRectangle(object->row, object->col, object->param1, object->param2);
            break;
        case SHAPE_CIRCLE:
            drawCircle(object->row, object->col, object->param1);
            break;
        case SHAPE_LINE:
            drawLine(object->row, object->col, object->row2, object->col2);
            break;
        case SHAPE_TRIANGLE:
            drawTriangle(object->row, object->col, object->param1, object->param2);
            break;
    }
}

void redrawCanvas(void)
{
    clearCanvas();
    for (int i = 0; i < objectCount; i++)
    {
        drawObject(&objects[i]);
    }
}

int addObject(GraphicObject object)
{
    if (objectCount >= MAX_OBJECTS)
        return 0;

    object.id = nextId++;
    objects[objectCount++] = object;
    redrawCanvas();
    return object.id;
}

int deleteObject(int id)
{
    for (int i = 0; i < objectCount; i++)
    {
        if (objects[i].id == id)
        {
            for (int j = i; j + 1 < objectCount; j++)
            {
                objects[j] = objects[j + 1];
            }
            objectCount--;
            redrawCanvas();
            return 1;
        }
    }
    return 0;
}

void listObjects(void)
{
    if (objectCount == 0)
    {
        printf("No objects in the canvas.\n");
        return;
    }

    printf("ID  Type       Parameters\n");
    printf("------------------------------------------\n");
    for (int i = 0; i < objectCount; i++)
    {
        GraphicObject *obj = &objects[i];
        switch (obj->type)
        {
            case SHAPE_RECTANGLE:
                printf("%2d  Rectangle row=%d col=%d w=%d h=%d\n", obj->id, obj->row, obj->col, obj->param1, obj->param2);
                break;
            case SHAPE_CIRCLE:
                printf("%2d  Circle    row=%d col=%d r=%d\n", obj->id, obj->row, obj->col, obj->param1);
                break;
            case SHAPE_LINE:
                printf("%2d  Line      r1=%d c1=%d r2=%d c2=%d\n", obj->id, obj->row, obj->col, obj->row2, obj->col2);
                break;
            case SHAPE_TRIANGLE:
                printf("%2d  Triangle  row=%d col=%d base=%d h=%d\n", obj->id, obj->row, obj->col, obj->param1, obj->param2);
                break;
        }
    }
}

static void printMenu(void)
{
    printf("\n2D Graphics Editor\n");
    printf("1. Add rectangle\n");
    printf("2. Add circle\n");
    printf("3. Add line\n");
    printf("4. Add triangle\n");
    printf("5. Delete object\n");
    printf("6. List objects\n");
    printf("7. Display canvas\n");
    printf("8. Clear canvas\n");
    printf("9. Quit\n");
    printf("Choose an option: ");
}

static int readInt(const char *prompt)
{
    int value;
    printf("%s", prompt);
    while (scanf("%d", &value) != 1)
    {
        printf("Invalid input. %s", prompt);
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {}
    }
    return value;
}

int main(void)
{
    clearCanvas();
    redrawCanvas();

    while (1)
    {
        printMenu();
        int choice = readInt("");

        GraphicObject object;
        int id;
        switch (choice)
        {
            case 1:
                object.type = SHAPE_RECTANGLE;
                object.row = readInt("Enter top row: ");
                object.col = readInt("Enter left col: ");
                object.param1 = readInt("Enter width: ");
                object.param2 = readInt("Enter height: ");
                id = addObject(object);
                if (id)
                    printf("Added rectangle with ID %d.\n", id);
                else
                    printf("Unable to add rectangle. Object limit reached.\n");
                break;
            case 2:
                object.type = SHAPE_CIRCLE;
                object.row = readInt("Enter center row: ");
                object.col = readInt("Enter center col: ");
                object.param1 = readInt("Enter radius: ");
                id = addObject(object);
                if (id)
                    printf("Added circle with ID %d.\n", id);
                else
                    printf("Unable to add circle. Object limit reached.\n");
                break;
            case 3:
                object.type = SHAPE_LINE;
                object.row = readInt("Enter row1: ");
                object.col = readInt("Enter col1: ");
                object.row2 = readInt("Enter row2: ");
                object.col2 = readInt("Enter col2: ");
                id = addObject(object);
                if (id)
                    printf("Added line with ID %d.\n", id);
                else
                    printf("Unable to add line. Object limit reached.\n");
                break;
            case 4:
                object.type = SHAPE_TRIANGLE;
                object.row = readInt("Enter top row: ");
                object.col = readInt("Enter top col: ");
                object.param1 = readInt("Enter base width: ");
                object.param2 = readInt("Enter height: ");
                id = addObject(object);
                if (id)
                    printf("Added triangle with ID %d.\n", id);
                else
                    printf("Unable to add triangle. Object limit reached.\n");
                break;
            case 5:
                id = readInt("Enter object ID to delete: ");
                if (deleteObject(id))
                    printf("Deleted object %d.\n", id);
                else
                    printf("Object with ID %d not found.\n", id);
                break;
            case 6:
                listObjects();
                break;
            case 7:
                displayCanvas();
                break;
            case 8:
                objectCount = 0;
                clearCanvas();
                printf("Canvas cleared.\n");
                break;
            case 9:
                printf("Exiting editor.\n");
                return 0;
            default:
                printf("Invalid option. Please choose 1 to 9.\n");
                break;
        }
    }
}

