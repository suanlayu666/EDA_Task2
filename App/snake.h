#ifndef SECOND_TASK_SNAKE_H
#define SECOND_TASK_SNAKE_H

#include <stdbool.h>
#include <stdint.h>

enum
{
    SNAKE_COLUMNS = 10,
    SNAKE_ROWS = 12,
    SNAKE_CELL_COUNT = SNAKE_COLUMNS * SNAKE_ROWS
};

typedef enum
{
    SNAKE_UP,
    SNAKE_RIGHT,
    SNAKE_DOWN,
    SNAKE_LEFT
} SnakeDirection;

typedef enum
{
    SNAKE_EMPTY,
    SNAKE_BODY,
    SNAKE_HEAD,
    SNAKE_FOOD
} SnakeCell;

typedef enum
{
    SNAKE_READY,
    SNAKE_RUNNING,
    SNAKE_LOST,
    SNAKE_WON
} SnakeStatus;

typedef struct
{
    uint8_t cells[SNAKE_CELL_COUNT];
    uint8_t score;
    SnakeStatus status;
    bool automatic;
} SnakeState;

void Snake_Reset(bool automatic, uint32_t seed);
void Snake_SetDirection(SnakeDirection requested);
void Snake_Step(void);
const SnakeState *Snake_GetState(void);

#endif
