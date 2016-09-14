/*
OTP verification code by AuroraWright, and taken from SafeA9LHInstaller
https://github.com/AuroraWright/SafeA9LHInstaller
*/

#include "otp.h"
#include "fatfs/ff.h"
#include "utils.h"
#include "screen.h"
#include <string.h>
#include "crypto.h"
#include "sdmmc.h"
#include "memory.h"
#include "buttons.h"
#include "godmode.h"

#define SECTOR_OFFSET     0x24100000
#define FIRM0_OFFSET      0x24200000
#define FIRM0_SIZE        0xF3000

#define MAX_STAGE1_SIZE   0x1E70
#define MAX_STAGE2_SIZE   0x89A00

#define STAGE1_POSITION   0xF0590

#define STAGE1_OFFSET     FIRM0_OFFSET + STAGE1_POSITION
#define STAGE2_OFFSET     0x24400000

#define OTP_FROM_MEM      0x10012000

static const u8 firm0Hash[0x20] = {
    0x6E, 0x4D, 0x14, 0xAD, 0x51, 0x50, 0xA5, 0x9A, 0x87, 0x59, 0x62, 0xB7, 0x09, 0x0A, 0x3C, 0x74,
    0x4F, 0x72, 0x4B, 0xBD, 0x97, 0x39, 0x33, 0xF2, 0x11, 0xC9, 0x35, 0x22, 0xC8, 0xBB, 0x1C, 0x7D
};

static const u8 firm0A9lhHash[0x20] = {
    0x79, 0x3D, 0x35, 0x7B, 0x8F, 0xF1, 0xFC, 0xF0, 0x8F, 0xB6, 0xDB, 0x51, 0x31, 0xD4, 0xA7, 0x74,
    0x8E, 0xF0, 0x4A, 0xB1, 0xA6, 0x7F, 0xCD, 0xAB, 0x0C, 0x0A, 0xC0, 0x69, 0xA7, 0x9D, 0xC5, 0x04
};

#define SECTION2_POSITION 0x66A00

bool OTPChecked = false;
bool validOTPFound = false;

u32 fileRead(void *dest, const char *path)
{
// 	FATFS otpFS;
// 	f_mount(&otpFS, "0:", 0);

    FIL file;
    u32 size;

    if(f_open(&file, path, FA_READ) == FR_OK)
    {
        unsigned int read;
        size = f_size(&file);
        f_read(&file, dest, size, &read);
        f_close(&file);
    }
    else {
    	size = 0;
    }

// 	f_mount(NULL, "0:", 0);
    return size;
}

u32 checkOTPMatch() {
	u32 i;
	for(i = 0; i < 3; i++) {
		//Break if a match was found
		if(memcmp((void *)(SECTOR_OFFSET + 0x10), key2s[i], 0x10) == 0) break;
	}
	
	return i;
}

bool otpIsValid(char * path, OTPLocation location) {
	OTPChecked = true;
	validOTPFound = true;

	/*
	Read the OTP into memory
	*/
	if (location == OTP_LOCATION_DISK) {
		if(fileRead((void *)OTP_OFFSET, path) != 256) {		
			validOTPFound = false;
			return false;
		}
	}
	
	/*
	Set up crypto stuff for OTP verification
	*/
	
	setupKeyslot0x11((location == OTP_LOCATION_MEMORY), (void *)OTP_OFFSET);
	getNandCTR();
	
	/*
	Read and decrypt from the FIRM0 partition on NAND
	*/
	readFirm0((u8 *)FIRM0_OFFSET, FIRM0_SIZE);
    if(memcmp((void *)FIRM0_OFFSET, "FIRM", 4) != 0) {
    	validOTPFound = false;
    	return false;
    }

	/*
	Read and decrypt the NAND key sector
	*/
	getSector((u8 *)SECTOR_OFFSET);

	/*
	Should be good to go by this point if using the sha256 from RAM
	*/
	if (location == OTP_LOCATION_MEMORY) {
		return true;
	}

	/*
	Check if OTP matches
	*/
	u32 i = checkOTPMatch();

	/*
	No match found - OTP is invalid
	*/
	if(i == 3) {
		validOTPFound = false;
		return false;
	}
	
	return validOTPFound;
}

