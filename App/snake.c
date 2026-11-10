#include "snake.h"

#include <string.h>

enum { START_LENGTH = 3 };

static SnakeState game;
static uint8_t body[SNAKE_CELL_COUNT];
static uint8_t tail_slot;
static uint8_t length;
static uint8_t head;
static SnakeDirection direction;
static SnakeDirection pending_direction;
static bool turn_pending;
static uint32_t random_state;

/* 从剩余空格中挑一格放置食物，由重开或吃到食物后调用。 */
static void PlaceFood(void)
{
    /* 在所有空格中选第 n 个，快填满时也只扫一遍。 */
    random_state = random_state * 1664525U + 1013904223U;
    unsigned int n = (random_state >> 16) % (SNAKE_CELL_COUNT - length);
    for (unsigned int cell = 0; cell < SNAKE_CELL_COUNT; ++cell)
    {
        if (game.cells[cell] == SNAKE_EMPTY && n-- == 0U)
        {
            game.cells[cell] = SNAKE_FOOD;
            return;
        }
    }
}

/* 按手动或自动模式重建棋盘、蛇身和方向，再生成第一份食物。 */
void Snake_Reset(bool automatic, uint32_t seed)
{
    memset(&game, 0, sizeof game);
    game.automatic = automatic;
    game.status = automatic ? SNAKE_RUNNING : SNAKE_READY;
    random_state = seed;
    tail_slot = 0;
    length = START_LENGTH;
    direction = SNAKE_RIGHT;
    pending_direction = direction;
    turn_pending = false;

    /* 自动从闭环的前几格开始，手动从棋盘中间一行开始。 */
    const uint8_t start = automatic ? 0 : (SNAKE_ROWS / 2) * SNAKE_COLUMNS;
    for (uint8_t i = 0; i < length; ++i)
    {
        body[i] = start + i;
        game.cells[body[i]] = SNAKE_BODY;
    }
    head = body[length - 1U];
    game.cells[head] = SNAKE_HEAD;
    PlaceFood();
}

/* 记录下一步的手动方向，拒绝立即反向和同一步内的第二次转向。 */
void Snake_SetDirection(SnakeDirection requested)
{
    if (game.automatic || (game.status != SNAKE_READY && game.status != SNAKE_RUNNING) ||
        turn_pending || requested == (direction + 2U) % 4U)
    {
        return;
    }

    pending_direction = requested;
    turn_pending = requested != direction;
    game.status = SNAKE_RUNNING;
}

/* 沿经过棋盘每一格的闭环计算下一个格子。 */
static uint8_t AutoNextCell(void)
{
    const unsigned int x = head % SNAKE_COLUMNS;
    const unsigned int y = head / SNAKE_COLUMNS;

    /* 行内左右往返，最左列留作回到起点的通道。 */
    if (x == 0U && y != 0U)
    {
        return head - SNAKE_COLUMNS;
    }
    if (y % 2U == 0U)
    {
        return x == SNAKE_COLUMNS - 1U ? head + SNAKE_COLUMNS : head + 1U;
    }
    if (x == 1U && y != SNAKE_ROWS - 1U)
    {
        return head + SNAKE_COLUMNS;
    }
    return head - 1U;
}

/* 应用已提交的方向并计算下一格，越界时返回 -1。 */
static int ManualNextCell(void)
{
    static const int dx[] = {0, 1, 0, -1};
    static const int dy[] = {-1, 0, 1, 0};

    direction = pending_direction;
    turn_pending = false;
    const int x = head % SNAKE_COLUMNS + dx[direction];
    const int y = head / SNAKE_COLUMNS + dy[direction];
    if (x < 0 || x >= SNAKE_COLUMNS || y < 0 || y >= SNAKE_ROWS)
    {
        return -1;
    }
    return y * SNAKE_COLUMNS + x;
}

/* 前进一步，检查碰撞、更新蛇身和分数，并判断是否吃满棋盘。 */
void Snake_Step(void)
{
    if (game.status != SNAKE_RUNNING)
    {
        return;
    }

    const int next = game.automatic ? AutoNextCell() : ManualNextCell();
    if (next < 0 || (game.cells[next] == SNAKE_BODY && next != body[tail_slot]))
    {
        game.status = SNAKE_LOST;
        return;
    }

    const bool ate_food = game.cells[next] == SNAKE_FOOD;
    if (ate_food)
    {
        ++length;
        ++game.score;
    }
    else
    {
        /* 没吃到食物时，先移开旧尾巴，再添加新蛇头。 */
        game.cells[body[tail_slot]] = SNAKE_EMPTY;
        tail_slot = (tail_slot + 1U) % SNAKE_CELL_COUNT;
    }

    game.cells[head] = SNAKE_BODY;
    head = (uint8_t)next;
    game.cells[head] = SNAKE_HEAD;
    body[(tail_slot + length - 1U) % SNAKE_CELL_COUNT] = head;
    if (length == SNAKE_CELL_COUNT)
    {
        game.status = SNAKE_WON;
    }
    else if (ate_food)
    {
        PlaceFood();
    }
}

/* 返回供界面绘制的棋盘、分数和游戏状态。 */
const SnakeState *Snake_GetState(void)
{
    return &game;
}
