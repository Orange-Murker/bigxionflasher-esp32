/* BigXionFlasher.c */
/* ====================================================================
 * Copyright (c) 2011-2017 by Thomas König <info@bigxionflasher.org>.
 * All rights reserved. Distributed under the GPLv3 software license,
 * see the accompanying file LICENSE or
 * https://opensource.org/licenses/GPL-3.0
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the
 *    BigXionFlasher Project. (http://www.bigxionflasher.org/)"
 *
 * 4. The name "BigXionFlasher" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    info@bigxionflasher.org.
 *
 * 5. Products derived from this software may not be called "BigXionFlasher"
 *    nor may "BigXionFlasher" appear in their names without prior written
 *    permission of the BigXionFlasher Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the
 *    BigXionFlasher Project. (http://www.bigxionflasher.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE BigXionFlasher PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE BigXionFlasher PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 */

/*
Copyright (c) 2023 by Orange_Murker. Ported the original project by Thomas König to ESP32.
*/

#include <Arduino.h>
#include <BluetoothSerial.h>
BluetoothSerial BTSerial;

#include "driver/gpio.h"
#include "driver/twai.h"

#define _NL "\n"
#define _DEGREE_SIGN "°"

#define __DOSTR(v) #v
#define __STR(v) __DOSTR(v)

#define __BXF_VERSION__ "V 0.2.4 rev. 97"

#define UNLIMITED_SPEED_VALUE 70	 /* Km/h */
#define UNLIMITED_MIN_SPEED_VALUE 30 /* Km/h */
#define MAX_THROTTLE_SPEED_VALUE 70	 /* Km/h */

#include "registers.h"

#define TIMEOUT_VALUE 80
#define TIMEOUT_MS 10 // 10ms

String node_name;

const char *getNodeName(uint32_t id)
{
	switch (id)
	{
	case CONSOLE:
		node_name = String("console (slave)");
		break;
	case BATTERY:
		node_name = String("battery");
		break;
	case MOTOR:
		node_name = String("motor");
		break;
	case BIB:
		node_name = String("bib");
		break;
	case CONSOLE_STANDARD_MODE:
		node_name = String("console");
		break;
	default:
		char unknown[20];
		sprintf(unknown, "unknown id: 0x%02X", id);
		node_name = String(unknown);
		break;
	}

	return node_name.c_str();
}

void setValue(uint8_t receipient, uint8_t reg, uint8_t value)
{
	twai_message_t message;

	message.identifier = receipient;
	message.data_length_code = 4;
	message.data[0] = 0x00;
	message.data[1] = reg;
	message.data[2] = 0x00;
	message.data[3] = value;

	if (twai_transmit(&message, pdMS_TO_TICKS(1000)) != ESP_OK)
	{
		BTSerial.printf("Failed to queue message for transmission\n");
		BTSerial.printf("ERROR: Failed to send the packet to %s" _NL, getNodeName(receipient));
	}
}

uint8_t getValue(uint8_t receipient, uint8_t reg)
{
	twai_message_t message;

	message.identifier = receipient;
	message.data_length_code = 2;
	message.data[0] = 0x00;
	message.data[1] = reg;

	if (twai_transmit(&message, pdMS_TO_TICKS(1000)) != ESP_OK)
	{
		BTSerial.printf("Failed to queue message for transmission\n");
		BTSerial.printf("ERROR: Failed to send the packet to %s" _NL, getNodeName(receipient));
	}

	int retry = TIMEOUT_VALUE;

	// Postfix is used because the loop will end when retry is 0
	while (retry--)
	{
		if (twai_receive(&message, pdMS_TO_TICKS(TIMEOUT_MS)) != ESP_OK)
		{
			BTSerial.printf(".");
		}

		if (message.identifier == BIB && message.data_length_code == 4 && message.data[1] == reg)
		{
			break;
		}
	}

	if (retry == -1)
	{
		BTSerial.printf("ERROR: no response from node %s to %s" _NL, getNodeName(receipient), getNodeName(BIB));
		return 0;
	}

	return message.data[3];
}

