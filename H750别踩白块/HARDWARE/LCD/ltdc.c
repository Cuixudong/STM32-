#include "lcd.h"
#include "ltdc.h"
#include "delay.h"
#include "sdram.h"
//////////////////////////////////////////////////////////////////////////////////
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32H7开发板
//LTDC 驱动
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//创建日期:2019/4/21
//版本：V1.1
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2014-2024
//All rights reserved
//********************************************************************************
//无
//////////////////////////////////////////////////////////////////////////////////

//根据不同的颜色格式,定义帧缓存数组
#if LCD_PIXFORMAT==LCD_PIXFORMAT_ARGB8888||LCD_PIXFORMAT==LCD_PIXFORMAT_RGB888
u32 ltdc_lcd_framebuf[1280][800] __attribute__((at(LCD_FRAME_BUF_ADDR)));	//定义最大屏分辨率时,LCD所需的帧缓存数组大小
#else
u16 ltdc_lcd_framebuf[1280][800] __attribute__((at(LCD_FRAME_BUF_ADDR)));	//定义最大屏分辨率时,LCD所需的帧缓存数组大小
#endif

u32 *ltdc_framebuf[2];					//LTDC LCD帧缓存数组指针,必须指向对应大小的内存区域
_ltdc_dev lcdltdc;						//管理LCD LTDC的重要参数

//LTDC开关
//sw:1,打开;0,关闭
void LTDC_Switch(u8 sw)
{
    if(sw==1)LTDC->GCR|=1<<0;			//打开LTDC
    else if(sw==0)LTDC->GCR&=~(1<<0);	//关闭LTDC
}

//开关指定层
//layerx:层号;0,第一层;1,第二层;
//sw:1,打开;0,关闭
void LTDC_Layer_Switch(u8 layerx,u8 sw)
{
    if(sw==1)
    {
        if(layerx==0)LTDC_Layer1->CR|=1<<0;		//开启层1
        else LTDC_Layer2->CR|=1<<0;				//开启层2
    } else if(sw==0)
    {
        if(layerx==0)LTDC_Layer1->CR&=~(1<<0);	//关闭层1
        else LTDC_Layer2->CR&=~(1<<0);			//关闭层2
    }
    LTDC->SRCR|=1<<0;							//重新加载配置
}

//选择层
//layerx:层号;0,第一层;1,第二层;
void LTDC_Select_Layer(u8 layerx)
{
    lcdltdc.activelayer=layerx;
}

//设置LCD显示方向
//dir:0,竖屏；1,横屏
void LTDC_Display_Dir(u8 dir)
{
    lcdltdc.dir=dir; 	//显示方向
    if(dir==0)			//竖屏
    {
        lcdltdc.width=lcdltdc.pheight;
        lcdltdc.height=lcdltdc.pwidth;
    } else if(dir==1)	//横屏
    {
        lcdltdc.width=lcdltdc.pwidth;
        lcdltdc.height=lcdltdc.pheight;
    }
}

//画点函数
//x,y:写入坐标
//color:颜色值
void LTDC_Draw_Point(u16 x,u16 y,u32 color)
{
#if LCD_PIXFORMAT==LCD_PIXFORMAT_ARGB8888||LCD_PIXFORMAT==LCD_PIXFORMAT_RGB888
    if(lcdltdc.dir)	//横屏
    {
        *(u32*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*y+x))=color;
    } else 			//竖屏
    {
        *(u32*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*(lcdltdc.pheight-x-1)+y))=color;
    }
#else
    if(lcdltdc.dir)	//横屏
    {
        *(u16*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*y+x))=color;
    } else 			//竖屏
    {
        *(u16*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*(lcdltdc.pheight-x-1)+y))=color;
    }
#endif
}

//读点函数
//x,y:读取点的坐标
//返回值:颜色值
u32 LTDC_Read_Point(u16 x,u16 y)
{
#if LCD_PIXFORMAT==LCD_PIXFORMAT_ARGB8888||LCD_PIXFORMAT==LCD_PIXFORMAT_RGB888
    if(lcdltdc.dir)	//横屏
    {
        return *(u32*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*y+x));
    } else 			//竖屏
    {
        return *(u32*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*(lcdltdc.pheight-x-1)+y));
    }
