//C code to read the I2C data from the MPU6050 accelerometer/gyroscope

//NOTE: work in progress

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/i2c-dev.h>

int f_dev; //Device file
int MPU6050_addr = 0x68;

float gyro[3]; //Array to store the gyroscope values
int accel[3]; //Array to store the gyroscope values
float temp_C; //Temperature variable

int init() { //Init function
	int status;

	f_dev = open("/dev/i2c-1", O_RDWR); //Open the I2C device file
	if (f_dev < 0) { //Catch errors
		std::cout << f_dev;
		std::cout << "Failed to open /dev/i2c-1. Please check that I2C is enabled with raspi-config\n";
		return 1;
	}

	status = ioctl(f_dev, I2C_SLAVE, MPU6050_addr); //Set the I2C bus to use the correct address
	if (status < 0) {
		std::cout << "Could not get I2C bus\n";
		return 1;
	}

	i2c_smbus_write_byte_data(f_dev, 0x6b, 0b00000000); //Take MPU6050 out of sleep mode - see Register Map

	i2c_smbus_write_byte_data(f_dev, 0x1b, 0b00000000); //Configure gyroscope settings - see Register Map

	i2c_smbus_write_byte_data(f_dev, 0x1b, 0b00000000); //Configure accelerometer settings - see Register Map

	return 0;
}

void getTemp() { //Read and return temperature
	int16_t temp = i2c_smbus_read_byte_data(f_dev, 0x41) << 8 | i2c_smbus_read_byte_data(f_dev, 0x42); //Read temperature registers
	temp_C = (float)temp / 340.0 + 36.53; //Calculate given formula in datasheet (in degrees C)
}

void getGyro() { //Read and return the gyroscope values
	int16_t X = i2c_smbus_read_byte_data(f_dev, 0x43) << 8 | i2c_smbus_read_byte_data(f_dev, 0x44); //Read X registers
	int16_t Y = i2c_smbus_read_byte_data(f_dev, 0x45) << 8 | i2c_smbus_read_byte_data(f_dev, 0x46); //Read Y registers
	int16_t Z = i2c_smbus_read_byte_data(f_dev, 0x47) << 8 | i2c_smbus_read_byte_data(f_dev, 0x48); //Read Z registers
	//Get angular velocity
	X = (float)X / 131.0;
	Y = (float)Y / 131.0;
	Z = (float)Z / 131.0;
	gyro[0]=X, gyro[1]=Y, gyro[2]=Z;
}

void getAccel() { //Read and return the accelerometer values
	int16_t X = i2c_smbus_read_byte_data(f_dev, 0x43) << 8 | i2c_smbus_read_byte_data(f_dev, 0x44); //Read X registers
	int16_t Y = i2c_smbus_read_byte_data(f_dev, 0x45) << 8 | i2c_smbus_read_byte_data(f_dev, 0x46); //Read Y registers
	int16_t Z = i2c_smbus_read_byte_data(f_dev, 0x47) << 8 | i2c_smbus_read_byte_data(f_dev, 0x48); //Read Z registers
	accel[0]=(int)X, accel[1]=(int)Y, accel[2]=(int)Z;
}

int main() {
	init();
	std::cout << "Setup Complete\n";


	while (1) {
	getTemp();

	getGyro();

	getAccel();

	std::cout << "The current temperature is: " << temp_C << "\n";
	std::cout << "The current accelerometer values are: " << accel[0] << "," << accel[1] << "," << accel[2] << "\n";
	std::cout << "The current gyroscope values are: " << gyro[0] << "," << gyro[1] << "," << gyro[2] << "\n";
	sleep(0.4);
	}
	return 0;
}