void setSpeedLimit(double speed)
{
	int limit = (speed != 0);

	if (!speed)
		speed = UNLIMITED_SPEED_VALUE;
	setValue(CONSOLE, CONSOLE_ASSIST_MAXSPEEDFLAG, limit);
	setValue(CONSOLE, CONSOLE_ASSIST_MAXSPEED_HI, ((int)(speed * 10)) >> 8);
	setValue(CONSOLE, CONSOLE_ASSIST_MAXSPEED_LO, ((int)(speed * 10)) & 0xff);
	setValue(MOTOR, MOTOR_PROTECT_UNLOCK, MOTOR_PROTECT_UNLOCK_KEY);
	setValue(MOTOR, MOTOR_ASSIST_MAXSPEED, (int)speed);
}

void setWheelCircumference(unsigned short circumference)
{
	if (!circumference)
		return;

	setValue(CONSOLE, CONSOLE_GEOMETRY_CIRC_HI, (int)(circumference >> 8));
	setValue(CONSOLE, CONSOLE_GEOMETRY_CIRC_LO, (int)(circumference & 0xff));
	setValue(MOTOR, MOTOR_PROTECT_UNLOCK, MOTOR_PROTECT_UNLOCK_KEY);
	setValue(MOTOR, MOTOR_GEOMETRY_CIRC_HI, (int)(circumference >> 8));
	setValue(MOTOR, MOTOR_GEOMETRY_CIRC_LO, (int)(circumference & 0xff));
}

void setMinSpeedLimit(double speed)
{
	char limit = (speed != 0);

	setValue(CONSOLE, CONSOLE_ASSIST_MINSPEEDFLAG, limit);
	setValue(CONSOLE, CONSOLE_ASSIST_MINSPEED, (int)(speed * 10));
}

void setThrottleSpeedLimit(double speed)
{
	int limit = (speed != 0);

	if (!speed)
		speed = MAX_THROTTLE_SPEED_VALUE;

	setValue(CONSOLE, CONSOLE_THROTTLE_MAXSPEEDFLAG, limit);
	setValue(CONSOLE, CONSOLE_THROTTLE_MAXSPEED_HI, ((int)(speed * 10)) >> 8);
	setValue(CONSOLE, CONSOLE_THROTTLE_MAXSPEED_LO, ((int)(speed * 10)) & 0xff);
}

void printBatteryStats()
{
	int channel = 1, packSerial, packParallel;

	BTSerial.printf(" balancer enabled ...: %s" _NL _NL, (getValue(BATTERY, BATTERY_CELLMON_BALANCERENABLED != 0) ? "yes" : "no"));

	packSerial = getValue(BATTERY, BATTERY_CONFIG_PACKSERIAL);
	packParallel = getValue(BATTERY, BATTERY_CONFIG_PACKPARALLEL);

	packSerial = (packSerial > 20) ? 0 : packSerial;
	packParallel = (packParallel > 20) ? 0 : packParallel;

	for (; channel <= packSerial; channel++)
	{
		setValue(BATTERY, BATTERY_CELLMON_CHANNELADDR, (int)0x80 + channel);
		BTSerial.printf(" voltage cell #%02d ...: %.3fV" _NL, channel,
						((getValue(BATTERY, BATTERY_CELLMON_CHANNELDATA_HI) << 8) + getValue(BATTERY, BATTERY_CELLMON_CHANNELDATA_LO)) * 0.001);
	}

	for (channel = 0; channel < packParallel; channel++)
		BTSerial.printf(" temperature pack #%02d: %d" _DEGREE_SIGN "C" _NL, channel + 1,
						getValue(BATTERY, BATTERY_STATUS_PACKTEMPERATURE1 + channel));

	BTSerial.println();
}

void printChargeStats()
{
	int channel = 1, totalChagres = 0, c;

	for (channel = 1; channel <= 10; channel++)
	{
		setValue(BATTERY, 0xf6, channel);
		c = (getValue(BATTERY, 0xf7) << 8) + getValue(BATTERY, 0xf8);
		totalChagres += c;
		BTSerial.printf(" charge level @ %03d%% : %04d" _NL, channel * 10, c);
	}

	BTSerial.printf(" total # of charges .: %04d" _NL _NL, totalChagres);
}

double getVoltageValue(unsigned char in, unsigned char reg)
{
	return (getValue(BATTERY, reg) + 20.8333) * 0.416667;
}

