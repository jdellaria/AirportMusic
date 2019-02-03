################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/AirPortMusic.cpp \
../src/AudioStream.cpp \
../src/MP3Stream.cpp \
../src/MusicDB.cpp \
../src/RAOPClient.cpp \
../src/RTSPClient.cpp \
../src/TCPSocket.cpp \
../src/configurationFile.cpp 

OBJS += \
./src/AirPortMusic.o \
./src/AudioStream.o \
./src/MP3Stream.o \
./src/MusicDB.o \
./src/RAOPClient.o \
./src/RTSPClient.o \
./src/TCPSocket.o \
./src/configurationFile.o 

CPP_DEPS += \
./src/AirPortMusic.d \
./src/AudioStream.d \
./src/MP3Stream.d \
./src/MusicDB.d \
./src/RAOPClient.d \
./src/RTSPClient.d \
./src/TCPSocket.d \
./src/configurationFile.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -I/usr/local/ssl/include -I"/home/jdellaria/workspace/DLiriumLib" -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


