#include "tilt_maze.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "main.h"
#include "mpu6050.h"
#include "tftlcd.h"
#include "touch_input.h"

enum
{
    BOARD_X = 10,
    BOARD_Y = 43,
    STEP_MS = 20,
    ZERO_SAMPLES = 25,
    MAZE_COLUMNS = 11,
    MAZE_ROWS = 13,
    MAZE_CELL_WIDTH = 20,
    MAZE_CELL_HEIGHT = 19,
    MAZE_BALL_RADIUS = 4,
    RESTART_X = 150,
    RESTART_Y = 292,
    RESTART_WIDTH = 78,
    RESTART_HEIGHT = 26,
};

/* 传感器方向若与屏幕不符，只调整这三个值。 */
#define SWAP_SENSOR_AXES 0
#define SCREEN_X_SIGN (-1)
#define SCREEN_Y_SIGN 1

#define COLOR_BACKGROUND 0x10A4U
#define COLOR_WALL 0x3AF0U
#define COLOR_PATH 0xE71CU
#define COLOR_START 0x9CF5U
#define COLOR_GOAL 0x5E29U
#define COLOR_BALL 0xFB60U
#define COLOR_CRASH 0xF800U
#define COLOR_BUTTON 0x3AF0U

typedef enum
{
    CALIBRATING,
    PLAYING,
    WON,
    FAILED,
    SENSOR_ERROR,
} GameState;

typedef struct
{
    float x;
    float y;
    float velocity_x;
    float velocity_y;
} Maze;

static Maze maze;
static GameState state;
static uint32_t last_step_ms;
static uint32_t started_ms;
static uint32_t drawn_tenths;
static int32_t zero_sum_x;
static int32_t zero_sum_y;
static unsigned int zero_count;
static unsigned int sensor_failures;
static float zero_x;
static float zero_y;
static float filtered_x;
static float filtered_y;

static const char level[MAZE_ROWS][MAZE_COLUMNS + 1] = {
    "###########",
    "#S#.....#.#",
    "#.###.#.#.#",
    "#.....#...#",
    "#.#######.#",
    "#...#.....#",
    "#####.#####",
    "#.#...#...#",
    "#.#.###.#.#",
    "#.#.#...#.#",
    "#.#.#.###.#",
    "#.....#..G#",
    "###########",
};

/* 返回地图格子。地图外按墙处理。 */
static char Maze_Tile(int column, int row)
{
    if (column < 0 || column >= MAZE_COLUMNS || row < 0 || row >= MAZE_ROWS)
    {
        return '#';
    }
    return level[row][column];
}

/* 把球放到 S 格中央并清空速度。 */
static void Maze_Reset(Maze *maze)
{
    *maze = (Maze){
        .x = 1.5f * MAZE_CELL_WIDTH,
        .y = 1.5f * MAZE_CELL_HEIGHT,
    };
}

