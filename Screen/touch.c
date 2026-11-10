#include "touch.h"
#include "24cxx.h"
#include "screen_delay.h"
#include "tftlcd.h"
#include "math.h"


_m_tp_dev tp_dev;
//默认为touchtype=0的数据.
static u8 CMD_RDX=0XD0;
static u8 CMD_RDY=0X90;

//SPI写数据
//向触摸屏IC写入1byte数据
//num:要写入的数据
static void TP_Write_Byte(u8 num)
{
	u8 count=0;
	for(count=0;count<8;count++)
	{
		if(num&0x80)TDIN=1;
		else TDIN=0;
		num<<=1;
		TCLK=0;
		Screen_DelayUs(1);
		TCLK=1;		//上升沿有效
	}
}
//SPI读数据
//从触摸屏IC读取adc值
//CMD:指令
//返回值:读到的数据
static u16 TP_Read_AD(u8 CMD)
{
	u8 count=0;
	u16 Num=0;
	TCLK=0;		//先拉低时钟
	TDIN=0; 	//拉低数据线
	TCS=0; 		//选中触摸屏IC
	TP_Write_Byte(CMD);//发送命令字
	Screen_DelayUs(6);//ADS7846的转换时间最长为6us
	TCLK=0;
	Screen_DelayUs(1);
	TCLK=1;		//给1个时钟，清除BUSY
	Screen_DelayUs(1);
	TCLK=0;
	for(count=0;count<16;count++)//读出16位数据,只有高12位有效
	{
		Num<<=1;
		TCLK=0;	//下降沿有效
		Screen_DelayUs(1);
 		TCLK=1;
 		if(DOUT)Num++;
	}
	Num>>=4;   	//只有高12位有效.
	TCS=1;		//释放片选
	return(Num);
}

//读取一个坐标值(x或者y)
//连续读取READ_TIMES次数据,对这些数据升序排列,
//然后去掉最低和最高LOST_VAL个数,取平均值
//xy:指令（CMD_RDX/CMD_RDY）
//返回值:读到的数据
#define READ_TIMES 40 	//读取次数
#define LOST_VAL 1	  	//丢弃值
static u16 TP_Read_XOY(u8 xy)
{
	u16 i, j;
	u16 buf[READ_TIMES];
	u32 sum=0;
	u16 temp;
	for(i=0;i<READ_TIMES;i++)buf[i]=TP_Read_AD(xy);
	for(i=0;i<READ_TIMES-1; i++)//排序
	{
		for(j=i+1;j<READ_TIMES;j++)
		{
			if(buf[i]>buf[j])//升序排列
			{
				temp=buf[i];
				buf[i]=buf[j];
				buf[j]=temp;
			}
		}
	}
	sum=0;
	for(i=LOST_VAL;i<READ_TIMES-LOST_VAL;i++)sum+=buf[i];
	temp=sum/(READ_TIMES-2*LOST_VAL);
	return temp;
}