#else
    if(lcdltdc.dir)	//横屏
    {
        return *(u16*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*y+x));
    } else 			//竖屏
    {
        return *(u16*)((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*(lcdltdc.pheight-x-1)+y));
    }
#endif
}

//LTDC填充矩形,DMA2D填充
//(sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1)
//注意:sx,ex,不能大于lcddev.width-1;sy,ey,不能大于lcddev.height-1!!!
//color:要填充的颜色
void LTDC_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u32 color)
{
    u32 psx,psy,pex,pey;	//以LCD面板为基准的坐标系,不随横竖屏变化而变化
    u32 timeout=0;
    u16 offline;
    u32 addr;
    //坐标系转换
    if(lcdltdc.dir)	//横屏
    {
        psx=sx;
        psy=sy;
        pex=ex;
        pey=ey;
    } else			//竖屏
    {
        psx=sy;
        psy=lcdltdc.pheight-ex-1;
        pex=ey;
        pey=lcdltdc.pheight-sx-1;
    }
    offline=lcdltdc.pwidth-(pex-psx+1);
    addr=((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*psy+psx));
    RCC->AHB3ENR|=1<<4;				//使能DMA2D时钟
    DMA2D->CR&=~(1<<0);				//先停止DMA2D
    DMA2D->CR=3<<16;				//寄存器到存储器模式
    DMA2D->OPFCCR=LCD_PIXFORMAT;	//设置颜色格式
    DMA2D->OOR=offline;				//设置行偏移
    DMA2D->OMAR=addr;				//输出存储器地址
    DMA2D->NLR=(pey-psy+1)|((pex-psx+1)<<16);	//设定行数寄存器
    DMA2D->OCOLR=color;				//设定输出颜色寄存器
    DMA2D->CR|=1<<0;				//启动DMA2D
    while((DMA2D->ISR&(1<<1))==0)	//等待传输完成
    {
        timeout++;
        if(timeout>0X1FFFFF)break;	//超时退出
    }
    DMA2D->IFCR|=1<<1;				//清除传输完成标志
}

//在指定区域内填充指定颜色块,DMA2D填充
//此函数仅支持u16,RGB565格式的颜色数组填充.
//(sx,sy),(ex,ey):填充矩形对角坐标,区域大小为:(ex-sx+1)*(ey-sy+1)
//注意:sx,ex,不能大于lcddev.width-1;sy,ey,不能大于lcddev.height-1!!!
//color:要填充的颜色数组首地址
void LTDC_Color_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 *color)
{
    u32 psx,psy,pex,pey;	//以LCD面板为基准的坐标系,不随横竖屏变化而变化
    u32 timeout=0;
    u16 offline;
    u32 addr;
    //坐标系转换
    if(lcdltdc.dir)	//横屏
    {
        psx=sx;
        psy=sy;
        pex=ex;
        pey=ey;
    } else			//竖屏
    {
        psx=sy;
        psy=lcdltdc.pheight-ex-1;
        pex=ey;
        pey=lcdltdc.pheight-sx-1;
    }
    offline=lcdltdc.pwidth-(pex-psx+1);
    addr=((u32)ltdc_framebuf[lcdltdc.activelayer]+lcdltdc.pixsize*(lcdltdc.pwidth*psy+psx));
    RCC->AHB3ENR|=1<<4;				//使能DMA2D时钟
    DMA2D->CR&=~(1<<0);				//先停止DMA2D
    DMA2D->CR=0<<16;				//存储器到存储器模式
    DMA2D->FGPFCCR=LCD_PIXFORMAT;	//设置颜色格式
    DMA2D->FGOR=0;					//前景层行偏移为0
    DMA2D->OOR=offline;				//设置行偏移
    DMA2D->FGMAR=(u32)color;		//源地址
    DMA2D->OMAR=addr;				//输出存储器地址
    DMA2D->NLR=(pey-psy+1)|((pex-psx+1)<<16);	//设定行数寄存器
    DMA2D->CR|=1<<0;				//启动DMA2D
    while((DMA2D->ISR&(1<<1))==0)	//等待传输完成
    {
        timeout++;
        if(timeout>0X1FFFFF)break;	//超时退出
    }
    DMA2D->IFCR|=1<<1;				//清除传输完成标志
}

