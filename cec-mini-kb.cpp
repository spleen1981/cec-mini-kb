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
#define KEYPRESS_SELECT_INTERVAL 500

// cecloader.h uses std::cout _without_ including iosfwd or iostream
// Furthermore is uses cout and not std::cout
#include <iostream>
#include <fstream>
#include <sstream>
using std::cout;
using std::endl;
#include "libcec/cecloader.h"

#include <algorithm>  // for std::min
#include <array>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <thread>

// The main loop will just continue until a ctrl-C is received
#include <signal.h>

enum {
	ERROR_CODE_WRONG_ARGS = 1,
	ERROR_CODE_UINPUT_DEV_INIT,
	ERROR_CODE_LIBCEC_LOADING,
	ERROR_CODE_NO_DEV_FOUND,
	ERROR_CODE_CANT_OPEN_DEVICE,
	ERROR_CODE_SOURCE_NOT_ACTIVE,
	ERROR_CODE_DEV_NOT_FOUND,
	ERROR_CODE_LOAD_MAPPINGS
} return_value;

bool exit_now = false;
int fd = -1;
int last_cec_keypressed_code=-1;
int last_cec_keypressed_time=-1;
int last_keypressed_index=-1;


std::unordered_map<int, std::vector<std::vector<int>>> bindings_map={
		{ CEC::CEC_USER_CONTROL_CODE_SELECT , {{KEY_ENTER}}},
		{ CEC::CEC_USER_CONTROL_CODE_UP , {{KEY_UP }}},
		{ CEC::CEC_USER_CONTROL_CODE_DOWN , {{KEY_DOWN }}},
		{ CEC::CEC_USER_CONTROL_CODE_LEFT , {{KEY_LEFT }}},
		{ CEC::CEC_USER_CONTROL_CODE_RIGHT , {{KEY_RIGHT }}},
		{ CEC::CEC_USER_CONTROL_CODE_EXIT , {{KEY_BACKSPACE }}},
		{ CEC::CEC_USER_CONTROL_CODE_F1_BLUE , {{KEY_ESC }}},
		{ CEC::CEC_USER_CONTROL_CODE_F2_RED , {{KEY_LEFTSHIFT }}},
		{ CEC::CEC_USER_CONTROL_CODE_F3_GREEN , {{KEY_SPACE }}},
		{ CEC::CEC_USER_CONTROL_CODE_F4_YELLOW , {{KEY_DELETE }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER0 , {{KEY_0 },{KEY_SPACE }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER1 , {{KEY_1 }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER2 , {{KEY_2 },{KEY_A },{KEY_B },{KEY_C }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER3 , {{KEY_3 },{KEY_D },{KEY_E },{KEY_F }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER4 , {{KEY_4 },{KEY_G },{KEY_H },{KEY_I }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER5 , {{KEY_5 },{KEY_J },{KEY_K },{KEY_K }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER6 , {{KEY_6 },{KEY_M },{KEY_N },{KEY_O }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER7 , {{KEY_7 },{KEY_P },{KEY_Q },{KEY_R },{KEY_S }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER8 , {{KEY_8 },{KEY_T },{KEY_U },{KEY_V }}},
		{ CEC::CEC_USER_CONTROL_CODE_NUMBER9 , {{KEY_9 },{KEY_W },{KEY_X },{KEY_Y },{KEY_Z }}},
		{ CEC::CEC_USER_CONTROL_CODE_FORWARD , {{KEY_TAB }}},
		{ CEC::CEC_USER_CONTROL_CODE_BACKWARD , {{KEY_LEFTSHIFT,KEY_TAB}}}
	};

std::string poweroff_command = "";

void load_bindings_map(std::ifstream &infile) {
	bindings_map = { };

	std::string line;
	while (std::getline(infile, line)) {
		for (int i = 0; i < line.length(); i++) {// remove spaces
		    if (line[i] == ' ') {
		    	line.erase(i, 1);
		        i--;
		    }
		}
		size_t pos = line.find(';');
		if (pos != std::string::npos) {// remove comments
			line.erase(pos);
		}
		if (line.empty()) { // skip empty lines
			continue;
		}
		std::stringstream ss(line);
		std::string key_str, value_str;
		if (std::getline(ss, key_str, ':') && std::getline(ss, value_str)) {
			int clave = std::stoi(key_str);
			std::stringstream ss_value(value_str);
			std::string array_str;
			std::vector<std::vector<int>> array_2d;
			while (std::getline(ss_value, array_str, ',')) {
				std::stringstream ss_array(array_str);
				std::vector<int> array;
				int value;
				while (ss_array >> value) {
					array.push_back(value);
					if (ss_array.peek() == '+')
						ss_array.ignore();
				}
				array_2d.push_back(array);
			}
			bindings_map[clave] = array_2d;
		}
	}

// testing
//	for (auto &pair : bindings_map) {
//		std::cout << pair.first << ":";
//		for (auto &array : pair.second) {
//			for (auto &element : array) {
//				std::cout << element << "+";
//			}
//			std::cout << ",";
//		}
//		std::cout << "\n";
//	}
}

void handle_signal(int signal) {
	exit_now = true;
}

void emit(int type, int code, int val) {
	struct input_event ie;

	ie.type = type;
	ie.code = code;
	ie.value = val;
	/* timestamp values below are ignored */
	ie.time.tv_sec = 0;
	ie.time.tv_usec = 0;

	write(fd, &ie, sizeof(ie));
}

void send_keypresses(std::vector<int> keys) {
	for (auto& key : keys) {
		emit(EV_KEY, key, 1);
	}
	emit(EV_SYN, SYN_REPORT, 0);
	usleep(KEYPRESS_DURATION);
	for (auto& key : keys) {
		emit(EV_KEY, key, 0);
	}
	emit(EV_SYN, SYN_REPORT, 0);
}

void on_command(void *not_used, const CEC::cec_command *msg) {
	if (msg->opcode == CEC::CEC_OPCODE_STANDBY) {
		if (system(poweroff_command.c_str())) {
			std::cerr << "Failed to run power-off command: " << poweroff_command << std::endl;
		}
	}
}

void on_keypress(void *not_used, const CEC::cec_keypress *msg) {
	if (msg->duration) return; //filter key releases interpreted as additional key presses by libcec, as duration is only given for key release

	//CEC keycode ref: https://github.com/Pulse-Eight/libcec/blob/master/include/cectypes.h
	if ( bindings_map.count(msg->keycode)) {
		std::vector<std::vector<int>> key_groups=bindings_map[msg->keycode];

		int now= std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

		if (msg->keycode==last_cec_keypressed_code && (now-last_cec_keypressed_time)<KEYPRESS_SELECT_INTERVAL){
			last_keypressed_index++;
			last_keypressed_index%=key_groups.size();
			send_keypresses({KEY_BACKSPACE });
		}
		else{
			last_cec_keypressed_code=msg->keycode;
			last_keypressed_index=0;
		}
		last_cec_keypressed_time=now;

		std::vector<int> keys=key_groups[last_keypressed_index];
		send_keypresses(keys);
	}else{
		std::cout << "Unmapped input: " << static_cast<int>(msg->keycode) << std::endl;
		std::cout.flush();
		return;
	}

}

int uinput_dev_init(void) {
	struct uinput_setup usetup;
	fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

	if (fd < 0)
		return fd;
	/*
	 * The ioctls below will enable the device that is about to be
	 * created, to pass key events.
	 */
	//Key codes ref: https://github.com/torvalds/linux/blob/master/include/uapi/linux/input-event-codes.h

	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_EVBIT, EV_SYN);

	for (auto const& cec_input : bindings_map) {
		for (auto& key_groups : cec_input.second ) {
			for (auto& key_code : key_groups) {
				ioctl(fd, UI_SET_KEYBIT, key_code);
			}
		}
	}

	memset(&usetup, 0, sizeof(usetup));
	usetup.id.bustype = BUS_USB;
	usetup.id.vendor = 0x1234; /* sample vendor */
	usetup.id.product = 0x5678; /* sample product */
	strcpy(usetup.name, "cec-mini-kb");

	ioctl(fd, UI_DEV_SETUP, &usetup);

	if (ioctl(fd, UI_DEV_CREATE, NULL) < 0) {
		return -1;
	}

	return 0;
}

void uinput_dev_deinit(void) {
	if (fd) {
		ioctl(fd, UI_DEV_DESTROY);
		close(fd);
	}
}

static void show_usage(std::string name) {
	std::cout << "\nUsage: " << name << " <option(s)>\n"
	          << "Options:\n"
	          << "\t-h,--help		Show this help message\n"
	          << "\t-a,--adapter NUM	Adapter to use (0-9, default 0)\n"
	          << "\t-b,--bindings FILE	Specify a map bindings file\n"
			  << "\t-p,--poweroff COMMAND	Specify a command to be executed from shell when power standby signal is received.\n"
	          << "\nDefault key bindings\n"
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
	          << "\tCEC_USER_CONTROL_CODE_FORWARD:	KEY_TAB\n"
	          << "\tCEC_USER_CONTROL_CODE_BACKWARD:	KEY_LEFTSHIFT + KEY_TAB (key combination)\n"
	          << "\tCEC_USER_CONTROL_CODE_NUMBER{0 to 9}:	KEY_{0 to 9} and ABC DEF ... WXYZ with multiple presses, like old cellphones\n"
			  << std::endl;
	std::cout.flush();
}

int main(int argc, char *argv[]) {
	int return_value = 0;
	int adapter_number = 0;
	if (SIG_ERR == signal(SIGINT, handle_signal) || SIG_ERR == signal(SIGTERM, handle_signal)) {
		std::cerr << "Failed to install the SIGINT/SIGTERM signal handler\n";
		return ERROR_CODE_WRONG_ARGS;
	}

	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if ((arg == "-h") || (arg == "--help")) {
			show_usage(argv[0]);
			return 0;
		} else if ((arg == "-a") || (arg == "--adapter")) {
			if (i + 1 < argc) {
				i++;
				char adapter_arg_buf[] = {*argv[i], '\0'};
				adapter_number = atoi(adapter_arg_buf);
				if (*argv[i] != '0' && !adapter_number) {
					std::cerr << "--adapter option requires a integer number argument between 0 and 9." << std::endl;
					return ERROR_CODE_WRONG_ARGS;
				}
			} else {
				std::cerr << "--adapter option requires one argument." << std::endl;
				return ERROR_CODE_WRONG_ARGS;
			}
		} else if ((arg == "-b") || (arg == "--bindings")) {
					if (i + 1 < argc) {
						i++;
						std::ifstream infile(argv[i]);
						if (!infile) {
							std::cerr << "Unable to open mappings file.";
						    return ERROR_CODE_LOAD_MAPPINGS;
						}
						load_bindings_map(infile);
					} else {
						std::cerr << "--bindings option requires bindings map file name." << std::endl;
						return ERROR_CODE_WRONG_ARGS;
					}
		} else if ((arg == "-p") || (arg == "--poweroff")) {
			if (i + 1 < argc) {
				i++;
				poweroff_command = argv[i];
			} else {
				std::cerr << "--poweroff option requires one argument." << std::endl;
				return ERROR_CODE_WRONG_ARGS;
			}
		}
	}
	if (uinput_dev_init()) {
		std::cerr << "Unable to initialize uinput device!\n";
		return ERROR_CODE_UINPUT_DEV_INIT;
	}

	// Set up the CEC config and specify the keypress callback function
	CEC::ICECCallbacks        cec_callbacks;
	CEC::libcec_configuration cec_config;
	cec_config.Clear();
	cec_callbacks.Clear();

	const std::string devicename("CEC_device");
	devicename.copy(cec_config.strDeviceName, std::min(devicename.size(), static_cast<std::string::size_type>(13)));

	cec_config.clientVersion       = CEC::LIBCEC_VERSION_CURRENT;
	cec_config.bActivateSource     = 0;
	cec_config.callbacks           = &cec_callbacks;
	cec_config.deviceTypes.Add(CEC::CEC_DEVICE_TYPE_RECORDING_DEVICE);

	cec_callbacks.keyPress    = &on_keypress;

	if (poweroff_command != "") {
		cec_callbacks.commandReceived    = &on_command;
	}

	// Get a cec adapter by initialising the cec library
	CEC::ICECAdapter *cec_adapter = LibCecInitialise(&cec_config);
	if (!cec_adapter) {
		std::cerr << "Failed loading libcec.so\n";
		return_value = ERROR_CODE_LIBCEC_LOADING;
		goto exit;
	}

	// Try to automatically determine the CEC devices
	std::array<CEC::cec_adapter_descriptor, 10> devices;
	{
		int8_t devices_found = cec_adapter->DetectAdapters(devices.data(), devices.size(), nullptr, true /*quickscan*/);
		if (devices_found <= 0) {
			std::cerr << "Could not detect the cec adapter devices\n";
			return_value = ERROR_CODE_NO_DEV_FOUND;
			goto exit;
		} else if (devices_found < adapter_number + 1) {
			std::cerr << "The required cec adapter device was not found\n";
			return_value = ERROR_CODE_DEV_NOT_FOUND;
			goto exit;
		}
	}

	// Open a connection to the target CEC device
	if (!cec_adapter->Open(devices[adapter_number].strComName)) {
		std::cerr << "Failed to open the CEC device on port " << devices[adapter_number].strComName << std::endl;
		return_value = ERROR_CODE_CANT_OPEN_DEVICE;
		goto exit;
	}

	// Activate source
	if (!cec_adapter->SetActiveSource(cec_config.deviceTypes[0])) {
		std::cerr << "Failed to become active" << std::endl;
		return_value = ERROR_CODE_SOURCE_NOT_ACTIVE;
		goto exit;
	}

	// Loop until ctrl-C occurs
	while (!exit_now) {
		// nothing to do.  All happens in the CEC callback on another thread
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}

exit:
	// Close down and cleanup
	cec_adapter->Close();
	UnloadLibCec(cec_adapter);
	uinput_dev_deinit();
	return return_value;
}