//读取x,y坐标
//最小值不能少于100.
//x,y:读取到的坐标值
//返回值:0,失败;1,成功。
static u8 TP_Read_XY(u16 *x,u16 *y)
{
	u16 xtemp,ytemp;
	xtemp=TP_Read_XOY(CMD_RDX);
	ytemp=TP_Read_XOY(CMD_RDY);
	//if(xtemp<100||ytemp<100)return 0;//读数失败
	*x=xtemp;
	*y=ytemp;
	return 1;//读数成功
}
//连续2次读取触摸屏IC,且这两次的偏差不能超过
//ERR_RANGE,满足条件,则认为读数正确,否则读数错误.
//该函数能大大提高准确度
//x,y:读取到的坐标值
//返回值:0,失败;1,成功。
#define ERR_RANGE 50 //误差范围
static u8 TP_Read_XY2(u16 *x,u16 *y)
{
	u16 x1,y1;
 	u16 x2,y2;
 	u8 flag;
    flag=TP_Read_XY(&x1,&y1);
    if(flag==0)return(0);
    flag=TP_Read_XY(&x2,&y2);
    if(flag==0)return(0);
    if(((x2<=x1&&x1<x2+ERR_RANGE)||(x1<=x2&&x2<x1+ERR_RANGE))//前后两次采样在+-50内
    &&((y2<=y1&&y1<y2+ERR_RANGE)||(y1<=y2&&y2<y1+ERR_RANGE)))
    {
        *x=(x1+x2)/2;
        *y=(y1+y2)/2;
        return 1;
    }else return 0;
}
//////////////////////////////////////////////////////////////////////////////////
//触摸按键扫描
//tp:0,屏幕坐标;1,物理坐标(校准等特殊场合用)
//返回值:当前触屏状态.
//0,触屏无触摸;1,触屏有触摸
u8 TP_Scan(u8 tp)
{

    if (PEN == 0)
    {
        u16 raw_x, raw_y;
        if ((tp || tp_dev.xfac != 0.0f) && TP_Read_XY2(&raw_x, &raw_y))
        {
            if (tp)
            {
                tp_dev.x[0] = raw_x;
                tp_dev.y[0] = raw_y;
            }
            else
            {
                int x = (int)(tp_dev.xfac * raw_x + tp_dev.xoff);
                int y = (int)(tp_dev.yfac * raw_y + tp_dev.yoff);
                tp_dev.x[0] = (u16)(x < 0 ? 0 : x >= tftlcd_data.width ? tftlcd_data.width - 1 : x);
                tp_dev.y[0] = (u16)(y < 0 ? 0 : y >= tftlcd_data.height ? tftlcd_data.height - 1 : y);
            }
            tp_dev.sta |= TP_PRES_DOWN;
        }
    }
    else
    {
        tp_dev.sta &= (u16)~TP_PRES_DOWN;
    }
    return tp_dev.sta & TP_PRES_DOWN;
}
//////////////////////////////////////////////////////////////////////////
//校准记录使用 EEPROM 200~213。末字节同时记录格式与屏幕方向。
#define SAVE_ADDR_BASE 200
#define CALIBRATION_MARKER (0xB0U | TFTLCD_DIR)
//保存校准参数
static void TP_Save_Adjdata(void)
{
	int temp;
	AT24CXX_WriteOneByte(SAVE_ADDR_BASE+13, 0);
	//保存校正结果!
	temp=tp_dev.xfac*100000000;//保存x校正因素
    AT24CXX_WriteLenByte(SAVE_ADDR_BASE,temp,4);
	temp=tp_dev.yfac*100000000;//保存y校正因素
    AT24CXX_WriteLenByte(SAVE_ADDR_BASE+4,temp,4);
	//保存x偏移量
    AT24CXX_WriteLenByte(SAVE_ADDR_BASE+8,tp_dev.xoff,2);
	//保存y偏移量
	AT24CXX_WriteLenByte(SAVE_ADDR_BASE+10,tp_dev.yoff,2);
	//保存触屏类型
	AT24CXX_WriteOneByte(SAVE_ADDR_BASE+12,tp_dev.touchtype);
	temp=CALIBRATION_MARKER;//标记校准过了
	AT24CXX_WriteOneByte(SAVE_ADDR_BASE+13,temp);
}
//得到保存在EEPROM里面的校准值
//返回值：1，成功获取数据
//        0，获取失败，要重新校准
static u8 TP_Get_Adjdata(void)
{
	int tempfac;
	tempfac=AT24CXX_ReadOneByte(SAVE_ADDR_BASE+13);//读取标记字,看是否校准过！
	if(tempfac==CALIBRATION_MARKER)//触摸屏已经校准过了
	{
		tempfac=AT24CXX_ReadLenByte(SAVE_ADDR_BASE,4);
		tp_dev.xfac=(float)tempfac/100000000;//得到x校准参数
		tempfac=AT24CXX_ReadLenByte(SAVE_ADDR_BASE+4,4);
		tp_dev.yfac=(float)tempfac/100000000;//得到y校准参数
	    //得到x偏移量
		tp_dev.xoff=AT24CXX_ReadLenByte(SAVE_ADDR_BASE+8,2);
 	    //得到y偏移量
		tp_dev.yoff=AT24CXX_ReadLenByte(SAVE_ADDR_BASE+10,2);
 		tp_dev.touchtype=AT24CXX_ReadOneByte(SAVE_ADDR_BASE+12);//读取触屏类型标记
        if (tp_dev.xfac == 0.0f || tp_dev.yfac == 0.0f ||
            fabsf(tp_dev.xfac) > 2.0f || fabsf(tp_dev.yfac) > 2.0f || tp_dev.touchtype > 1U)
        {
            tp_dev.xfac = 0.0f;
            return 0;
        }
		if(tp_dev.touchtype)//X,Y方向与屏幕相反
		{
			CMD_RDX=0X90;
			CMD_RDY=0XD0;
		}else				   //X,Y方向与屏幕相同
		{
			CMD_RDX=0XD0;
			CMD_RDY=0X90;
		}
		return 1;
	}
	return 0;
}
//触摸屏初始化
//返回值:0,进行过校准
//       1,未进行校准
u8 TP_InitWithoutCalibration(void)
{
    GPIO_InitTypeDef pins = {0};

    Screen_DelayInit();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_11, GPIO_PIN_RESET);

    pins.Mode = GPIO_MODE_OUTPUT_PP;
    pins.Speed = GPIO_SPEED_FREQ_LOW;
    pins.Pin = GPIO_PIN_0;
    HAL_GPIO_Init(GPIOB, &pins);
    pins.Pin = GPIO_PIN_13;
    HAL_GPIO_Init(GPIOC, &pins);
    pins.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOF, &pins);

    pins.Mode = GPIO_MODE_INPUT;
    pins.Pull = GPIO_PULLUP;
    pins.Pin = GPIO_PIN_1 | GPIO_PIN_2;
    HAL_GPIO_Init(GPIOB, &pins);

    CMD_RDX = 0xD0;
    CMD_RDY = 0x90;
    (void)TP_Read_AD(CMD_RDX);  // 使触摸控制器进入可用的 PENIRQ 状态。
    AT24CXX_Init();
    tp_dev.sta = 0;
    tp_dev.xfac = 0.0f;
    return TP_Get_Adjdata() == 0;
}

