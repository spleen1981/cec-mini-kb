#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include "libcec/include/cec.h"

#define KEYPRESS_DURATION 50000

// cecloader.h uses std::cout _without_ including iosfwd or iostream
// Furthermore is uses cout and not std::cout
#include <iostream>
using std::cout;
using std::endl;
#include "libcec/include/cecloader.h"

#include <algorithm>  // for std::min
#include <array>
#include <chrono>
#include <thread>

// The main loop will just continue until a ctrl-C is received
#include <signal.h>
bool exit_now = false;
int fd=-1;

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
	default: {
		std::cout << "Unmapped input: " << static_cast<int>(msg->keycode) << std::endl;
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
	 * created, to pass key events, in this case the space key.
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

int main(int argc, char* argv[])
{
	int return_value=0;

	if( SIG_ERR == signal(SIGINT, handle_signal) || SIG_ERR == signal(SIGTERM, handle_signal) )
	{
		std::cerr << "Failed to install the SIGINT/SIGTERM signal handler\n";
		return 1;
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
	if( !cec_adapter->Open(devices[0].strComName) )
	{
		std::cerr << "Failed to open the CEC device on port " << devices[0].strComName << std::endl;
		UnloadLibCec(cec_adapter);
		return_value=5;
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
