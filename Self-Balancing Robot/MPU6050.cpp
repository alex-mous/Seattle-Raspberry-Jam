//Copyright (c) 2019, Alex Mous
//Licensed under the CC BY-NC SA 4.0

//C code to read the I2C data from the MPU6050 accelerometer/gyroscope

#define _POSIX_C_SOURCE 200809L

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <time.h>
#include <linux/i2c-dev.h>
#include <cmath>

#define TAU 0.05 //Complementary filter percentage
#define RAD_T_DEG 57.29577951308 //Radians to degrees (180/PI)
#define GYRO_SENS 131.0 //Gyroscope sensetivity factor
#define ACCEL_SENS 16384.0 //Accelerometer sensetivity factor


//Offsets - supply your own here (calculate offsets with readAccelOffsets function)
//     Accelerometer
#define A_OFF_X 19402
#define A_OFF_Y -2692
#define A_OFF_Z -8625
//    Gyroscope
#define G_OFF_X -733
#define G_OFF_Y 433
#define G_OFF_Z -75

int f_dev; //Device file
int MPU6050_addr = 0x68;

float dt = 0.009; //Loop time (recalculated with each loop)

struct timespec start,end; //Create a time structure

bool _first_run = 1; //Variable for whether to set gyro angle to acceleration angle in compFilter

float accel_angle; //Accelerometer angle
float gyro_angle; //Gyroscope angle
float angle;


int initMPU6050() { //General init function; opens dev file and does first configuration of MPU6050
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

	i2c_smbus_write_byte_data(f_dev, 0x1a, 0b00000011); //Set DLPF (low pass filter) to 44Hz (so no noise above 44Hz will pass through)

	i2c_smbus_write_byte_data(f_dev, 0x19, 0b00000100); //Set sample rate divider (to 200Hz) - see Register Map

	i2c_smbus_write_byte_data(f_dev, 0x1b, 0b00000000); //Configure gyroscope settings - see Register Map

	i2c_smbus_write_byte_data(f_dev, 0x1c, 0b00000000); //Configure accelerometer settings - see Register Map

	//Set offsets to zero
	i2c_smbus_write_byte_data(f_dev, 0x06, 0b00000000);
	i2c_smbus_write_byte_data(f_dev, 0x07, 0b00000000);
	i2c_smbus_write_byte_data(f_dev, 0x08, 0b00000000);
	i2c_smbus_write_byte_data(f_dev, 0x09, 0b00000000);
	i2c_smbus_write_byte_data(f_dev, 0x0A, 0b00000000);
	i2c_smbus_write_byte_data(f_dev, 0x0B, 0b00000000);
	i2c_smbus_write_byte_data(f_dev, 0x00, 0b10000001); //Set more offsets to zero - Normal is 11111101
	i2c_smbus_write_byte_data(f_dev, 0x01, 0b00000001); //Set more offsets to zero - Normal is 00000101
	i2c_smbus_write_byte_data(f_dev, 0x02, 0b10000001); //Set more offsets to zero - Normal is 10000001

	return 0;
}

void getTemp(float *temp) { //Read and return temperature
	int16_t rawtemp = i2c_smbus_read_byte_data(f_dev, 0x41) << 8 | i2c_smbus_read_byte_data(f_dev, 0x42); //Read temperature registers
	*temp = (float)rawtemp / 340.0 + 36.53; //Calculate given formula in datasheet (in degrees C)
}

void getGyroRaw(float *r, float *p, float *y) { //Read the raw gyroscope values (r is roll, p is pitch, y is yaw)
	int16_t X = i2c_smbus_read_byte_data(f_dev, 0x43) << 8 | i2c_smbus_read_byte_data(f_dev, 0x44); //Read X registers
	int16_t Y = i2c_smbus_read_byte_data(f_dev, 0x45) << 8 | i2c_smbus_read_byte_data(f_dev, 0x46); //Read Y registers
	int16_t Z = i2c_smbus_read_byte_data(f_dev, 0x47) << 8 | i2c_smbus_read_byte_data(f_dev, 0x48); //Read Z registers
	*r = (float)X; //Roll on X axis
	*p = (float)Y; //Pitch on Y axis
	*y = (float)Z; //Yaw on Z axis
}

