#include "mpu6050.h"

#include <stddef.h>

#include "iic.h"

enum
{
    REG_SAMPLE_RATE = 0x19,
    REG_CONFIG = 0x1A,
    REG_ACCEL_CONFIG = 0x1C,
    REG_ACCEL_X_HIGH = 0x3B,
    REG_POWER = 0x6B,
    REG_WHO_AM_I = 0x75,
};

static uint8_t address;

/* 发一个字节并检查应答。底层在应答超时时自行发 STOP。 */
static bool SendByte(uint8_t byte)
{
    IIC_Send_Byte(byte);
    return IIC_Wait_Ack() == 0U;
}

/* 先写寄存器地址，再用重复 START 连续读取数据。 */
static bool ReadRegisters(uint8_t reg, uint8_t *bytes, size_t length)
{
    IIC_Start();
    if (!SendByte(address << 1U) || !SendByte(reg))
    {
        return false;
    }

    IIC_Start();
    if (!SendByte((address << 1U) | 1U))
    {
        return false;
    }
    for (size_t i = 0; i < length; ++i)
    {
        bytes[i] = IIC_Read_Byte(i + 1U < length);
    }
    IIC_Stop();
    return true;
}

/* 写一个 MPU6050 配置寄存器。 */
static bool WriteRegister(uint8_t reg, uint8_t value)
{
    IIC_Start();
    if (!SendByte(address << 1U) || !SendByte(reg) || !SendByte(value))
    {
        return false;
    }
    IIC_Stop();
    return true;
}

/* 探测地址，唤醒芯片，并设置低通滤波和 ±2 g 量程。 */
bool Mpu6050_Init(void)
{
    uint8_t identity;
    for (address = 0x68U; address <= 0x69U; ++address)
    {
        if (ReadRegisters(REG_WHO_AM_I, &identity, 1U) && identity == 0x68U)
        {
            return WriteRegister(REG_POWER, 0x01U) &&
                   WriteRegister(REG_CONFIG, 0x04U) &&
                   WriteRegister(REG_SAMPLE_RATE, 9U) &&
                   WriteRegister(REG_ACCEL_CONFIG, 0x00U);
        }
    }
    return false;
}

/* 合并两个高字节在前的补码字节。 */
static int16_t DecodeAxis(const uint8_t bytes[2])
{
    const int32_t value = ((uint16_t)bytes[0] << 8U) | bytes[1];
    return (int16_t)(value >= 32768 ? value - 65536 : value);
}

/* 连续读取三轴加速度，避免每轴分开读到不同批次的值。 */
bool Mpu6050_ReadAcceleration(Mpu6050Acceleration *acceleration)
{
    uint8_t bytes[6];
    if (!ReadRegisters(REG_ACCEL_X_HIGH, bytes, sizeof bytes))
    {
        return false;
    }

    acceleration->x = DecodeAxis(&bytes[0]);
    acceleration->y = DecodeAxis(&bytes[2]);
    acceleration->z = DecodeAxis(&bytes[4]);
    return true;
}