void usage(void)
{
	BTSerial.printf("Usage:" _NL
					"l <speedLimit> .......... set the speed limit to <speedLimit> (1 - " __STR(UNLIMITED_SPEED_VALUE) "), 0 = remove the limit" _NL
																													   "m <minSpeedLimit> ....... set the minimum speed limit to <minSpeedLimit> (0 - " __STR(UNLIMITED_MIN_SPEED_VALUE) "), 0 = remove the limit" _NL
																																																										 "t <throttleSpeedLimit> .. set the throttle speed limit to <throttleSpeedLimit> (0 - " __STR(MAX_THROTTLE_SPEED_VALUE) "), 0 = remove the limit" _NL
																																																																																								"a <assistLevel> ......... set the initial assist level after power on (0 - 4)" _NL
																																																																																								"o <level> ............... set the mountain cap level (0%% - 100%%), use 55%%" _NL
																																																																																								"c <wheel circumference> . set the wheel circumference (in mm)" _NL
																																																																																								"s ....................... print system settings overview" _NL
																																																																																								"p ....................... power off system" _NL
																																																																																								"n ....................... put the console in slave mode" _NL
																																																																																								"i ....................... capture and display CAN packets. Send anything to stop." _NL
																																																																																								"h ....................... print this help screen" _NL _NL);
}