void getGyro(float *roll, float *pitch, float *yaw) { //Cleanup gyro data
	getGyroRaw(roll, pitch, yaw); //Store raw values into variables
	*roll = round((*roll - G_OFF_X) * 1000.0 / GYRO_SENS) / 1000.0;
	*pitch = round((*pitch - G_OFF_Y) * 1000.0 / GYRO_SENS) / 1000.0;
	*yaw = round((*yaw - G_OFF_Z) * 1000.0 / GYRO_SENS) / 1000.0;
}

void getAccelRaw(float *x, float *y, float *z) { //Read the raw accelerometer values
	int16_t X = i2c_smbus_read_byte_data(f_dev, 0x3b) << 8 | i2c_smbus_read_byte_data(f_dev, 0x3c); //Read X registers
	int16_t Y = i2c_smbus_read_byte_data(f_dev, 0x3d) << 8 | i2c_smbus_read_byte_data(f_dev, 0x3e); //Read Y registers
	int16_t Z = i2c_smbus_read_byte_data(f_dev, 0x3f) << 8 | i2c_smbus_read_byte_data(f_dev, 0x40); //Read Z registers
	*x = (float)X;
	*y = (float)Y;
	*z = (float)Z;
}

void getAccel(float *x, float *y, float *z) { //Cleanup gyro data
	getAccelRaw(x, y, z); //Store raw values into variables
	*x = round((*x - A_OFF_X) * 1000.0 / ACCEL_SENS) / 1000.0;
	*y = round((*y - A_OFF_Y) * 1000.0 / ACCEL_SENS) / 1000.0;
	*z = round((*z - A_OFF_Z) * 1000.0 / ACCEL_SENS) / 1000.0;
}

void getOffsets() { //Get accelerometer offsets - place the module on a completely flat surface (check with a spirit level if possible) and make sure that is stays still while running this function
	float gyro_off[3]; //Temporary storage
	float accel_off[3];

	float gyro_off_s[3]; //Sum variables
	float accel_off_s[3];

	gyro_off_s[0] = 0, gyro_off_s[1] = 0, gyro_off_s[2] = 0; //Initialize the offsets to zero
	accel_off_s[0] = 0, accel_off_s[1] = 0, accel_off_s[2] = 0;

	for (int i = 0; i < 10000; i++) { //Use loop to average offsets
		getGyroRaw(&gyro_off[0], &gyro_off[1], &gyro_off[2]); //Raw gyroscope values
		gyro_off_s[0] = gyro_off_s[0] + gyro_off[0], gyro_off_s[1] = gyro_off_s[1] + gyro_off[1], gyro_off_s[2] = gyro_off_s[2] + gyro_off[2]; //Add to sum

		getAccelRaw(&accel_off[0], &accel_off[1], &accel_off[2]); //Raw accelerometer values
		accel_off_s[0] = accel_off_s[0] + accel_off[0], accel_off_s[1] = accel_off_s[1] + accel_off[1], accel_off_s[2] = accel_off_s[2] + accel_off[2]; //Add to sum
	}

	gyro_off_s[0] = gyro_off_s[0] / 10000, gyro_off_s[1] = gyro_off_s[1] / 10000, gyro_off_s[2] = gyro_off_s[2] / 10000; //Divide by number of loops (average)
	accel_off_s[0] = accel_off_s[0] / 10000, accel_off_s[1] = accel_off_s[1] / 10000, accel_off_s[2] = accel_off_s[2] / 10000;

	accel_off_s[2] = accel_off_s[2] - ACCEL_SENS; //Remove 1g from the value calculated to compensate for gravity)

	std::cout << "Accelerometer Values: X " << accel_off_s[0] << ", Y " << accel_off_s[1] << ", Z" << accel_off_s[2] << "\nPut these in for A_OFF_X, A_OFF_Y and A_OFF_Z at the beginning of this code.\n\n\n";
	std::cout << "Gyroscope Values: X " << gyro_off_s[0] << ", Y " << gyro_off_s[1] << ", Z" << gyro_off_s[2] << "\nPut these in for G_OFF_X, G_OFF_Y and G_OFF_Z at the beginning of this code.\n\n\n";
}

