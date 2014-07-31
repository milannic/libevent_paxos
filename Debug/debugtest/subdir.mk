################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../debugtest/libconfig_test.c 

OBJS += \
./debugtest/libconfig_test.o 

C_DEPS += \
./debugtest/libconfig_test.d 


# Each subdirectory must supply rules for building sources it contributes
debugtest/%.o: ../debugtest/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -std=c99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


