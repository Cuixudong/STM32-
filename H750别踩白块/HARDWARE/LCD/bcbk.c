#include "bcbk.h"
#include "lcd.h"
#include "led.h"
#include "key.h"
#include "adc.h"
#include "pcf8574.h"
#include "delay.h"
#include "usart.h"
#include "touch.h"

#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

uint8_t colorful = 0;//0 ��ɫ�� 1:��ɫ��

Game_ game;

block_ blk[20];//(game.row+1)*(game.rank)

void Init(Game_ *game)
{
    game->mode = 0;
    game->screen_w = lcddev.width;
    game->screen_h = lcddev.height;
    game->row = 4;
    game->rank = 4;
    game->score = 0;
}

uint8_t Button_Check(uint16_t x,uint16_t y)
{
    uint8_t res = 0;
    uint8_t i, j;
    static uint8_t touch=0;
    tp_dev.scan(0);
    if(tp_dev.sta&TP_PRES_DOWN)			//������������
    {
        if(touch != 1)
        {
            touch = 1;
        }
        else
        {
            return 0xEE;
        }
        for(i=0; i<game.rank; i++)
        {
            for(j=0; j<game.row; j++)
            {
                if(tp_dev.x[0]<(x+(j+1)*(game.screen_w/game.rank))&&tp_dev.x[0]>(x+j*(game.screen_w/game.rank))&&\
                        tp_dev.y[0]<(y+(i+1)*(game.screen_h/game.row))&&tp_dev.y[0]>(y+i*(game.screen_h/game.row)))
                {
                    res=i*game.rank+j;
                    break;
                }
            }
        }
    }
    else
    {
        touch = 0;
        res = 0xEE;
    }
    return res;
}

void Show_Block(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t color)
{
    LCD_Fill(x,y,x+w,y+h,color);//������
}

void Show_Line(void)
{
    uint8_t i;
    POINT_COLOR = BLACK;
    for(i=1; i<game.row; i++)
    {
        LCD_DrawLine(0,i*(game.screen_h/game.row),game.screen_w,i*(game.screen_h/game.row));//������
    }
    for(i=1; i<game.rank; i++)
    {
        LCD_DrawLine(i*(game.screen_w/game.rank),0,i*(game.screen_w/game.rank),game.screen_h);//������
    }
}

