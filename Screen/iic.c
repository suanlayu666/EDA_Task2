#include "iic.h"
#include "screen_delay.h"

/*******************************************************************************
* 函 数 名         : IIC_Init
* 函数功能		   : IIC初始化
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/
void IIC_Init(void)
{
    GPIO_InitTypeDef pins = {0};
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8 | GPIO_PIN_9, GPIO_PIN_SET);
    pins.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    pins.Mode = GPIO_MODE_OUTPUT_OD;
    pins.Pull = GPIO_NOPULL;  // 板载上拉。
    pins.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &pins);
}

/*******************************************************************************
* 函 数 名         : IIC_Start
* 函数功能		   : 产生IIC起始信号   
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/
void IIC_Start(void)
{
	IIC_SDA=1;	  	  
	IIC_SCL=1;
	Screen_DelayUs(5);
 	IIC_SDA=0;//START:when CLK is high,DATA change form high to low 
	Screen_DelayUs(6);
	IIC_SCL=0;//钳住I2C总线，准备发送或接收数据 
}	

/*******************************************************************************
* 函 数 名         : IIC_Stop
* 函数功能		   : 产生IIC停止信号   
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/
void IIC_Stop(void)
{
	IIC_SCL=0;
	IIC_SDA=0;//STOP:when CLK is high DATA change form low to high
 	IIC_SCL=1; 
	Screen_DelayUs(6); 
	IIC_SDA=1;//发送I2C总线结束信号
	Screen_DelayUs(6);							   	
}

/*******************************************************************************
* 函 数 名         : IIC_Wait_Ack
* 函数功能		   : 等待应答信号到来   
* 输    入         : 无
* 输    出         : 1，接收应答失败
        			 0，接收应答成功
*******************************************************************************/
u8 IIC_Wait_Ack(void)
{
	u8 tempTime=0;
	IIC_SDA=1;
	Screen_DelayUs(1);	   
	IIC_SCL=1;
	Screen_DelayUs(1);	 
	while(READ_SDA)
	{
		tempTime++;
		if(tempTime>250)
		{
			IIC_Stop();
			return 1;
		}
	}
	IIC_SCL=0;//时钟输出0 	   
	return 0;  
} 

/*******************************************************************************
* 函 数 名         : IIC_Ack
* 函数功能		   : 产生ACK应答  
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/
void IIC_Ack(void)
{
	IIC_SCL=0;
	IIC_SDA=0;
	Screen_DelayUs(2);
	IIC_SCL=1;
	Screen_DelayUs(5);
	IIC_SCL=0;
}

/*******************************************************************************
* 函 数 名         : IIC_NAck
* 函数功能		   : 产生NACK非应答  
* 输    入         : 无
* 输    出         : 无
*******************************************************************************/		    
void IIC_NAck(void)
{
	IIC_SCL=0;
	IIC_SDA=1;
	Screen_DelayUs(2);
	IIC_SCL=1;
	Screen_DelayUs(5);
	IIC_SCL=0;
}	

/*******************************************************************************
* 函 数 名         : IIC_Send_Byte
* 函数功能		   : IIC发送一个字节 
* 输    入         : txd：发送一个字节
* 输    出         : 无
*******************************************************************************/		  
void IIC_Send_Byte(u8 txd)
{                        
    u8 t;   
    IIC_SCL=0;//拉低时钟开始数据传输
    for(t=0;t<8;t++)
    {              
        if((txd&0x80)>0) //0x80  1000 0000
			IIC_SDA=1;
		else
			IIC_SDA=0;
        txd<<=1; 	  
		Screen_DelayUs(2);   //对TEA5767这三个延时都是必须的
		IIC_SCL=1;
		Screen_DelayUs(2); 
		IIC_SCL=0;	
		Screen_DelayUs(2);
    }	 
} 

/*******************************************************************************
* 函 数 名         : IIC_Read_Byte
* 函数功能		   : IIC读一个字节 
* 输    入         : ack=1时，发送ACK，ack=0，发送nACK 
* 输    出         : 应答或非应答
*******************************************************************************/  
u8 IIC_Read_Byte(u8 ack)
{
	u8 i,receive=0;
    IIC_SDA=1;
    for(i=0;i<8;i++ )
	{
        IIC_SCL=0; 
        Screen_DelayUs(2);
		IIC_SCL=1;
        receive<<=1;
        if(READ_SDA)receive++;   
		Screen_DelayUs(1); 
    }					 
    if (!ack)
        IIC_NAck();//发送nACK
    else
        IIC_Ack(); //发送ACK   
    return receive;
}