void printSystemSettings()
{
	int hwVersion, swVersion, wheelCirc;
	const char *sl;
	double speedLimit = 0;

	BTSerial.println();
	BTSerial.println();

	hwVersion = getValue(CONSOLE, CONSOLE_REF_HW);

	if (hwVersion == 0)
		BTSerial.printf("Console not responding" _NL _NL);
	else
	{
		swVersion = getValue(CONSOLE, CONSOLE_REF_SW);
		BTSerial.printf("Console information:" _NL
						" hardware version ........: %02d" _NL
						" software version ........: %02d" _NL
						" assistance level ........: %d" _NL,
						hwVersion, swVersion,
						getValue(CONSOLE, CONSOLE_ASSIST_INITLEVEL));

		BTSerial.printf(" part number .............: %05d" _NL
						" item number .............: %05d" _NL _NL,
						((getValue(CONSOLE, CONSOLE_SN_PN_HI) << 8) + getValue(CONSOLE, CONSOLE_SN_PN_LO)),
						((getValue(CONSOLE, CONSOLE_SN_ITEM_HI) << 8) + getValue(CONSOLE, CONSOLE_SN_ITEM_LO)));

		/* ASSIST speed limit */
		sl = getValue(CONSOLE, CONSOLE_ASSIST_MAXSPEEDFLAG) == 0 ? "no" : "yes";
		speedLimit = ((getValue(CONSOLE, CONSOLE_ASSIST_MAXSPEED_HI) << 8) + getValue(CONSOLE, CONSOLE_ASSIST_MAXSPEED_LO)) / (double)10;
		BTSerial.printf(" max limit enabled .......: %s" _NL
						" speed limit .............: %0.2f Km/h" _NL _NL,
						sl, speedLimit);

		/* MIN speed limit */
		sl = getValue(CONSOLE, CONSOLE_ASSIST_MINSPEEDFLAG) == 0 ? "no" : "yes";
		speedLimit = (getValue(CONSOLE, CONSOLE_ASSIST_MINSPEED)) / (double)10;
		BTSerial.printf(" min limit enabled .......: %s" _NL
						" min speed limit .........: %0.2f Km/h" _NL _NL,
						sl, speedLimit);

		/* THROTTLE speed limit */
		sl = getValue(CONSOLE, CONSOLE_THROTTLE_MAXSPEEDFLAG) == 0 ? "no" : "yes";
		speedLimit = ((getValue(CONSOLE, CONSOLE_THROTTLE_MAXSPEED_HI) << 8) + getValue(CONSOLE, CONSOLE_THROTTLE_MAXSPEED_LO)) / (double)10;
		BTSerial.printf(" throttle limit enabled ..: %s" _NL
						" throttle speed limit ....: %0.2f Km/h" _NL _NL,
						sl, speedLimit);

		/* WHEEL CIRCUMFERENCE */
		wheelCirc = (getValue(CONSOLE, CONSOLE_GEOMETRY_CIRC_HI) << 8) + getValue(CONSOLE, CONSOLE_GEOMETRY_CIRC_LO);
		BTSerial.printf(" wheel circumference .....: %d mm" _NL _NL, wheelCirc);

		if (swVersion >= 59)
			BTSerial.printf(
				" mountain cap ............: %0.2f%%" _NL,
				(getValue(CONSOLE, CONSOLE_ASSIST_MOUNTAINCAP) * 1.5625));

		BTSerial.printf(" odo .....................: %0.2f Km" _NL _NL,
						((getValue(CONSOLE, CONSOLE_STATS_ODO_1) << 24) +
						 (getValue(CONSOLE, CONSOLE_STATS_ODO_2) << 16) +
						 (getValue(CONSOLE, CONSOLE_STATS_ODO_3) << 8) +
						 (getValue(CONSOLE, CONSOLE_STATS_ODO_4))) /
							(double)10);
	}

	hwVersion = getValue(BATTERY, BATTERY_REF_HW);
	if (hwVersion == 0)
		BTSerial.printf("Battery not responding" _NL _NL);
	else
	{
		BTSerial.printf("Battery information:" _NL
						" hardware version ........: %02d" _NL
						" software version ........: %02d" _NL,
						hwVersion, getValue(BATTERY, BATTERY_REF_SW));

		BTSerial.printf(" part number .............: %05d" _NL
						" item number .............: %05d" _NL,
						((getValue(BATTERY, BATTERY_SN_PN_HI) << 8) + getValue(BATTERY, BATTERY_SN_PN_LO)),
						((getValue(BATTERY, BATTERY_SN_ITEM_HI) << 8) + getValue(BATTERY, BATTERY_SN_ITEM_LO)));

		BTSerial.printf(" voltage .................: %0.2fV" _NL
						" battery level ...........: %0.2f%%" _NL
						" maximum voltage .........: %0.2f%%" _NL
						" minimum voltage .........: %0.2f%%" _NL
						" mean voltage ............: %0.2f%%" _NL
						" resets ..................: %0d" _NL
						" ggjrCalib ...............: %0d" _NL
						" vctrlShorts .............: %0d" _NL
						" lmd .....................: %0.2fAh" _NL
						" cell capacity ...........: %0.2fAh" _NL _NL,
						((getValue(BATTERY, BATTERY_STATUS_VBATT_HI) << 8) + getValue(BATTERY, BATTERY_STATUS_VBATT_LO)) * 0.001,
						(getValue(BATTERY, BATTERY_STATUS_LEVEL) * 6.6667),
						getVoltageValue(BATTERY, BATTERY_STATS_VBATTMAX),
						getVoltageValue(BATTERY, BATTERY_STATS_VBATTMIN),
						getVoltageValue(BATTERY, BATTERY_STATS_VBATTMEAN),
						(getValue(BATTERY, BATTERY_STATS_RESET_HI) << 8) + getValue(BATTERY, BATTERY_STATS_RESET_LO),
						getValue(BATTERY, BATTERY_STSTS_GGJSRCALIB),
						getValue(BATTERY, BATTERY_STSTS_VCTRLSHORTS),
						((getValue(BATTERY, BATTERY_STATS_LMD_HI) << 8) + getValue(BATTERY, BATTERY_STATS_LMD_LO)) * 0.002142,
						((getValue(BATTERY, BATTERY_CONFIG_CELLCAPACITY_HI) << 8) + getValue(BATTERY, BATTERY_CONFIG_CELLCAPACITY_LO)) * 0.001);

		BTSerial.printf(" charge time worst .......: %0d" _NL
						" charge time mean ........: %0d" _NL
						" charge cycles ...........: %0d" _NL
						" full charge cycles ......: %0d" _NL
						" power cycles ............: %0d" _NL
						" battery temp max ........: %0d" _NL
						" battery temp min ........: %0d" _NL _NL,
						(getValue(BATTERY, BATTERY_STATS_CHARGETIMEWORST_HI) << 8) + getValue(BATTERY, BATTERY_STATS_CHARGETIMEWORST_LO),
						(getValue(BATTERY, BATTERY_STATS_CHARGETIMEMEAN_HI) << 8) + getValue(BATTERY, BATTERY_STATS_CHARGETIMEMEAN_LO),
						(getValue(BATTERY, BATTERY_STATS_BATTCYCLES_HI) << 8) + getValue(BATTERY, BATTERY_STATS_BATTCYCLES_LO),
						(getValue(BATTERY, BATTERY_STATS_BATTFULLCYCLES_HI) << 8) + getValue(BATTERY, BATTERY_STATS_BATTFULLCYCLES_LO),
						(getValue(BATTERY, BATTERY_STATS_POWERCYCLES_HI) << 8) + getValue(BATTERY, BATTERY_STATS_POWERCYCLES_HI),
						getValue(BATTERY, BATTERY_STATS_TBATTMAX),
						getValue(BATTERY, BATTERY_STATS_TBATTMIN));

		printChargeStats();

		if (hwVersion >= 60)
			printBatteryStats();
		else
			BTSerial.printf("No battery details supported by battery hardware #%d" _NL _NL, hwVersion);
	}

	hwVersion = getValue(MOTOR, MOTOR_REF_HW);
	if (hwVersion == 0)
		BTSerial.printf("Motor not responding" _NL _NL);
	else
	{
		BTSerial.printf("Motor information:" _NL
						" hardware version ........: %02d" _NL
						" software version ........: %02d" _NL
						" temperature .............: %02d" _DEGREE_SIGN "C" _NL
						" speed limit .............: %02d Km/h" _NL,
						hwVersion, getValue(MOTOR, MOTOR_REF_SW),
						getValue(MOTOR, MOTOR_REALTIME_TEMP),
						getValue(MOTOR, MOTOR_ASSIST_MAXSPEED));

		wheelCirc = (getValue(MOTOR, MOTOR_GEOMETRY_CIRC_HI) << 8) + getValue(MOTOR, MOTOR_GEOMETRY_CIRC_LO);
		BTSerial.printf(" wheel circumference .....: %d mm" _NL _NL, wheelCirc);

		BTSerial.printf(" part number .............: %05d" _NL
						" item number .............: %05d" _NL _NL,
						((getValue(MOTOR, MOTOR_SN_PN_HI) << 8) + getValue(MOTOR, MOTOR_SN_PN_LO)),
						((getValue(MOTOR, MOTOR_SN_ITEM_HI) << 8) + getValue(MOTOR, MOTOR_SN_ITEM_LO)));
	}
}

