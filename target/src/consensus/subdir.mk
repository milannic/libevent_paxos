################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/consensus/consensus.c \
../src/consensus/learner.c 

OBJS += \
./src/consensus/consensus.o \
./src/consensus/learner.o 

C_DEPS += \
./src/consensus/consensus.d \
./src/consensus/learner.d 


# Each subdirectory must supply rules for building sources it contributes
src/consensus/%.o: ../src/consensus/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I"$(ROOT_DIR)/../.local/include" -O0 -g3 -Wall -c -fmessage-length=0 -std=c99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


