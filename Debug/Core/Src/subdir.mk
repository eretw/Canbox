################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/adc_keys.c \
../Core/Src/app_time.c \
../Core/Src/can.c \
../Core/Src/dbc_codec.c \
../Core/Src/gpio.c \
../Core/Src/key_service.c \
../Core/Src/lcd12864.c \
../Core/Src/main.c \
../Core/Src/stm32f1xx_hal_msp.c \
../Core/Src/stm32f1xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c \
../Core/Src/system_stm32f1xx.c \
../Core/Src/tim.c \
../Core/Src/usart.c 

OBJS += \
./Core/Src/adc_keys.o \
./Core/Src/app_time.o \
./Core/Src/can.o \
./Core/Src/dbc_codec.o \
./Core/Src/gpio.o \
./Core/Src/key_service.o \
./Core/Src/lcd12864.o \
./Core/Src/main.o \
./Core/Src/stm32f1xx_hal_msp.o \
./Core/Src/stm32f1xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o \
./Core/Src/system_stm32f1xx.o \
./Core/Src/tim.o \
./Core/Src/usart.o 

C_DEPS += \
./Core/Src/adc_keys.d \
./Core/Src/app_time.d \
./Core/Src/can.d \
./Core/Src/dbc_codec.d \
./Core/Src/gpio.d \
./Core/Src/key_service.d \
./Core/Src/lcd12864.d \
./Core/Src/main.d \
./Core/Src/stm32f1xx_hal_msp.d \
./Core/Src/stm32f1xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d \
./Core/Src/system_stm32f1xx.d \
./Core/Src/tim.d \
./Core/Src/usart.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o Core/Src/%.su Core/Src/%.cyclo: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g1 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F105xC -c -I../Core/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -Og -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/adc_keys.cyclo ./Core/Src/adc_keys.d ./Core/Src/adc_keys.o ./Core/Src/adc_keys.su ./Core/Src/app_time.cyclo ./Core/Src/app_time.d ./Core/Src/app_time.o ./Core/Src/app_time.su ./Core/Src/can.cyclo ./Core/Src/can.d ./Core/Src/can.o ./Core/Src/can.su ./Core/Src/dbc_codec.cyclo ./Core/Src/dbc_codec.d ./Core/Src/dbc_codec.o ./Core/Src/dbc_codec.su ./Core/Src/gpio.cyclo ./Core/Src/gpio.d ./Core/Src/gpio.o ./Core/Src/gpio.su ./Core/Src/key_service.cyclo ./Core/Src/key_service.d ./Core/Src/key_service.o ./Core/Src/key_service.su ./Core/Src/lcd12864.cyclo ./Core/Src/lcd12864.d ./Core/Src/lcd12864.o ./Core/Src/lcd12864.su ./Core/Src/main.cyclo ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/main.su ./Core/Src/stm32f1xx_hal_msp.cyclo ./Core/Src/stm32f1xx_hal_msp.d ./Core/Src/stm32f1xx_hal_msp.o ./Core/Src/stm32f1xx_hal_msp.su ./Core/Src/stm32f1xx_it.cyclo ./Core/Src/stm32f1xx_it.d ./Core/Src/stm32f1xx_it.o ./Core/Src/stm32f1xx_it.su ./Core/Src/syscalls.cyclo ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/syscalls.su ./Core/Src/sysmem.cyclo ./Core/Src/sysmem.d ./Core/Src/sysmem.o ./Core/Src/sysmem.su ./Core/Src/system_stm32f1xx.cyclo ./Core/Src/system_stm32f1xx.d ./Core/Src/system_stm32f1xx.o ./Core/Src/system_stm32f1xx.su ./Core/Src/tim.cyclo ./Core/Src/tim.d ./Core/Src/tim.o ./Core/Src/tim.su ./Core/Src/usart.cyclo ./Core/Src/usart.d ./Core/Src/usart.o ./Core/Src/usart.su

.PHONY: clean-Core-2f-Src