//LCD清屏
//color:颜色值
void LTDC_Clear(u32 color)
{
    LTDC_Fill(0,0,lcdltdc.width-1,lcdltdc.height-1,color);
}

//LTDC时钟(Fdclk)设置函数
//Fvco=Fs*(pll3n/pll3m);
//Fr=Fvco/pll3r=Fs*(pll3n/(pll3m*pll3r));
//Fdclk=Fr;

//Fvco:VCO频率
//Fr:PLL3的r分频输出时钟频率
//Fs:PLL3输入时钟频率,可以是HSI,CSI,HSE等.
//Fdclk:LTDC的输入时钟

//pll3n:PLL3倍频系数(PLL倍频),取值范围:4~512.
//pll3m:PLL3预分频系数(进PLL之前的分频),取值范围:2~63.
//pll3r:PLL3的r分频系数(PLL之后的分频),取值范围:1~128.

//因为LTDC和SAI共用pll3,所以,当使用SAI的时候,会切换不同的pll3n,导致LTDC的plln也变化了,
//因此这里设置为160,得到的像素时钟在后面SAI修改pll3n的时候,会有变化.需要根据SAI的设置调整
//本例程，LTDC,我们一般设置pll3m=5,pll3n=160,这样,可以得到Fvco=800Mhz
//然后,只需要通过修改pll3r,来匹配不同的液晶屏时序即可.

//例如:要得到33M的LTDC时钟,则可以设置:pll3n=160,pll3m=5,pll3r=24
//Fdclk=160*(25/5)/24=800/24=33.33Mhz
//返回值:0,成功;1,失败。
u8 LTDC_Clk_Set(u32 pll3n,u32 pll3m,u32 pll3r)
{
    u16 retry=0;
    u8 status=0;
    RCC->CR&=~(1<<28);				//关闭PLL3时钟
    while(((RCC->CR&(1<<29)))&&(retry<0X1FFF))retry++;//等待PLL3时钟失锁
    if(retry==0X1FFF)status=1;		//PLL3时钟关闭失败
    else
    {
        RCC->PLLCKSELR&=~(0X3F<<20);	//清除DIVM3[5:0]原来的设置
        RCC->PLLCKSELR|=pll3m<<20;		//DIVM3[5:0]=25,设置PLL3的预分频系数
        RCC->PLL3DIVR&=~(0X1FF<<0);		//清除DIVN3[8:0]原来的设置
        RCC->PLL3DIVR|=(pll3n-1)<<0;	//DIVN3[8:0]=pll3n-1,设置PLL3的倍频系数,设置值需减1
        RCC->PLL3DIVR&=~(0X7F<<24);		//清除DIVR2[6:0]原来的设置
        RCC->PLL3DIVR|=(pll3r-1)<<24;	//DIVR3[8:0]=pll3r-1,设置PLL3的倍频系数,设置值需减1

        RCC->PLLCFGR&=~(0X0F<<8);		//清除PLL3RGE[1:0]/PLL3VCOSEL/PLL3FRACEN的设置
        RCC->PLLCFGR|=0<<10;			//PLL3RGE[1:0]=0,PLL3输入时钟频率在1~2Mhz之间(25/25=1Mhz)
        RCC->PLLCFGR|=0<<9;				//PLL3VCOSEL=0,PLL3宽的VCO范围,192~836Mhz
        RCC->PLLCFGR|=1<<24;			//DIVR3EN=1,使能pll3_r_ck
        RCC->CR|=1<<28;					//PLL3ON=1,使能PLL3
        while(((RCC->CR&(1<<29))==0)&&(retry<0X1FFF))retry++;	//等待PLL3时钟锁定
        if(retry==0X1FFF)status=2;
    }
    return status;
}
//LTDC,层窗口设置,窗口以LCD面板坐标系为基准
//注意:此函数必须在LTDC_Layer_Parameter_Config之后再设置.另外,当设置的窗口值不等于面板的尺
//寸时,GRAM的操作(读/写点函数),也要根据窗口的宽高来进行修改,否则显示不正常(本例程就未做修改).
//layerx:层值,0/1.
//sx,sy:起始坐标
//width,height:宽度和高度
//pixformat:颜色格式.0,ARGB8888;1,RGB888;2,RGB565;3,ARGB1555;4,ARGB4444;5,L8;6;AL44;7;AL88
void LTDC_Layer_Window_Config(u8 layerx,u16 sx,u16 sy,u16 width,u16 height,u8 pixformat)
{
    u32 temp;
    if(layerx==0)
    {
        temp=(sx+width+((LTDC->BPCR&0X0FFF0000)>>16))<<16;
        LTDC_Layer1->WHPCR=(sx+((LTDC->BPCR&0X0FFF0000)>>16)+1)|temp;//设置行起始和结束位置
        temp=(sy+height+(LTDC->BPCR&0X7FF))<<16;
        LTDC_Layer1->WVPCR=(sy+(LTDC->BPCR&0X7FF)+1)|temp;		//设置列起始和结束位置
        if(pixformat==0)temp=4;									//ARGB8888,一个点4个字节
        else if(pixformat==1)temp=3;							//RGB888,一个点3个字节
        else if(pixformat==5||pixformat==6)temp=1;				//L8/AL44,一个点1个字节
        else temp=2;											//其他格式,一个点2个字节
        LTDC_Layer1->CFBLR=(width*temp<<16)|(width*temp+7);		//帧缓冲区长度设置(字节为单位)
        LTDC_Layer1->CFBLNR=height;								//帧缓冲区行数设置
    } else
    {
        temp=(sx+width+((LTDC->BPCR&0X0FFF0000)>>16))<<16;
        LTDC_Layer2->WHPCR=(sx+((LTDC->BPCR&0X0FFF0000)>>16)+1)|temp;//设置行起始和结束位置
        temp=(sy+height+(LTDC->BPCR&0X7FF))<<16;
        LTDC_Layer2->WVPCR=(sy+(LTDC->BPCR&0X7FF)+1)|temp;		//设置列起始和结束位置
        if(pixformat==0)temp=4;									//ARGB8888,一个点4个字节
        else if(pixformat==1)temp=3;							//RGB888,一个点3个字节
        else if(pixformat==5||pixformat==6)temp=1;				//L8/AL44,一个点1个字节
        else temp=2;											//其他格式,一个点2个字节
        LTDC_Layer2->CFBLR=(width*temp<<16)|(width*temp+7);		//帧缓冲区长度设置(字节为单位)
        LTDC_Layer2->CFBLNR=height;								//帧缓冲区行数设置
    }
    LTDC_Layer_Switch(layerx,1);								//层使能
}