void shutdown()
{
	BTSerial.println("Shutting the system down");
	setValue(BATTERY, BATTERY_CONFIG_SHUTDOWN, 1);
}

void packetCapture()
{
	BTSerial.println("Capturing packets...");

	twai_message_t message;

	while (BTSerial.available() == 0)
	{
		if (twai_receive(&message, pdMS_TO_TICKS(1000)) == ESP_OK)
		{

			BTSerial.printf("\nPacket from: %s\n", getNodeName(message.identifier));

			for (int i = 0; i < message.data_length_code; i++)
			{
				BTSerial.printf("%02X ", message.data[i]);
			}

			BTSerial.println();
		}
	}

	BTSerial.readString();

	BTSerial.println("Done capturing packets");
}

void setup()
{
	BTSerial.begin(115200);
	// Wait for Bluetooth serial to connect before doing anything
	while (!BTSerial.connected())
		;

	// Initialize configuration structures using macro initializers
	twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_4, TWAI_MODE_NORMAL);
	twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
	twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

	// Install TWAI driver
	if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK)
	{
		BTSerial.printf("CAN driver installed\n");
	}
	else
	{
		BTSerial.printf("Failed to install the CAN driver\n");
		return;
	}

	// Start TWAI driver
	if (twai_start() == ESP_OK)
	{
		BTSerial.printf("CAN driver started\n");
	}
	else
	{
		BTSerial.printf("Failed to start the CAN driver\n");
		return;
	}

	BTSerial.printf("Welcome. Before giving any commands put the console into slave mode using n. Send h for help.");
}

