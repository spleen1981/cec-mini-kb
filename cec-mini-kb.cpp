/*
 * Copyright (C) 2022 Giovanni Cascione <ing.cascione@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include "libcec/cec.h"

#define KEYPRESS_DURATION 50000

// cecloader.h uses std::cout _without_ including iosfwd or iostream
// Furthermore is uses cout and not std::cout
#include <iostream>
using std::cout;
using std::endl;
#include "libcec/cecloader.h"

#include <algorithm>  // for std::min
#include <array>
#include <chrono>
#include <thread>

// The main loop will just continue until a ctrl-C is received
#include <signal.h>
bool exit_now = false;
int fd=-1;
std::string poweroff_command="";

void handle_signal(int signal)
{
	exit_now = true;
}

void emit(int type, int code, int val){
	struct input_event ie;

	ie.type = type;
	ie.code = code;
	ie.value = val;
	/* timestamp values below are ignored */
	ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;

	write(fd, &ie, sizeof(ie));
}

void send_keypress(int key){
	emit(EV_KEY, key, 1);
	emit(EV_SYN, SYN_REPORT, 0);
	usleep(KEYPRESS_DURATION);
	emit(EV_KEY, key, 0);
	emit(EV_SYN, SYN_REPORT, 0);
}

void on_command(void* not_used, const CEC::cec_command* msg){
	if (msg->opcode==CEC::CEC_OPCODE_STANDBY){
		if (system(poweroff_command.c_str())){
			std::cerr << "Failed to run power-off command: " << poweroff_command << std::endl;
		}
	}
}