//LTDC,基本参数设置.
//注意:此函数,必须在LTDC_Layer_Window_Config之前设置.
//layerx:层值,0/1.
//bufaddr:层颜色帧缓存起始地址
//pixformat:颜色格式.0,ARGB8888;1,RGB888;2,RGB565;3,ARGB1555;4,ARGB4444;5,L8;6;AL44;7;AL88
//alpha:层颜色Alpha值,0,全透明;255,不透明
//alpha0:默认颜色Alpha值,0,全透明;255,不透明
//bfac1:混合系数1,4(100),恒定的Alpha;6(101),像素Alpha*恒定Alpha
//bfac2:混合系数2,5(101),恒定的Alpha;7(111),像素Alpha*恒定Alpha
//bkcolor:层默认颜色,32位,低24位有效,RGB888格式
//返回值:无
void LTDC_Layer_Parameter_Config(u8 layerx,u32 bufaddr,u8 pixformat,u8 alpha,u8 alpha0,u8 bfac1,u8 bfac2,u32 bkcolor)
{
    if(layerx==0)
    {
        LTDC_Layer1->CFBAR=bufaddr;					//设置层颜色帧缓存起始地址
        LTDC_Layer1->PFCR=pixformat;				//设置层颜色格式
        LTDC_Layer1->CACR=alpha;					//设置层颜色Alpha值,255分频;设置255,则不透明
        LTDC_Layer1->DCCR=((u32)alpha0<<24)|bkcolor;//设置默认颜色Alpha值,以及默认颜色
        LTDC_Layer1->BFCR=((u32)bfac1<<8)|bfac2;	//设置层混合系数
    } else
    {
        LTDC_Layer2->CFBAR=bufaddr;					//设置层颜色帧缓存起始地址
        LTDC_Layer2->PFCR=pixformat;				//设置层颜色格式
        LTDC_Layer2->CACR=alpha;					//设置层颜色Alpha值,255分频;设置255,则不透明
        LTDC_Layer2->DCCR=((u32)alpha0<<24)|bkcolor;//设置默认颜色Alpha值,以及默认颜色
        LTDC_Layer2->BFCR=((u32)bfac1<<8)|bfac2;	//设置层混合系数
    }
}

