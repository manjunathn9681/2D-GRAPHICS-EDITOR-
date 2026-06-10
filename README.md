# 2D Graphics Editor

## Project Overview
The 2D Graphics Editor is a menu-driven mini project developed in C that allows users to create and manage basic geometric shapes on a 2D character array canvas. The application provides an interactive terminal-based interface for drawing and editing shapes such as rectangles, circles, lines, and triangles.

## Features
- Draw Rectangle
- Draw Circle
- Draw Line
- Draw Triangle
- Object IDs
- Delete Object
- Modify Object
- Canvas Snapshot
- Interactive Cursor Controls
- Circle Radius Selection
- Triangle Point Selection
- Object Listing
- Clear Canvas

## Technologies Used
- C Programming Language
- GCC Compiler
- Git
- GitHub

## Project Structure
The project is organized into the following files:
- main.c - Contains the main menu and program flow
- graphics.c - Implements all drawing, object management, and interaction features
- graphics.h - Declares structures, function prototypes, and constants
- Makefile - Used to build the project
- prompt.txt - Stores the GenAI prompt and response history used for development support

## How to Build
Run the following command in the project directory:

```bash
make
```

## How to Run
After building the project, run:

```bash
./graphics
```

## Sample Workflow
1. Start the program.
2. Select a shape from the menu.
3. Position the shape using interactive cursor controls.
4. Confirm the shape placement.
5. View the updated canvas.
6. Modify or delete existing objects as needed.
7. Save a snapshot of the current canvas.

## Data Structures
The project uses the following core data structures:
- 2D character array canvas for rendering the drawing area
- Object structure for storing shape type and coordinates
- Object ID system for identifying and managing individual shapes

## Mini Project Requirements Covered
- [x] 2D character array
- [x] Display function
- [x] Add objects
- [x] Delete objects
- [x] Modify objects
- [x] Rectangle
- [x] Circle
- [x] Line
- [x] Triangle

## Future Improvements
- Colors
- Filled shapes
- Undo/Redo
- Mouse support
- Export to image

## Author
Manjunath