void compFilter(int axis, float *result) { //Set axis to 0,1 or 2 for R, P or Y measurents
	float ax, ay, az, gr, gp, gy; //Raw values

	getGyro(&gr, &gp, &gy); //Get the data from the sensors
	getAccel(&ax, &ay, &az);

	accel_angle = atan2(az, ax) * RAD_T_DEG - 90.0; //Calculate the angle with z and x convert to degrees and subtract 90 degrees to rotate
	gyro_angle = angle + gp*dt; //Use pitch axis (Y axis)
/*	switch (axis) { //Switch which axis is used
		case 0:
			accel_angle = atan2(az, ay) * RAD_T_DEG - 90.0; //Calculate the angle with z and y convert to degrees and subtract 90 degrees to rotate
			gyro_angle = gyro_angle + gr*dt; //Use roll axis (X axis)
		case 1:
			accel_angle = atan2(az, ax) * RAD_T_DEG - 90.0; //Calculate the angle with z and x convert to degrees and subtract 90 degrees to rotate
			gyro_angle = gyro_angle + gp*dt; //Use pitch axis (Y axis)
		case 2:
			accel_angle = atan2(ay, ax) * RAD_T_DEG - 90.0; //Calculate the angle with y and x convert to degrees and subtract 90 degrees to rotate
			gyro_angle = gyro_angle + gy*dt; //Use yaw axis (Z axis)
	}*/

	if (_first_run) { //Set the gyroscope angle reference point if this is the first function run
		gyro_angle = accel_angle; //Start off with angle from accelerometer (absolute angle since gyroscope is relative)
		_first_run = 0;
	}

	if (abs(gyro_angle - accel_angle) > 5) { //Correct for very large drift (or incorrect measurment of gyroscope by longer loop time)
		gyro_angle = accel_angle;
	}

	float asum = abs(ax) + abs(ay) + abs(az); //Calculate the sum of the accelerations
	float gsum = abs(gr) + abs(gp) + abs(gy); //Calculate the sum of the gyro readings

	//Create result from either complementary filter or directly from gyroscope or accelerometer depending on conditions
	if (asum > 0.1 && asum < 3 && gsum > 0.3) { //Check that th movement is not very high (therefore providing inacurate angles)
		*result = (1 - TAU)*(gyro_angle) + (TAU)*(accel_angle); //Calculate the angle using a complementary filter
	}
	else if (gsum > 0.3) { //Use the gyroscope angle if the acceleration is high
		*result = gyro_angle;
	}
	else if (gsum <= 0.3) { //Use accelerometer angle if not much movement
		*result = accel_angle;
	}

	std::cout << "A" << accel_angle << "\n";
	std::cout << "G" << gyro_angle << "\n";
	std::cout << "R" << *result << "\n";
}

int mainLoop() {
	float res;
	clock_gettime(CLOCK_REALTIME, &start); //Read current time into start variable

	while (1) {
		compFilter(1, &res); //Run filter function
		clock_gettime(CLOCK_REALTIME, &end); //Save time to end clock
		dt = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9; //Calculate new dt
		clock_gettime(CLOCK_REALTIME, &start); //Save time to start clock
	}
}

int main() {
	initMPU6050();

//	getOffsets();

	mainLoop();

	return 0;
}