void loop()
{
	// Get commands from BTSerial
	while (BTSerial.available() == 0)
		;
	String command = BTSerial.readString();
	command.trim();

	int space = command.indexOf(' ');

	if (space != 1)
	{
		switch (command.charAt(0))
		{
		case 'p':
			shutdown();
			break;
		case 's':
			printSystemSettings();
			break;
		case 'i':
			packetCapture();
			break;
		case 'n':
		{
			int consoleInSlaveMode = getValue(CONSOLE, CONSOLE_STATUS_SLAVE);
			if (consoleInSlaveMode)
			{
				BTSerial.printf("Console already in slave mode. good!" _NL _NL);
			}
			else
			{
				bool gConsoleSetSlaveMode = true;
				if (gConsoleSetSlaveMode)
				{
					int retry = 20;

					BTSerial.printf("Putting the console in slave mode ... ");
					do
					{
						setValue(CONSOLE, CONSOLE_STATUS_SLAVE, 1);
						consoleInSlaveMode = getValue(CONSOLE, CONSOLE_STATUS_SLAVE);
						delay(200);
					} while (retry-- && !consoleInSlaveMode);

					delay(500); // give the console some time to settle
					BTSerial.printf("%s" _NL _NL, consoleInSlaveMode ? "done" : "failed");
				}
				else
					BTSerial.printf("console not in slave mode" _NL _NL);
			}
		}
		default:
			usage();
			break;
		}
		return;
	}

	// Space at index 1 is ensured by the previous if statement
	char action = command.charAt(space - 1);
	String value_string = command.substring(2);

	int doShutdown = 0;
	switch (action)
	{
	case 'l':
	{
		float speedLimit = value_string.toFloat();
		if (speedLimit > UNLIMITED_SPEED_VALUE || speedLimit < 0)
		{
			BTSerial.printf("ERROR: Speed limit %.2f is out of range." _NL, speedLimit);
			return;
		}

		if (setSpeedLimit > 0)
		{
			BTSerial.printf("Set speed limit to %0.2f km/h" _NL, speedLimit);
			setSpeedLimit(speedLimit);
			doShutdown = 1;
		}
		else
		{
			BTSerial.println("Disabled speed limit, drive carefully");
			setSpeedLimit(0);
			doShutdown = 1;
		}
		break;
	}
	case 't':
	{
		float throttleSpeedLimit = value_string.toFloat();
		if (throttleSpeedLimit > MAX_THROTTLE_SPEED_VALUE || throttleSpeedLimit < 0)
		{
			BTSerial.printf("ERROR: Throttle speed limit %.2f is out of range." _NL, throttleSpeedLimit);
			return;
		}
		if (throttleSpeedLimit > 0)
		{
			BTSerial.printf("Set throttle speed limit to %0.2f km/h" _NL, throttleSpeedLimit);
			setThrottleSpeedLimit(throttleSpeedLimit);
			doShutdown = 1;
		}
		else
		{
			BTSerial.println("Disabled throttle speed limit, drive carefully.");
			setThrottleSpeedLimit(0);
			doShutdown = 1;
		}
		break;
	}
	case 'm':
	{
		float minSpeedLimit = value_string.toFloat();
		if (minSpeedLimit > UNLIMITED_MIN_SPEED_VALUE || minSpeedLimit < 0)
		{
			BTSerial.printf("ERROR: Min speed limit %.2f is out of range." _NL, minSpeedLimit);
			return;
		}

		if (minSpeedLimit > 0)
		{
			BTSerial.printf("Set minimal speed limit to %0.2f km/h" _NL, minSpeedLimit);
			setMinSpeedLimit(minSpeedLimit);
			doShutdown = 1;
		}
		else
		{
			BTSerial.println("Disabled minimal speed limit, drive carefully.");
			setMinSpeedLimit(0);
			doShutdown = 1;
		}
		break;
	}
	case 'a':
	{
		int assistInitLevel = value_string.toInt();
		if (assistInitLevel > 4 || assistInitLevel < 0)
		{
			BTSerial.printf("ERROR: Initial assist level %d is out of range." _NL, assistInitLevel);
			return;
		}

		BTSerial.printf("Setting initial assistance level to %d" _NL, assistInitLevel);
		setValue(CONSOLE, CONSOLE_ASSIST_INITLEVEL, assistInitLevel);
		break;
	}
	case 'o':
	{
		int mountainCap = value_string.toInt();
		if (mountainCap > 100 || mountainCap < 0)
		{
			BTSerial.printf("ERROR: Mountain cap level %d is out of range." _NL, mountainCap);
			return;
		}

		BTSerial.printf("Set mountain cap level to %0.2f%%" _NL, ((int)mountainCap / 1.5625) * 1.5625);
		setValue(CONSOLE, CONSOLE_ASSIST_MOUNTAINCAP, mountainCap / 1.5625);
		break;
	}
	case 'c':
	{
		int wheelCircumference = value_string.toInt();
		if (wheelCircumference > 3000 || wheelCircumference < 1000)
		{
			BTSerial.printf("ERROR: wheel circumference %d is out of range." _NL, wheelCircumference);
			return;
		}

		BTSerial.printf("Set wheel circumference to %d" _NL, wheelCircumference);
		setWheelCircumference(wheelCircumference);
		break;
	}
	default:
		usage();
	}

	if (doShutdown)
	{
		BTSerial.println("Don't forget to shut down!");
	}
}