//读取面板参数
//PG6=R7(M0);PI2=G7(M1);PI7=B7(M2);
//M2:M1:M0
//0 :0 :0	//4.3寸480*272 RGB屏,ID=0X4342
//0 :0 :1	//7寸800*480 RGB屏,ID=0X7084
//0 :1 :0	//7寸1024*600 RGB屏,ID=0X7016
//0 :1 :1	//7寸1280*800 RGB屏,ID=0X7018
//1 :0 :0	//8寸1024*600 RGB屏,ID=0X8016
//1 :0 :1	//10.1寸1280*800 RGB屏,ID=0X1018
//返回值:LCD ID:0,非法;其他值,ID;
u16 LTDC_PanelID_Read(void)
{
    u8 idx=0;
    RCC->AHB4ENR|=1<<9|1<<10; 			//使能PJ/PK时钟
    GPIO_Set(GPIOJ,PIN6,GPIO_MODE_IN,0,0,GPIO_PUPD_PU); 			//PJ6上拉输入
    GPIO_Set(GPIOK,PIN2|PIN6,GPIO_MODE_IN,0,0,GPIO_PUPD_PU); 		//PK2/PK6上拉输入
    delay_ms(50);
    idx=GPIO_Pin_Get(GPIOJ,PIN6);		//读取M1
    idx|=GPIO_Pin_Get(GPIOK,PIN2)<<1;	//读取M0
    idx|=GPIO_Pin_Get(GPIOK,PIN6)<<2;	//读取M2
    switch(idx)
    {
    case 0:
        return 0X4342;		//4.3寸屏,480*272分辨率
    case 1:
        return 0X7084;		//7寸屏,800*480分辨率
    case 2:
        return 0X7016;		//7寸屏,1024*600分辨率
    case 3:
        return 0X7018;		//7寸屏,1280*800分辨率
    case 4:
        return 0X8016; 		//8寸屏,1024*600分辨率
    case 5:
        return 0X1018; 		//10.1寸屏,1280*800分辨率
    default:
        return 0;
    }
}

