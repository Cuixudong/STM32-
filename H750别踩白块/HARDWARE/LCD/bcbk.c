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

uint8_t colorful = 0;//0 黑色快 1:彩色块

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
    if(tp_dev.sta&TP_PRES_DOWN)			//触摸屏被按下
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
    LCD_Fill(x,y,x+w,y+h,color);//填充矩形
}

void Show_Line(void)
{
    uint8_t i;
    POINT_COLOR = BLACK;
    for(i=1; i<game.row; i++)
    {
        LCD_DrawLine(0,i*(game.screen_h/game.row),game.screen_w,i*(game.screen_h/game.row));//画横线
    }
    for(i=1; i<game.rank; i++)
    {
        LCD_DrawLine(i*(game.screen_w/game.rank),0,i*(game.screen_w/game.rank),game.screen_h);//画竖线
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
    if(mode <= 1)//禅模式
    {
        Init(&game);
        for(val=0; val<game.row*game.rank; val++) //初始化坐标颜色尺寸信息
        {
            blk[val].blockw = game.screen_w/game.rank;
            blk[val].blockh = game.screen_h/game.row;
            blk[val].blockx = (val%game.rank) * blk[val].blockw;
            blk[val].blocky = game.screen_h - (game.screen_h - (game.row - (val/game.rank) - 1) * blk[val].blockh);
            if(val>((game.row*(game.rank-1))-1))//最后一行黄色
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

        for(fail=0; fail<3; fail++) //更新三行数据，不显示
        {
            ran = Get_Adc(0)%game.rank;
            for(val=game.row*game.rank; val<(game.row+1)*game.rank; val++) //更新最上方最新颜色
            {
                if((val - game.row*game.rank) == ran)
                {
                    blk[val].color = (Get_Adc(0)%16) << 12 | (Get_Adc(0)%16) << 8 | (Get_Adc(0)%16) << 4 | (Get_Adc(0)%16);//随机色彩
                    //blk[val].color = BLACK;
                }
                else
                {
                    blk[val].color = WHITE;//默认白色
                }
            }
            update = 0;
            for(val=0; val<game.row*game.rank; val++) //更新界面中的颜色
            {
                if(blk[val].color != blk[val + game.rank].color)//选择更新颜色
                {
                    blk[val].color = blk[val + game.rank].color;
                    update |= 0x01 << val;
                }
            }

            printf("\r\n");
        }
        //显示颜色
        for(val=0; val<game.row*game.rank; val++) //更新界面中的颜色
        {
            if(update & (0x01 << val))
                Show_Block(blk[val].blockx,blk[val].blocky,blk[val].blockw,blk[val].blockh,blk[val].color);//显示颜色
        }
        Show_Line();
        for(fail = game.rank; fail<game.rank*2; fail++) //查找开始块，显示开始
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
            while(1)//等待触摸按下
            {
                key = Button_Check(0,0);
                if((0xEE != key) && (key >= game.rank*(game.row - 1 -1)) && (key < game.rank*(game.row -1)))//需要消除的行列
                {
                    uint8_t ow,ank;
                    ow = key/game.rank;
                    ank = key%game.rank;
                    if(blk[(game.row - ow - 1) * game.rank + ank].color != WHITE)//是需要消除的块
                    {
                        blk[(game.row - ow - 1) * game.rank + ank].color = GRAY;
                        Show_Block(blk[(game.row - ow - 1) * game.rank + ank].blockx,blk[(game.row - ow - 1) * game.rank + ank].blocky,\
                                   blk[(game.row - ow - 1) * game.rank + ank].blockw,blk[(game.row - ow - 1) * game.rank + ank].blockh,GRAY);
                        game.score ++;
                        if(mode == 1)//模式1经典，200完成
                        {
                            if(game.score == 200)
                                fail = 1;
                        }
                        else//无限模式 65535分数退出
                        {
                            if(game.score == UINT16_MAX)
                                fail = 1;
                        }
                        break;
                    }
                    else//闪烁失败
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
            if(0 != fail)//失败退出
            {
                break;
            }
            ran = Get_Adc(0)%game.rank;
            for(val=game.row*game.rank; val<(game.row+1)*game.rank; val++) //更新最上方最新颜色
            {
                if((val - game.row*game.rank) == ran)
                {
                    blk[val].color = (Get_Adc(0)%16) << 12 | (Get_Adc(0)%16) << 8 | (Get_Adc(0)%16) << 4 | (Get_Adc(0)%16);//随机色彩
                    //blk[val].color = BLACK;
                }
                else
                {
                    blk[val].color = WHITE;
                }
            }
            update = 0;
            for(val=0; val<game.row*game.rank; val++) //更新界面中的颜色
            {
                if(blk[val].color != blk[val + game.rank].color)
                {
                    blk[val].color = blk[val + game.rank].color;
                    update |= 0x01 << val;
                }
            }
            //显示颜色
            for(val=0; val<game.row*game.rank; val++) //更新界面中的颜色
            {
                if(update & (0x01 << val))
                    Show_Block(blk[val].blockx,blk[val].blocky,blk[val].blockw,blk[val].blockh,blk[val].color);
            }
            Show_Line();
        }
        if(fail == 1)
        {
            POINT_COLOR = BLUE;
            LCD_ShowString(10,50,128,24,24,"Game Win ");//结束
        }
        else if(fail == 2)
        {
            POINT_COLOR = RED;
            LCD_ShowString(10,50,128,24,24,"Game Over");//结束
        }

        LCD_ShowString(10,80,128,24,24,"Score:");
        LCD_ShowNum(82,80,game.score,5,24);
        delay_ms(1500);
        delay_ms(1500);
        delay_ms(1500);
    }
    else if(mode == 2)//街机模式
    {
        //
    }
    else if(mode == 3)//经典模式
    {
        //
    }
}

void game_mode(void)
{
    //选择模式
}