/* 用四点样本拟合一条屏幕坐标轴的比例和偏移，拒绝不稳定的采样。 */
static u8 FitCalibrationAxis(const u16 samples[4][2], u8 axis, u8 side,
                             u16 first, u16 last, float *scale, short *offset)
{
    const u8 other = 3U ^ side;
    const float raw_first = (samples[0][axis] + samples[other][axis]) * 0.5f;
    const float raw_last = (samples[side][axis] + samples[3][axis]) * 0.5f;
    const float span = raw_last - raw_first;
    if (fabsf(span) < 400.0f)
    {
        return 0;
    }

    const float slope = (last - first) / span;
    const float intercept = first - slope * raw_first;
    for (u8 i = 0; i < 4U; ++i)
    {
        const u16 target = (i & side) ? last : first;
        if (fabsf(slope * samples[i][axis] + intercept - target) > 18.0f)
        {
            return 0;
        }
    }

    *scale = slope;
    *offset = (short)intercept;
    return 1;
}

/* 从四个原始触摸点求出坐标变换，通过校验后保存到 EEPROM。 */
u8 TP_CalibrateFromPoints(const u16 samples[4][2],
                         u16 left, u16 top, u16 right, u16 bottom)
{
    for (u8 swapped = 0; swapped < 2U; ++swapped)
    {
        float x_scale, y_scale;
        short x_offset, y_offset;
        if (!FitCalibrationAxis(samples, swapped, 1U, left, right, &x_scale, &x_offset) ||
            !FitCalibrationAxis(samples, swapped ^ 1U, 2U, top, bottom, &y_scale, &y_offset))
        {
            continue;
        }

        tp_dev.xfac = x_scale;
        tp_dev.yfac = y_scale;
        tp_dev.xoff = x_offset;
        tp_dev.yoff = y_offset;
        tp_dev.touchtype = swapped;
        CMD_RDX = swapped ? 0x90 : 0xD0;
        CMD_RDY = swapped ? 0xD0 : 0x90;
        tp_dev.sta = 0;
        TP_Save_Adjdata();
        return 1;
    }
    return 0;
}