//LTDC初始化函数
void LTDC_Init(void)
{
    u32 tempreg=0;
    u16 lcdid=0;

    lcdid=LTDC_PanelID_Read();		//读取LCD面板ID  PE
    if(lcdid==0)return;				//非RGB屏
    SDRAM_RGBLCD_Set();				//针对RGB屏,初始化SDRAM,提速

    RCC->APB3ENR|=1<<3;				//开启LTDC时钟
    RCC->AHB4ENR|=1<<1;				//使能PORTB时钟
    RCC->AHB4ENR|=0X7<<8; 			//使能PI/PJ/PK时钟
    GPIO_Set(GPIOB,PIN5,GPIO_MODE_OUT,GPIO_OTYPE_PP,GPIO_SPEED_MID,GPIO_PUPD_PU);				//PB5 推挽输出,控制背光
    GPIO_Set(GPIOI,7<<12,GPIO_MODE_AF,GPIO_OTYPE_PP,GPIO_SPEED_HIGH,GPIO_PUPD_PU);				//PI12/13/14
    GPIO_Set(GPIOJ,(0X1F<<2)|(0X07<<9)|(1<<15),GPIO_MODE_AF,GPIO_OTYPE_PP,GPIO_SPEED_HIGH,GPIO_PUPD_PU);//PJ2~6,9~11,15
    GPIO_Set(GPIOK,0XFF,GPIO_MODE_AF,GPIO_OTYPE_PP,GPIO_SPEED_HIGH,GPIO_PUPD_PU);				//PK0~7



    GPIO_AF_Set(GPIOI,12,14);		//PI12,AF14
    GPIO_AF_Set(GPIOI,13,14);		//PI13,AF14
    GPIO_AF_Set(GPIOI,14,14);		//PI14,AF14

    GPIO_AF_Set(GPIOJ,2,14);		//PJ2,AF14
    GPIO_AF_Set(GPIOJ,3,14);		//PJ3,AF14
    GPIO_AF_Set(GPIOJ,4,14);		//PJ4,AF14
    GPIO_AF_Set(GPIOJ,5,14);		//PJ5,AF14
    GPIO_AF_Set(GPIOJ,6,14);		//PJ6,AF14
    GPIO_AF_Set(GPIOJ,9,14);		//PJ9,AF14
    GPIO_AF_Set(GPIOJ,10,14);		//PJ10,AF14
    GPIO_AF_Set(GPIOJ,11,14);		//PJ11,AF14
    GPIO_AF_Set(GPIOJ,15,14);		//PJ15,AF14

    GPIO_AF_Set(GPIOK,0,14);		//PK0,AF14
    GPIO_AF_Set(GPIOK,1,14);		//PK1,AF14
    GPIO_AF_Set(GPIOK,2,14);		//PK2,AF14
    GPIO_AF_Set(GPIOK,3,14);		//PK3,AF14
    GPIO_AF_Set(GPIOK,4,14);		//PK4,AF14
    GPIO_AF_Set(GPIOK,5,14);		//PK5,AF14
    GPIO_AF_Set(GPIOK,6,14);		//PK6,AF14
    GPIO_AF_Set(GPIOK,7,14);		//PK7,AF14



    if(lcdid==0X4342)
    {
        lcdltdc.pwidth=480;			//面板宽度,单位:像素
        lcdltdc.pheight=272;		//面板高度,单位:像素
        lcdltdc.hsw=1;				//水平同步宽度
        lcdltdc.vsw=1;				//垂直同步宽度
        lcdltdc.hbp=40;				//水平后廊
        lcdltdc.vbp=8;				//垂直后廊
        lcdltdc.hfp=5;				//水平前廊
        lcdltdc.vfp=8;				//垂直前廊
        LTDC_Clk_Set(160,5,88);		//设置像素时钟  9Mhz
    } else if(lcdid==0X7084)
    {
        lcdltdc.pwidth=800;			//面板宽度,单位:像素
        lcdltdc.pheight=480;		//面板高度,单位:像素
        lcdltdc.hsw=1;				//水平同步宽度
        lcdltdc.vsw=1;				//垂直同步宽度
        lcdltdc.hbp=46;				//水平后廊
        lcdltdc.vbp=23;				//垂直后廊
        lcdltdc.hfp=210;			//水平前廊
        lcdltdc.vfp=22;				//垂直前廊
        LTDC_Clk_Set(160,5,24);		//设置像素时钟 33M(如果开双显,需要降低DCLK到:16.67Mhz  160*(25/5)/48,才会比较好)
    } else if(lcdid==0X7016)
    {
        lcdltdc.pwidth=1024;		//面板宽度,单位:像素
        lcdltdc.pheight=600;		//面板高度,单位:像素
        lcdltdc.hsw=20;				//水平同步宽度
        lcdltdc.vsw=3;				//垂直同步宽度
        lcdltdc.hbp=140;			//水平后廊
        lcdltdc.vbp=20;				//垂直后廊
        lcdltdc.hfp=160;			//水平前廊
        lcdltdc.vfp=12;				//垂直前廊
        LTDC_Clk_Set(160,5,16);		//设置像素时钟  50Mhz
    } else if(lcdid==0X7018)
    {
        lcdltdc.pwidth=1280;		//面板宽度,单位:像素
        lcdltdc.pheight=800;		//面板高度,单位:像素
        //其他参数待定.
    } else if(lcdid==0X8016)
    {
        lcdltdc.pwidth=1024;		//面板宽度,单位:像素
        lcdltdc.pheight=600;		//面板高度,单位:像素
        //其他参数待定.
    } else if(lcdid==0X1018)			//10.1寸1280*800 RGB屏
    {
        lcdltdc.pwidth=1280;		//面板宽度,单位:像素
        lcdltdc.pheight=800;		//面板高度,单位:像素
        lcdltdc.hbp=140;			//水平后廊
        lcdltdc.hfp=10;			    //水平前廊
        lcdltdc.hsw=10;				//水平同步宽度
        lcdltdc.vbp=10;				//垂直后廊
        lcdltdc.vfp=10;				//垂直前廊
        lcdltdc.vsw=3;				//垂直同步宽度
        LTDC_Clk_Set(160,5,10);		//设置像素时钟  80Mhz
    }
    tempreg=0<<28;					//像素时钟极性:不反向
    if(lcdid==0X1018)tempreg=1<<28;	//像素时钟极性:反向
    tempreg|=0<<29;					//数据使能极性:低电平有效
    tempreg|=0<<30;					//垂直同步极性:低电平有效
    tempreg|=0<<31;					//水平同步极性:低电平有效
    LTDC->GCR=tempreg;				//设置全局控制寄存器
    tempreg=(lcdltdc.vsw-1)<<0;		//垂直脉宽-1
    tempreg|=(lcdltdc.hsw-1)<<16;	//水平脉宽-1
    LTDC->SSCR=tempreg;				//设置同步大小配置寄存器

    tempreg=(lcdltdc.vsw+lcdltdc.vbp-1)<<0;		//累加垂直后沿=垂直脉宽+垂直后沿-1
    tempreg|=(lcdltdc.hsw+lcdltdc.hbp-1)<<16;	//累加水平后沿=水平脉宽+水平后沿-1
    LTDC->BPCR=tempreg;							//设置后沿配置寄存器

    tempreg=(lcdltdc.vsw+lcdltdc.vbp+lcdltdc.pheight-1)<<0;	//累加有效高度=垂直脉宽+垂直后沿+垂直分辨率-1
    tempreg|=(lcdltdc.hsw+lcdltdc.hbp+lcdltdc.pwidth-1)<<16;//累加有效宽度=水平脉宽+水平后沿+水平分辨率-1
    LTDC->AWCR=tempreg;							//设置有效宽度配置寄存器

    tempreg=(lcdltdc.vsw+lcdltdc.vbp+lcdltdc.pheight+lcdltdc.vfp-1)<<0;	//总高度=垂直脉宽+垂直后沿+垂直分辨率+垂直前廊-1
    tempreg|=(lcdltdc.hsw+lcdltdc.hbp+lcdltdc.pwidth+lcdltdc.hfp-1)<<16;//总宽度=水平脉宽+水平后沿+水平分辨率+水平前廊-1
    LTDC->TWCR=tempreg;				//设置总宽度配置寄存器

    LTDC->BCCR=LTDC_BACKLAYERCOLOR;	//设置背景层颜色寄存器(RGB888格式)
    LTDC_Switch(1);					//开启LTDC


#if LCD_PIXFORMAT==LCD_PIXFORMAT_ARGB8888||LCD_PIXFORMAT==LCD_PIXFORMAT_RGB888
    ltdc_framebuf[0]=(u32*)&ltdc_lcd_framebuf;
    lcdltdc.pixsize=4;				//每个像素占4个字节
#else
    ltdc_framebuf[0]=(u32*)&ltdc_lcd_framebuf;
    //ltdc_framebuf[1]=(u32*)&ltdc_lcd_framebuf1;
    lcdltdc.pixsize=2;				//每个像素占2个字节
#endif
    //层配置
    LTDC_Layer_Parameter_Config(0,(u32)ltdc_framebuf[0],LCD_PIXFORMAT,255,0,6,7,0X000000);	//层参数配置
    LTDC_Layer_Window_Config(0,0,0,lcdltdc.pwidth,lcdltdc.pheight,LCD_PIXFORMAT);			//层窗口配置,以LCD面板坐标系为基准,不要随便修改!

    //LTDC_Layer_Parameter_Config(1,(u32)ltdc_framebuf[1],LCD_PIXFORMAT,127,0,6,7,0X000000);//层参数配置
    //LTDC_Layer_Window_Config(1,0,0,lcdltdc.pwidth,lcdltdc.pheight,LCD_PIXFORMAT);			//层窗口配置,以LCD面板坐标系为基准,不要随便修改!



    lcddev.width=lcdltdc.pwidth;	//设置lcddev的宽度参数
    lcddev.height=lcdltdc.pheight; 	//设置lcddev的宽度参数
    //LTDC_Display_Dir(0);			//默认竖屏，在LCD_Init函数里面设置
    LTDC_Select_Layer(0); 			//选择第1层
    LCD_LED(1);         		 	//点亮背光
    LTDC_Clear(0XFFFFFFFF);			//清屏
}
























