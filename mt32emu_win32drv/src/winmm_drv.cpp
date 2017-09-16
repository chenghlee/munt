/* Copyright (C) 2003, 2004, 2005 Dean Beeler, Jerome Fisher
 * Copyright (C) 2011-2017 Dean Beeler, Jerome Fisher, Sergey V. Mikayev
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"

#define MAX_DRIVERS 8
#define MAX_CLIENTS 8 // Per driver

namespace {

static bool hrTimerAvailable;
static double mult;
static LARGE_INTEGER counter;
static LARGE_INTEGER nanoCounter = {0L};

void initNanoTimer() {
	LARGE_INTEGER freq = {0L};
	if (QueryPerformanceFrequency(&freq)) {
		hrTimerAvailable = true;
		mult = 1E9 / freq.QuadPart;
	} else {
		hrTimerAvailable = false;
	}
}

void updateNanoCounter() {
	if (hrTimerAvailable) {
		QueryPerformanceCounter(&counter);
		nanoCounter.QuadPart = (long long)(counter.QuadPart * mult);
	} else {
		DWORD currentTime = timeGetTime();
		if (currentTime < counter.LowPart) counter.HighPart++;
		counter.LowPart = currentTime;
		nanoCounter.QuadPart = counter.QuadPart * 1000000;
	}
}

using namespace MT32Emu;

static MidiSynth &midiSynth = MidiSynth::getInstance();
static bool synthOpened = false;
static HWND hwnd = NULL;
static int driverCount;

struct Driver {
	bool open;
	int clientCount;
	HDRVR hdrvr;
	struct Client {
		bool allocated;
		DWORD_PTR instance;
		DWORD flags;
		DWORD_PTR callback;
		DWORD synth_instance;
		MidiStreamParser *midiStreamParser;
	} clients[MAX_CLIENTS];
} drivers[MAX_DRIVERS];

STDAPI_(LONG) DriverProc(DWORD dwDriverID, HDRVR hdrvr, WORD wMessage, DWORD dwParam1, DWORD dwParam2) {
	switch(wMessage) {
	case DRV_LOAD:
		memset(drivers, 0, sizeof(drivers));
		driverCount = 0;
		return DRV_OK;
	case DRV_ENABLE:
		return DRV_OK;
	case DRV_OPEN:
		int driverNum;
		if (driverCount == MAX_DRIVERS) {
			return 0;
		} else {
			for (driverNum = 0; driverNum < MAX_DRIVERS; driverNum++) {
				if (!drivers[driverNum].open) {
					break;
				}
				if (driverNum == MAX_DRIVERS) {
					return 0;
				}
			}
		}
		initNanoTimer();
		drivers[driverNum].open = true;
		drivers[driverNum].clientCount = 0;
		drivers[driverNum].hdrvr = hdrvr;
		driverCount++;
		return DRV_OK;
	case DRV_INSTALL:
	case DRV_PNPINSTALL:
		return DRV_OK;
	case DRV_QUERYCONFIGURE:
		return 0;
	case DRV_CONFIGURE:
		return DRVCNF_OK;
	case DRV_CLOSE:
		for (int i = 0; i < MAX_DRIVERS; i++) {
			if (drivers[i].open && drivers[i].hdrvr == hdrvr) {
				drivers[i].open = false;
				--driverCount;
				return DRV_OK;
			}
		}
		return DRV_CANCEL;
	case DRV_DISABLE:
		return DRV_OK;
	case DRV_FREE:
		return DRV_OK;
	case DRV_REMOVE:
		return DRV_OK;
	}
	return DRV_OK;
}


HRESULT modGetCaps(PVOID capsPtr, DWORD capsSize) {
	MIDIOUTCAPSA * myCapsA;
	MIDIOUTCAPSW * myCapsW;
	MIDIOUTCAPS2A * myCaps2A;
	MIDIOUTCAPS2W * myCaps2W;

	CHAR synthName[] = "MT-32 Synth Emulator\0";
	WCHAR synthNameW[] = L"MT-32 Synth Emulator\0";

	switch (capsSize) {
	case (sizeof(MIDIOUTCAPSA)):
		myCapsA = (MIDIOUTCAPSA *)capsPtr;
		myCapsA->wMid = MM_UNMAPPED;
		myCapsA->wPid = MM_MPU401_MIDIOUT;
		memcpy(myCapsA->szPname, synthName, sizeof(synthName));
		myCapsA->wTechnology = MOD_MIDIPORT;
		myCapsA->vDriverVersion = 0x0090;
		myCapsA->wVoices = 0;
		myCapsA->wNotes = 0;
		myCapsA->wChannelMask = 0xffff;
		myCapsA->dwSupport = 0;
		return MMSYSERR_NOERROR;

	case (sizeof(MIDIOUTCAPSW)):
		myCapsW = (MIDIOUTCAPSW *)capsPtr;
		myCapsW->wMid = MM_UNMAPPED;
		myCapsW->wPid = MM_MPU401_MIDIOUT;
		memcpy(myCapsW->szPname, synthNameW, sizeof(synthNameW));
		myCapsW->wTechnology = MOD_MIDIPORT;
		myCapsW->vDriverVersion = 0x0090;
		myCapsW->wVoices = 0;
		myCapsW->wNotes = 0;
		myCapsW->wChannelMask = 0xffff;
		myCapsW->dwSupport = 0;
		return MMSYSERR_NOERROR;

	case (sizeof(MIDIOUTCAPS2A)):
		myCaps2A = (MIDIOUTCAPS2A *)capsPtr;
		myCaps2A->wMid = MM_UNMAPPED;
		myCaps2A->wPid = MM_MPU401_MIDIOUT;
		memcpy(myCaps2A->szPname, synthName, sizeof(synthName));
		myCaps2A->wTechnology = MOD_MIDIPORT;
		myCaps2A->vDriverVersion = 0x0090;
		myCaps2A->wVoices = 0;
		myCaps2A->wNotes = 0;
		myCaps2A->wChannelMask = 0xffff;
		myCaps2A->dwSupport = 0;
		return MMSYSERR_NOERROR;

	case (sizeof(MIDIOUTCAPS2W)):
		myCaps2W = (MIDIOUTCAPS2W *)capsPtr;
		myCaps2W->wMid = MM_UNMAPPED;
		myCaps2W->wPid = MM_MPU401_MIDIOUT;
		memcpy(myCaps2W->szPname, synthNameW, sizeof(synthNameW));
		myCaps2W->wTechnology = MOD_MIDIPORT;
		myCaps2W->vDriverVersion = 0x0090;
		myCaps2W->wVoices = 0;
		myCaps2W->wNotes = 0;
		myCaps2W->wChannelMask = 0xffff;
		myCaps2W->dwSupport = 0;
		return MMSYSERR_NOERROR;

	default:
		return MMSYSERR_ERROR;
	}
}

void DoCallback(int driverNum, DWORD_PTR clientNum, DWORD msg, DWORD_PTR param1, DWORD_PTR param2) {
	Driver::Client *client = &drivers[driverNum].clients[clientNum];
	DriverCallback(client->callback, client->flags, drivers[driverNum].hdrvr, msg, client->instance, param1, param2);
}

LONG OpenDriver(Driver &driver, UINT uDeviceID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
	int clientNum;
	if (driver.clientCount == 0) {
		clientNum = 0;
	} else if (driver.clientCount == MAX_CLIENTS) {
		return MMSYSERR_ALLOCATED;
	} else {
		int i;
		for (i = 0; i < MAX_CLIENTS; i++) {
			if (!driver.clients[i].allocated) {
				break;
			}
		}
		if (i == MAX_CLIENTS) {
			return MMSYSERR_ALLOCATED;
		}
		clientNum = i;
	}
	MIDIOPENDESC *desc = (MIDIOPENDESC *)dwParam1;
	driver.clients[clientNum].allocated = true;
	driver.clients[clientNum].flags = HIWORD(dwParam2);
	driver.clients[clientNum].callback = desc->dwCallback;
	driver.clients[clientNum].instance = desc->dwInstance;
	*(LONG *)dwUser = clientNum;
	driver.clientCount++;
	DoCallback(uDeviceID, clientNum, MOM_OPEN, NULL, NULL);
	return MMSYSERR_NOERROR;
}

LONG CloseDriver(Driver &driver, UINT uDeviceID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
	if (!driver.clients[dwUser].allocated) {
		return MMSYSERR_INVALPARAM;
	}
	driver.clients[dwUser].allocated = false;
	driver.clientCount--;
	DoCallback(uDeviceID, dwUser, MOM_CLOSE, NULL, NULL);
	return MMSYSERR_NOERROR;
}

class MidiStreamParserImpl : public MidiStreamParser {
public:
	MidiStreamParserImpl(Driver::Client &useClient) : client(useClient) {}

protected:
	virtual void handleShortMessage(const Bit32u message) {
		if (hwnd == NULL) {
			midiSynth.PlayMIDI(message);
		} else {
			updateNanoCounter();
			DWORD msg[] = { 0, 0, nanoCounter.LowPart, (DWORD)nanoCounter.HighPart, message }; // 0, short MIDI message indicator, timestamp, data
			COPYDATASTRUCT cds = { client.synth_instance, sizeof(msg), msg };
			LRESULT res = SendMessage(hwnd, WM_COPYDATA, NULL, (LPARAM)&cds);
			if (res != 1) {
				// Synth app was terminated. Fall back to integrated synth
				hwnd = NULL;
				if (midiSynth.Init() == 0) {
					synthOpened = true;
					midiSynth.PlayMIDI(message);
				}
			}
		}
	}

	virtual void handleSysex(const Bit8u stream[], const Bit32u length) {
		if (hwnd == NULL) {
			midiSynth.PlaySysex(stream, length);
		} else {
			COPYDATASTRUCT cds = { client.synth_instance, length, (PVOID)stream };
			LRESULT res = SendMessage(hwnd, WM_COPYDATA, NULL, (LPARAM)&cds);
			if (res != 1) {
				// Synth app was terminated. Fall back to integrated synth
				hwnd = NULL;
				if (midiSynth.Init() == 0) {
					synthOpened = true;
					midiSynth.PlaySysex(stream, length);
				}
			}
		}
	}

	virtual void handleSystemRealtimeMessage(const Bit8u realtime) {
		// Unsupported by now
	}

	virtual void printDebug(const char *debugMessage) {
#ifdef ENABLE_DEBUG_OUTPUT
		std::cout << debugMessage << std::endl;
#endif
	}

private:
	Driver::Client &client;
};

STDAPI_(DWORD) modMessage(DWORD uDeviceID, DWORD uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
	Driver &driver = drivers[uDeviceID];
	switch (uMsg) {
	case MODM_OPEN: {
		if (hwnd == NULL) {
			hwnd = FindWindow(L"mt32emu_class", NULL);
		}
		DWORD instance;
		if (hwnd == NULL) {
			// Synth application not found
			if (!synthOpened) {
				if (midiSynth.Init() != 0) return MMSYSERR_ERROR;
				synthOpened = true;
			}
			instance = NULL;
		} else {
			if (synthOpened) {
				midiSynth.Close();
				synthOpened = false;
			}
			updateNanoCounter();
			DWORD msg[70] = {0, (DWORD)-1, 1, nanoCounter.LowPart, (DWORD)nanoCounter.HighPart}; // 0, handshake indicator, version, timestamp, .exe filename of calling application
			GetModuleFileNameA(GetModuleHandle(NULL), (char *)&msg[5], 255);
			COPYDATASTRUCT cds = {0, sizeof(msg), msg};
			instance = (DWORD)SendMessage(hwnd, WM_COPYDATA, NULL, (LPARAM)&cds);
		}
		DWORD res = OpenDriver(driver, uDeviceID, uMsg, dwUser, dwParam1, dwParam2);
		Driver::Client &client = driver.clients[*(LONG *)dwUser];
		client.synth_instance = instance;
		client.midiStreamParser = new MidiStreamParserImpl(client);
		return res;
	}

	case MODM_CLOSE:
		if (driver.clients[dwUser].allocated == false) {
			return MMSYSERR_ERROR;
		}
		if (hwnd == NULL) {
			if (synthOpened) midiSynth.Reset();
		} else {
			SendMessage(hwnd, WM_APP, driver.clients[dwUser].synth_instance, NULL); // end of session message
		}
		delete driver.clients[dwUser].midiStreamParser;
		return CloseDriver(driver, uDeviceID, uMsg, dwUser, dwParam1, dwParam2);

	case MODM_PREPARE:
		return MMSYSERR_NOTSUPPORTED;

	case MODM_UNPREPARE:
		return MMSYSERR_NOTSUPPORTED;

	case MODM_GETDEVCAPS:
		return modGetCaps((PVOID)dwParam1, (DWORD)dwParam2);

	case MODM_DATA: {
		if (driver.clients[dwUser].allocated == false) {
			return MMSYSERR_ERROR;
		}
		driver.clients[dwUser].midiStreamParser->processShortMessage((Bit32u)dwParam1);
		if ((hwnd == NULL) && (synthOpened == false))
			return MMSYSERR_ERROR;
		return MMSYSERR_NOERROR;
	}

	case MODM_LONGDATA: {
		if (driver.clients[dwUser].allocated == false) {
			return MMSYSERR_ERROR;
		}
		MIDIHDR *midiHdr = (MIDIHDR *)dwParam1;
		if ((midiHdr->dwFlags & MHDR_PREPARED) == 0) {
			return MIDIERR_UNPREPARED;
		}
		driver.clients[dwUser].midiStreamParser->parseStream((const Bit8u *)midiHdr->lpData, midiHdr->dwBufferLength);
		if ((hwnd == NULL) && (synthOpened == false))
			return MMSYSERR_ERROR;
		midiHdr->dwFlags |= MHDR_DONE;
		midiHdr->dwFlags &= ~MHDR_INQUEUE;
		DoCallback(uDeviceID, dwUser, MOM_DONE, dwParam1, NULL);
 		return MMSYSERR_NOERROR;
	}

	case MODM_GETNUMDEVS:
		return 0x1;

	default:
		return MMSYSERR_NOERROR;
		break;
	}
}

} // namespace