void on_keypress(void* not_used, const CEC::cec_keypress* msg)
{
	if (msg->duration) return; //filter key releases interpreted as additional key presses by libcec, as duration is only given for key release

	int key;
	std::string key_pressed;

	//CEC keycode ref: https://github.com/Pulse-Eight/libcec/blob/master/include/cectypes.h

	switch(msg->keycode){
	case CEC::CEC_USER_CONTROL_CODE_SELECT: { key = KEY_ENTER; break; }
	case CEC::CEC_USER_CONTROL_CODE_UP: { key = KEY_UP; break; }
	case CEC::CEC_USER_CONTROL_CODE_DOWN: { key = KEY_DOWN; break; }
	case CEC::CEC_USER_CONTROL_CODE_LEFT: { key = KEY_LEFT; break; }
	case CEC::CEC_USER_CONTROL_CODE_RIGHT: { key = KEY_RIGHT; break; }
	case CEC::CEC_USER_CONTROL_CODE_EXIT: { key = KEY_BACKSPACE; break; }
	case CEC::CEC_USER_CONTROL_CODE_F1_BLUE: { key = KEY_ESC; break; }
	case CEC::CEC_USER_CONTROL_CODE_F2_RED: { key = KEY_LEFTSHIFT; break; }
	case CEC::CEC_USER_CONTROL_CODE_F3_GREEN: { key = KEY_SPACE; break; }
	case CEC::CEC_USER_CONTROL_CODE_F4_YELLOW: { key = KEY_DELETE; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER0: { key = KEY_0; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER1: { key = KEY_1; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER2: { key = KEY_2; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER3: { key = KEY_3; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER4: { key = KEY_4; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER5: { key = KEY_5; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER6: { key = KEY_6; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER7: { key = KEY_7; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER8: { key = KEY_8; break; }
	case CEC::CEC_USER_CONTROL_CODE_NUMBER9: { key = KEY_9; break; }

	default: {
		std::cout << "Unmapped input: " << static_cast<int>(msg->keycode) << std::endl;
		std::cout.flush();
		return;}
	};

	send_keypress(key);
}

int uinput_dev_init(void){
	struct uinput_setup usetup;
	fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

	if ( fd < 0 )
		return fd;
	/*
	 * The ioctls below will enable the device that is about to be
	 * created, to pass key events.
	 */
	//Key codes ref: https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_EVBIT, EV_SYN);

	ioctl(fd, UI_SET_KEYBIT, KEY_UP);
	ioctl(fd, UI_SET_KEYBIT, KEY_DOWN);
	ioctl(fd, UI_SET_KEYBIT, KEY_LEFT);
	ioctl(fd, UI_SET_KEYBIT, KEY_RIGHT);
	ioctl(fd, UI_SET_KEYBIT, KEY_ENTER);
	ioctl(fd, UI_SET_KEYBIT, KEY_BACKSPACE);
	ioctl(fd, UI_SET_KEYBIT, KEY_ESC);
  	ioctl(fd, UI_SET_KEYBIT, KEY_LEFTSHIFT);
  	ioctl(fd, UI_SET_KEYBIT, KEY_SPACE);
  	ioctl(fd, UI_SET_KEYBIT, KEY_DELETE);
	ioctl(fd, UI_SET_KEYBIT, KEY_0);
	ioctl(fd, UI_SET_KEYBIT, KEY_1);
	ioctl(fd, UI_SET_KEYBIT, KEY_2);
	ioctl(fd, UI_SET_KEYBIT, KEY_3);
	ioctl(fd, UI_SET_KEYBIT, KEY_4);
	ioctl(fd, UI_SET_KEYBIT, KEY_5);
	ioctl(fd, UI_SET_KEYBIT, KEY_6);
	ioctl(fd, UI_SET_KEYBIT, KEY_7);
	ioctl(fd, UI_SET_KEYBIT, KEY_8);
	ioctl(fd, UI_SET_KEYBIT, KEY_9);


	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = 0x1234; /* sample vendor */
	usetup.id.product = 0x5678; /* sample product */
	strcpy(usetup.name, "cec-mini-kb");

	ioctl(fd, UI_DEV_SETUP, &usetup);

	if (ioctl(fd, UI_DEV_CREATE, NULL) < 0){
		return -1;
	}

	return 0;
}

void uinput_dev_deinit(void){
	if (fd){
		ioctl(fd, UI_DEV_DESTROY);
		close(fd);
	}
}

bool legal_int(char *str)
{
    char* end;
    long int num = std::strtol(str, &end, 10);

    if (*end == '\0') {
    	return true;
    }
    else {
        return false;
    }
}

static void show_usage(std::string name)
{
	std::cout << "\nUsage: " << name << " <option(s)>\n"
		<< "Options:\n"
		<< "\t-h,--help		Show this help message\n"
		<< "\t-a,--adapter NUM	Adapter to use (default 0)\n"
		<< "\t-p,--poweroff COMMAND	Specify a command to be executed from shell when power standby signal is received.\n"
		<< "\nKey bindings\n"
		<< "\tCEC_USER_CONTROL_CODE_SELECT:		KEY_ENTER\n"
		<< "\tCEC_USER_CONTROL_CODE_UP:		KEY_UP\n"
		<< "\tCEC_USER_CONTROL_CODE_DOWN:		KEY_DOWN\n"
		<< "\tCEC_USER_CONTROL_CODE_LEFT:		KEY_LEFT\n"
		<< "\tCEC_USER_CONTROL_CODE_RIGHT:		KEY_RIGHT\n"
		<< "\tCEC_USER_CONTROL_CODE_EXIT:		KEY_BACKSPACE\n"
		<< "\tCEC_USER_CONTROL_CODE_F1_BLUE:		KEY_ESC\n"
		<< "\tCEC_USER_CONTROL_CODE_F2_RED:		KEY_LEFTSHIFT\n"
		<< "\tCEC_USER_CONTROL_CODE_F3_GREEN:		KEY_SPACE\n"
		<< "\tCEC_USER_CONTROL_CODE_F4_YELLOW:	KEY_DELETE\n"
		<< "\tCEC_USER_CONTROL_CODE_NUMBER{0 to 9}:	KEY_{0 to 9}\n"
		<< std::endl;
	std::cout.flush();
}

int main(int argc, char* argv[])
{
	int return_value=0;
	int adapter_number=0;

	if( SIG_ERR == signal(SIGINT, handle_signal) || SIG_ERR == signal(SIGTERM, handle_signal) )
	{
		std::cerr << "Failed to install the SIGINT/SIGTERM signal handler\n";
		return 1;
	}

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if ((arg == "-h") || (arg == "--help")) {
			show_usage(argv[0]);
			return 0;
		} else if ((arg == "-a") || (arg == "--adapter")) {
			if (i + 1 < argc) {
				i++;
				if (legal_int(argv[i])){
					adapter_number = atoi(argv[i]);
				} else {
					std::cerr << "--adapter option requires a integer number argument." << std::endl;
					return 1;
				}
			} else {
				std::cerr << "--adapter option requires one argument." << std::endl;
				return 1;
			}
		} else if ((arg == "-p") || (arg == "--poweroff")) {
			if (i + 1 < argc) {
				i++;
				poweroff_command = argv[i];
			} else {
				std::cerr << "--poweroff option requires one argument." << std::endl;
				return 1;
			}
		}
	}
	if (uinput_dev_init()){
		std::cerr << "Unable to initialize uinput device!\n";
		return 2;
	}

	// Set up the CEC config and specify the keypress callback function
	CEC::ICECCallbacks        cec_callbacks;
	CEC::libcec_configuration cec_config;
	cec_config.Clear();
	cec_callbacks.Clear();

	const std::string devicename("CEC_device");
	devicename.copy(cec_config.strDeviceName, std::min(devicename.size(), static_cast<std::string::size_type>(13)) );

	cec_config.clientVersion       = CEC::LIBCEC_VERSION_CURRENT;
	cec_config.bActivateSource     = 0;
	cec_config.callbacks           = &cec_callbacks;
	cec_config.deviceTypes.Add(CEC::CEC_DEVICE_TYPE_RECORDING_DEVICE);

	cec_callbacks.keyPress    = &on_keypress;

	if (poweroff_command!="")
	{
		cec_callbacks.commandReceived    = &on_command;
	}

	// Get a cec adapter by initialising the cec library
	CEC::ICECAdapter* cec_adapter = LibCecInitialise(&cec_config);
	if( !cec_adapter )
	{
		std::cerr << "Failed loading libcec.so\n";
		return_value=3;
		goto exit;
	}

	// Try to automatically determine the CEC devices
	std::array<CEC::cec_adapter_descriptor,10> devices;
	{int8_t devices_found = cec_adapter->DetectAdapters(devices.data(), devices.size(), nullptr, true /*quickscan*/);
	if( devices_found <= 0)
	{
		std::cerr << "Could not automatically determine the cec adapter devices\n";
		UnloadLibCec(cec_adapter);
		return_value=4;
		goto exit;
	}}

	// Open a connection to the zeroth CEC device
	if( !cec_adapter->Open(devices[adapter_number].strComName))
	{
		std::cerr << "Failed to open the CEC device on port " << devices[0].strComName << std::endl;
		UnloadLibCec(cec_adapter);
		return_value=5;
		goto exit;
	}

	// Activate source
	if( !cec_adapter->SetActiveSource(cec_config.deviceTypes[0]))
	{
		std::cerr << "Failed to become active" << std::endl;
		UnloadLibCec(cec_adapter);
		return_value=6;
		goto exit;
	}

	// Loop until ctrl-C occurs
	while( !exit_now )
	{
		// nothing to do.  All happens in the CEC callback on another thread
		std::this_thread::sleep_for( std::chrono::seconds(1) );
	}

	exit:
	// Close down and cleanup
	cec_adapter->Close();
	UnloadLibCec(cec_adapter);
	uinput_dev_deinit();
	return return_value;
}