/* 对单轴移动检查整段球心轨迹到墙的最小距离，擦到墙角也算碰撞。 */
static bool HitsWall(float from_x, float from_y, float to_x, float to_y)
{
    const float min_x = fminf(from_x, to_x);
    const float max_x = fmaxf(from_x, to_x);
    const float min_y = fminf(from_y, to_y);
    const float max_y = fmaxf(from_y, to_y);
    const int left = (int)ceilf((min_x - MAZE_BALL_RADIUS) / MAZE_CELL_WIDTH) - 1;
    const int right = (int)floorf((max_x + MAZE_BALL_RADIUS) / MAZE_CELL_WIDTH);
    const int top = (int)ceilf((min_y - MAZE_BALL_RADIUS) / MAZE_CELL_HEIGHT) - 1;
    const int bottom = (int)floorf((max_y + MAZE_BALL_RADIUS) / MAZE_CELL_HEIGHT);

    for (int row = top; row <= bottom; ++row)
    {
        for (int column = left; column <= right; ++column)
        {
            if (Maze_Tile(column, row) == '#')
            {
                const float wall_x = column * MAZE_CELL_WIDTH;
                const float wall_y = row * MAZE_CELL_HEIGHT;
                const float dx = fmaxf(0.0f, fmaxf(wall_x - max_x, min_x - wall_x - MAZE_CELL_WIDTH));
                const float dy = fmaxf(0.0f, fmaxf(wall_y - max_y, min_y - wall_y - MAZE_CELL_HEIGHT));
                if (dx * dx + dy * dy <= MAZE_BALL_RADIUS * MAZE_BALL_RADIUS)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

/* 限制加速度和最高速度，加大阻尼让放平后的小球更快停下。 */
static float NextVelocity(float velocity, float tilt)
{
    const float speed_limit = 1.8f;  /* 每轴每 20 ms 的最大移动像素数。 */
    const float next = (velocity + tilt * 1.2f) * 0.84f;
    return fminf(speed_limit, fmaxf(-speed_limit, next));
}

/* 先检查横向与纵向轨迹，再提交坐标。撞墙时保留上一次安全的位置。 */
static GameState Maze_Step(Maze *maze, float tilt_x, float tilt_y)
{
    maze->velocity_x = NextVelocity(maze->velocity_x, tilt_x);
    maze->velocity_y = NextVelocity(maze->velocity_y, tilt_y);

    const float next_x = maze->x + maze->velocity_x;
    const float next_y = maze->y + maze->velocity_y;
    if (HitsWall(maze->x, maze->y, next_x, maze->y) ||
        HitsWall(next_x, maze->y, next_x, next_y))
    {
        return FAILED;
    }

    maze->x = next_x;
    maze->y = next_y;
    const char tile = Maze_Tile((int)maze->x / MAZE_CELL_WIDTH, (int)maze->y / MAZE_CELL_HEIGHT);
    return tile == 'G' ? WON : PLAYING;
}

/* 重开只响应按钮区域内的按下事件。按住或拖动时不重复触发。 */
static bool RestartPressed(TouchEvent event)
{
    return event.type == TOUCH_DOWN &&
           event.point.x >= RESTART_X && event.point.x < RESTART_X + RESTART_WIDTH &&
           event.point.y >= RESTART_Y && event.point.y < RESTART_Y + RESTART_HEIGHT;
}

/* 在深色背景上用已有 LCD 字库绘制标签。 */
static void Text(uint16_t x, uint16_t y, uint16_t width, uint8_t size, const char *label)
{
    FRONT_COLOR = WHITE;
    BACK_COLOR = COLOR_BACKGROUND;
    LCD_ShowString(x, y, width, size, size, (u8 *)label);
}

/* 重开按钮的位置与触摸判断使用同一组坐标。 */
static void DrawRestartButton(void)
{
    LCD_Fill(RESTART_X, RESTART_Y, RESTART_X + RESTART_WIDTH - 1,
             RESTART_Y + RESTART_HEIGHT - 1, COLOR_BUTTON);
    FRONT_COLOR = WHITE;
    BACK_COLOR = COLOR_BUTTON;
    LCD_ShowString(RESTART_X + 11, RESTART_Y + 5, 56, 16, 16, (u8 *)"RESTART");
}

/* 校零或出错时显示原因，进入游戏后由计时数字接管这一块区域。 */
static void DrawStatus(void)
{
    LCD_Fill(0, 292, 144, 319, COLOR_BACKGROUND);
    if (state == CALIBRATING)
    {
        Text(12, 298, 128, 16, "KEEP BOARD FLAT");
    }
    else if (state == SENSOR_ERROR)
    {
        Text(12, 298, 128, 16, "MPU6050 ERROR");
    }
    drawn_tenths = UINT32_MAX;
}

/* 按十分之一秒更新读数，结束时显示 WIN 或 FAIL。 */
static void DrawTime(uint32_t now)
{
    const uint32_t tenths = (now - started_ms) / 100U;
    if (tenths == drawn_tenths)
    {
        return;
    }
    drawn_tenths = tenths;
    char text[20];
    const char *label = state == FAILED ? "FAIL" : state == WON ? "WIN" : "TIME";
    (void)snprintf(text, sizeof text, "%s %lu.%lu s", label,
                   (unsigned long)(tenths / 10U), (unsigned long)(tenths % 10U));
    LCD_Fill(0, 292, 144, 319, COLOR_BACKGROUND);
    Text(12, 298, 128, 16, text);
}

/* 按格子类型选择颜色并填满格子。 */
static void DrawTile(int column, int row)
{
    const char tile = Maze_Tile(column, row);
    uint16_t color = COLOR_PATH;
    if (tile == '#') color = COLOR_WALL;
    if (tile == 'S') color = COLOR_START;
    if (tile == 'G') color = COLOR_GOAL;

    const int x = BOARD_X + column * MAZE_CELL_WIDTH;
    const int y = BOARD_Y + row * MAZE_CELL_HEIGHT;
    LCD_Fill(x, y, x + MAZE_CELL_WIDTH - 1, y + MAZE_CELL_HEIGHT - 1, color);
}

/* 逐行画实心圆，每一行只设置一次 LCD 窗口。 */
static void DrawBall(int x, int y)
{
    static const uint8_t half_width[] = {0, 2, 3, 3, 4, 3, 3, 2, 0};
    const uint16_t color = state == FAILED ? COLOR_CRASH : COLOR_BALL;
    for (int dy = -MAZE_BALL_RADIUS; dy <= MAZE_BALL_RADIUS; ++dy)
    {
        const int half = half_width[dy + MAZE_BALL_RADIUS];
        LCD_Fill(BOARD_X + x - half, BOARD_Y + y + dy,
                 BOARD_X + x + half, BOARD_Y + y + dy, color);
    }
}

/* 只还原旧球覆盖过的格子，保留其他区域。 */
static void EraseBall(int x, int y)
{
    for (int row = (y - MAZE_BALL_RADIUS) / MAZE_CELL_HEIGHT;
         row <= (y + MAZE_BALL_RADIUS) / MAZE_CELL_HEIGHT; ++row)
    {
        for (int column = (x - MAZE_BALL_RADIUS) / MAZE_CELL_WIDTH;
             column <= (x + MAZE_BALL_RADIUS) / MAZE_CELL_WIDTH; ++column)
        {
            DrawTile(column, row);
        }
    }
}

/* 清屏画标题和迷宫，进入或重开时调用。 */
static void DrawBoard(void)
{
    LCD_Clear(COLOR_BACKGROUND);
    Text(12, 9, 188, 16, "TILT MAZE");
    Text(12, 28, 216, 12, "Reach green. Touch wall = fail");
    for (int row = 0; row < MAZE_ROWS; ++row)
    {
        for (int column = 0; column < MAZE_COLUMNS; ++column)
        {
            DrawTile(column, row);
        }
    }
    DrawBall((int)maze.x, (int)maze.y);
    DrawRestartButton();
    DrawStatus();
}

/* 进入或重开时复位球和校零状态，并重新探测板载传感器。 */
void TiltMaze_Open(void)
{
    Maze_Reset(&maze);
    zero_count = 0;
    zero_sum_x = 0;
    zero_sum_y = 0;
    filtered_x = 0;
    filtered_y = 0;
    sensor_failures = 0;
    state = Mpu6050_Init() ? CALIBRATING : SENSOR_ERROR;
    DrawBoard();
    last_step_ms = HAL_GetTick();
}

/* 平放时连续采样，取 X、Y 的平均值作为零点。 */
static void Calibrate(const Mpu6050Acceleration *raw, uint32_t now)
{
    if (abs(raw->x) > 3600 || abs(raw->y) > 3600 || abs(raw->z) < 14000)
    {
        zero_count = 0;
        zero_sum_x = 0;
        zero_sum_y = 0;
        return;
    }
    zero_sum_x += raw->x;
    zero_sum_y += raw->y;
    ++zero_count;
    if (zero_count == ZERO_SAMPLES)
    {
        zero_x = (float)zero_sum_x / ZERO_SAMPLES;
        zero_y = (float)zero_sum_y / ZERO_SAMPLES;
        started_ms = now;
        state = PLAYING;
        DrawStatus();
        DrawTime(now);
    }
}

/* 减去零点并平滑加速度，推进游戏，只重绘移动过的球。 */
static void PlayStep(const Mpu6050Acceleration *raw, uint32_t now)
{
    filtered_x += ((raw->x - zero_x) / 16384.0f - filtered_x) * 0.2f;
    filtered_y += ((raw->y - zero_y) / 16384.0f - filtered_y) * 0.2f;
    const float x = SCREEN_X_SIGN * (SWAP_SENSOR_AXES ? filtered_y : filtered_x);
    const float y = SCREEN_Y_SIGN * (SWAP_SENSOR_AXES ? filtered_x : filtered_y);

    const int old_x = (int)maze.x;
    const int old_y = (int)maze.y;
    state = Maze_Step(&maze, fabsf(x) < 0.035f ? 0.0f : x, fabsf(y) < 0.035f ? 0.0f : y);
    const int new_x = (int)maze.x;
    const int new_y = (int)maze.y;
    if (old_x != new_x || old_y != new_y)
    {
        EraseBall(old_x, old_y);
        DrawBall(new_x, new_y);
    }
    if (state == FAILED)
    {
        DrawBall(old_x, old_y);
    }
    if (state == WON || state == FAILED)
    {
        drawn_tenths = UINT32_MAX;
    }
    DrawTime(now);
}

/* 仅在迷宫页调用。处理触屏重开，并每 20 ms 读取一次姿态。 */
void TiltMaze_Task(void)
{
    TouchEvent event;
    if (TouchInput_Poll(false, &event) && RestartPressed(event))
    {
        TiltMaze_Open();
        return;
    }

    const uint32_t now = HAL_GetTick();
    if ((state != CALIBRATING && state != PLAYING) || now - last_step_ms < STEP_MS)
    {
        return;
    }
    last_step_ms = now;

    Mpu6050Acceleration raw;
    if (!Mpu6050_ReadAcceleration(&raw))
    {
        if (++sensor_failures >= 3U)
        {
            state = SENSOR_ERROR;
            DrawStatus();
        }
        return;
    }
    sensor_failures = 0;
    if (state == CALIBRATING)
    {
        Calibrate(&raw, now);
    }
    else
    {
        PlayStep(&raw, now);
    }
}
