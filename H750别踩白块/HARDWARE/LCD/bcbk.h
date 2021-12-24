#ifndef __BCBK_H
#define __BCBK_H
#include "sys.h"

typedef struct
{
    uint8_t mode;//ģʽ
    uint16_t screen_w;//��Ļ���
    uint16_t screen_h;//��Ļ�߶�
    uint8_t row;//����
    uint8_t rank;//����
    uint16_t score;//����
} Game_;

typedef struct
{
    uint16_t blockx;//����X����
    uint16_t blocky;//����Y����
    uint16_t blockw;//������
    uint16_t blockh;//����߶�
    uint16_t color;//������ɫ
    uint8_t state;//����״̬
} block_;

void Init(Game_ *game);
uint8_t Button_Check(uint16_t x,uint16_t y);
void Show_Block(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t color);
void Show_Line(void);
void Play_Game(uint8_t mode);
void game_mode(void);

#endif
