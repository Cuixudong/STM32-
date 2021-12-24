#ifndef __BCBK_H
#define __BCBK_H
#include "sys.h"

typedef struct
{
    uint8_t mode;//模式
    uint16_t screen_w;//屏幕宽度
    uint16_t screen_h;//屏幕高度
    uint8_t row;//行数
    uint8_t rank;//列数
    uint16_t score;//分数
} Game_;

typedef struct
{
    uint16_t blockx;//方块X坐标
    uint16_t blocky;//方块Y坐标
    uint16_t blockw;//方块宽度
    uint16_t blockh;//方块高度
    uint16_t color;//方块颜色
    uint8_t state;//方块状态
} block_;

void Init(Game_ *game);
uint8_t Button_Check(uint16_t x,uint16_t y);
void Show_Block(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t color);
void Show_Line(void);
void Play_Game(uint8_t mode);
void game_mode(void);

#endif