void Play_Game(uint8_t mode)
{
    uint8_t val;
    uint8_t fail = 0;
    uint8_t key=0;
    uint8_t ran = 0;
    uint16_t update=0;
    POINT_COLOR=BLACK;
    BACK_COLOR = WHITE;
    //srand(Get_Adc(0));
    LCD_Clear(WHITE);
    if(mode <= 1)//��ģʽ
    {
        Init(&game);
        for(val=0; val<game.row*game.rank; val++) //��ʼ��������ɫ�ߴ���Ϣ
        {
            blk[val].blockw = game.screen_w/game.rank;
            blk[val].blockh = game.screen_h/game.row;
            blk[val].blockx = (val%game.rank) * blk[val].blockw;
            blk[val].blocky = game.screen_h - (game.screen_h - (game.row - (val/game.rank) - 1) * blk[val].blockh);
            if(val>((game.row*(game.rank-1))-1))//���һ�л�ɫ
            {
                blk[val].color = YELLOW;
            }
            else
            {
                blk[val].color = WHITE;
            }
            printf("Block:%3d w:%6d h:%6d x:%6d y:%6d coloor:%6d\r\n",val,blk[val].blockw,blk[val].blockh,blk[val].blockx,blk[val].blocky,blk[val].color);
        }
        printf("\r\n");

        for(fail=0; fail<3; fail++) //�����������ݣ�����ʾ
        {
            ran = Get_Adc(0)%game.rank;
            for(val=game.row*game.rank; val<(game.row+1)*game.rank; val++) //�������Ϸ�������ɫ
            {
                if((val - game.row*game.rank) == ran)
                {
                    blk[val].color = (Get_Adc(0)%16) << 12 | (Get_Adc(0)%16) << 8 | (Get_Adc(0)%16) << 4 | (Get_Adc(0)%16);//���ɫ��
                    //blk[val].color = BLACK;
                }
                else
                {
                    blk[val].color = WHITE;//Ĭ�ϰ�ɫ
                }
            }
            update = 0;
            for(val=0; val<game.row*game.rank; val++) //���½����е���ɫ
            {
                if(blk[val].color != blk[val + game.rank].color)//ѡ�������ɫ
                {
                    blk[val].color = blk[val + game.rank].color;
                    update |= 0x01 << val;
                }
            }

            printf("\r\n");
        }
        //��ʾ��ɫ
        for(val=0; val<game.row*game.rank; val++) //���½����е���ɫ
        {
            if(update & (0x01 << val))
                Show_Block(blk[val].blockx,blk[val].blocky,blk[val].blockw,blk[val].blockh,blk[val].color);//��ʾ��ɫ
        }
        Show_Line();
        for(fail = game.rank; fail<game.rank*2; fail++) //���ҿ�ʼ�飬��ʾ��ʼ
        {
            if(blk[fail].color != WHITE)
            {
                POINT_COLOR = ~blk[fail].color;
                LCD_ShowString(blk[fail].blockx + (blk[fail].blockw - strlen((const char *)"Start") * 24 / 2) / 2,\
                               blk[fail].blocky + (blk[fail].blockh - 24) / 2,blk[fail].blockw,blk[fail].blockh,24,(u8 *)"Start");
            }
        }
        POINT_COLOR = BLACK;
        fail = 0;
        while(1)
        {
            while(1)//�ȴ���������
            {
                key = Button_Check(0,0);
                if((0xEE != key) && (key >= game.rank*(game.row - 1 -1)) && (key < game.rank*(game.row -1)))//��Ҫ����������
                {
                    uint8_t ow,ank;
                    ow = key/game.rank;
                    ank = key%game.rank;
                    if(blk[(game.row - ow - 1) * game.rank + ank].color != WHITE)//����Ҫ�����Ŀ�
                    {
                        blk[(game.row - ow - 1) * game.rank + ank].color = GRAY;
                        Show_Block(blk[(game.row - ow - 1) * game.rank + ank].blockx,blk[(game.row - ow - 1) * game.rank + ank].blocky,\
                                   blk[(game.row - ow - 1) * game.rank + ank].blockw,blk[(game.row - ow - 1) * game.rank + ank].blockh,GRAY);
                        game.score ++;
                        if(mode == 1)//ģʽ1���䣬200���
                        {
                            if(game.score == 200)
                                fail = 1;
                        }
                        else//����ģʽ 65535�����˳�
                        {
                            if(game.score == UINT16_MAX)
                                fail = 1;
                        }
                        break;
                    }
                    else//��˸ʧ��
                    {
                        for(fail=3; fail<6; fail++)
                        {
                            Show_Block(blk[(game.row - ow - 1) * game.rank + ank].blockx,blk[(game.row - ow - 1) * game.rank + ank].blocky,\
                                       blk[(game.row - ow - 1) * game.rank + ank].blockw,blk[(game.row - ow - 1) * game.rank + ank].blockh,RED);
                            delay_ms(150);
                            Show_Block(blk[(game.row - ow - 1) * game.rank + ank].blockx,blk[(game.row - ow - 1) * game.rank + ank].blocky,\
                                       blk[(game.row - ow - 1) * game.rank + ank].blockw,blk[(game.row - ow - 1) * game.rank + ank].blockh,WHITE);
                            Show_Line();
                            delay_ms(150);
                        }
                        fail = 2;
                        break;
                    }
                }
                delay_ms(2);
            }
            if(0 != fail)//ʧ���˳�
            {
                break;
            }
            ran = Get_Adc(0)%game.rank;
            for(val=game.row*game.rank; val<(game.row+1)*game.rank; val++) //�������Ϸ�������ɫ
            {
                if((val - game.row*game.rank) == ran)
                {
                    blk[val].color = (Get_Adc(0)%16) << 12 | (Get_Adc(0)%16) << 8 | (Get_Adc(0)%16) << 4 | (Get_Adc(0)%16);//���ɫ��
                    //blk[val].color = BLACK;
                }
                else
                {
                    blk[val].color = WHITE;
                }
            }
            update = 0;
            for(val=0; val<game.row*game.rank; val++) //���½����е���ɫ
            {
                if(blk[val].color != blk[val + game.rank].color)
                {
                    blk[val].color = blk[val + game.rank].color;
                    update |= 0x01 << val;
                }
            }
            //��ʾ��ɫ
            for(val=0; val<game.row*game.rank; val++) //���½����е���ɫ
            {
                if(update & (0x01 << val))
                    Show_Block(blk[val].blockx,blk[val].blocky,blk[val].blockw,blk[val].blockh,blk[val].color);
            }
            Show_Line();
        }
        if(fail == 1)
        {
            POINT_COLOR = BLUE;
            LCD_ShowString(10,50,128,24,24,"Game Win ");//����
        }
        else if(fail == 2)
        {
            POINT_COLOR = RED;
            LCD_ShowString(10,50,128,24,24,"Game Over");//����
        }

        LCD_ShowString(10,80,128,24,24,"Score:");
        LCD_ShowNum(82,80,game.score,5,24);
        delay_ms(1500);
        delay_ms(1500);
        delay_ms(1500);
    }
    else if(mode == 2)//�ֻ�ģʽ
    {
        //
    }
    else if(mode == 3)//����ģʽ
    {
        //
    }
}

void game_mode(void)
{
    //ѡ��ģʽ
}