void sa9lhi(bool allowExit) {
	/*
	If OTP needs to be checked (this should always be the case unless SA9LHI was run
	using OTP bypass)
	*/
	if (!OTPChecked) {
		/*
		If the user is allowed to back out from SafeA9LHInstaller then this means that
		it was run from the options menu. Therefore, read the sha256 from RAM. We can
		do this safely because the only way to enter the options menu is if the user
		entered a correct PIN, or if they used OTP bypass. We therefore don't care
		whether or not they have the OTP on disk
		*/
		if (allowExit) {
			otpIsValid(NULL, OTP_LOCATION_MEMORY);
		}
		/*
		If the user is NOT allowed to back out from SafeA9LHInstaller then this means
		that it was run in a rare situation in which godmode could not be achieved.
		In this situation, the user cannot verify their PIN or use OTP bypass. For
		this reason, read the sha256 from disk to make sure the user actually has a
		valid OTP.
		*/
		else {
			otpIsValid("OTP.BIN", OTP_LOCATION_DISK);
		}
	}
	
	/*
	Die if no valid OTP was found
	*/
	if (!validOTPFound) {
		error("Can't run SafeA9LHInstaller as no valid OTP\nfound. Ensure otp.bin is on your SD card.", !allowExit);
		return;
	}

	u32 updatea9lh = 0;
	u32 i = checkOTPMatch();
	if(i == 1) updatea9lh = 1;
	
	
	int posY;
	bool validOption = false;
	u32 pressed;
	
	while (!validOption) {
		drawString("SafeA9LHInstaller v2.0.4", 10, 10, COLOR_TITLE);
		posY = drawString("Thanks to delebile, #cakey and StandardBus", 10, 40, COLOR_WHITE);
		posY = drawString("Press SELECT to update A9LH", 10, posY + SPACING_Y, COLOR_WHITE);
		if (allowExit) {
			posY += SPACING_Y;
			posY = drawString("Press B to cancel", 10, posY, COLOR_WHITE);
		}
		
		drawString("SafeA9LHInstaller by AuroraWright", 10, 200, COLOR_WHITE);
		drawString("http://goo.gl/EPfoS5", 10, 210, COLOR_WHITE);
		
		pressed = waitInput();
		if (pressed == BUTTON_SELECT || (allowExit && pressed == BUTTON_B)) {
			validOption = true;
		}
	}

    
    if(pressed == BUTTON_SELECT) {

		const char *path;
	
		if(updatea9lh)
		{
			//Generate and encrypt a per-console A9LH key sector
			generateSector((u8 *)SECTOR_OFFSET, 0);

			//Read FIRM0
			path = "a9lh/firm0.bin";
			if(fileRead((void *)FIRM0_OFFSET, path) != FIRM0_SIZE)
				error("firm0.bin doesn't exist or has a wrong size", !allowExit);

			if(!verifyHash((void *)FIRM0_OFFSET, FIRM0_SIZE, firm0Hash))
				error("firm0.bin is invalid or corrupted", !allowExit);
		}
		else if(!verifyHash((void *)FIRM0_OFFSET, SECTION2_POSITION, firm0A9lhHash))
			error("NAND FIRM0 is invalid", !allowExit);


		//Inject stage1
		memset32((void *)STAGE1_OFFSET, 0, MAX_STAGE1_SIZE);
		path = "a9lh/payload_stage1.bin";
		u32 size = fileRead((void *)STAGE1_OFFSET, path);
		if(!size || size > MAX_STAGE1_SIZE)
			error("payload_stage1.bin doesn't exist or\nexceeds max size", !allowExit);

		const u8 zeroes[688] = {0};
		if(memcmp(zeroes, (void *)STAGE1_OFFSET, 688) == 0)
			error("The payload_stage1.bin you're attempting\nto install is not compatible", !allowExit);

		//Read stage2
		memset32((void *)STAGE2_OFFSET, 0, MAX_STAGE2_SIZE);
		path = "a9lh/payload_stage2.bin";
		size = fileRead((void *)STAGE2_OFFSET, path);
		if(!size || size > MAX_STAGE2_SIZE)
			error("payload_stage2.bin doesn't exist or\nexceeds max size", !allowExit);

		posY = drawString("All checks passed, installing...", 10, posY + SPACING_Y, COLOR_WHITE);

		//Point of no return, install stuff in the safest order
		sdmmc_nand_writesectors(0x5C000, MAX_STAGE2_SIZE / 0x200, (vu8 *)STAGE2_OFFSET);
		if(updatea9lh) sdmmc_nand_writesectors(0x96, 1, (vu8 *)SECTOR_OFFSET);
		writeFirm((u8 *)FIRM0_OFFSET, 0, FIRM0_SIZE);

		posY = drawString("Success! Press any key to shut down", 10, posY + SPACING_Y, COLOR_WHITE);
		
		waitInput();
		mcuShutDown();
	}
	else if (pressed == BUTTON_B && allowExit) {
		return;
	}
}