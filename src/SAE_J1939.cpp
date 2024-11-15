#include <SAE_J1939.h>


byte nameBuff[8];		// Global name buffer for when people want to grab the 8 bytes from the CAName class.


// Electronic control unit.						
ECU::ECU(byte inECUInst)
	: linkList(), idler() {
	
	ECUInst = inECUInst;
}


ECU::~ECU(void) {  }

		
byte ECU::getECUInst(void) { return ECUInst; }


void ECU::setECUInst(byte inInst) { ECUInst = inInst; }


// Decoding the 29 bit CAN header.
void ECU::readHeader(uint32_t CANID, msgHeader* inHeader) {
  
  uint32_t buffer = CANID;
  
  inHeader->sourceAddr	= buffer & 0xFF;
  buffer						= buffer >> 8;
  inHeader->PGN			= buffer & 0x3FFFF;
  inHeader->PDUs			= buffer & 0xFF;
  buffer						= buffer >> 8;
  inHeader->PDUf			= buffer & 0xFF;
  buffer						= buffer >> 8;
  inHeader->DP				= buffer & 0x01;
  buffer						= buffer >> 1;
  inHeader->R				= buffer & 0x01;
  buffer						= buffer >> 1;
  inHeader->priority		= buffer &0x07;
}


uint32_t ECU::makeHeader(uint32_t PGN, uint8_t priority, uint8_t sourceAddr) {

	return ((PGN << 8) | priority << 26) | sourceAddr;
}
 

// First thing is to check for and handle incoming messages.
// Next is to see if any CA's need to output messages of their own.
void ECU::idle(void) {

	CA*			trace;

   handlePacket();							// If polling, and that's what's up now. We see if a message has arrived.
	trace = (CA*)getFirst();				// Well start at the beginning and let 'em all have a go.
	while(trace) {								// While we got something..
		trace->idleTime();					// Give 'em some time to do things.
		trace = (CA*)trace->getNext();	// Grab the next one.
	}
}




/*
 Addressing categories for CA's. Choose one.
enum adderCat {

	nonConfig,			// Address is hard coded.
	serviceConfig,		// You can hook up equipment to change our address.
	commandConfig,		// We can respond to address change messages.
	selfConfig,			// We set our own depending on how the network is set up.
	arbitraryConfig,	// We can do the arbitrary addressing dance.
	noAddress			// We have no address, just a listener, or broadcaster.
};
*/

//  -----------  CAName class  -----------


// Packed eight byte set of goodies.
CAName::CAName(void) {  }

CAName::~CAName(void) {  }


// Just in case you need to clear it out.
void CAName::clearName(void) {

	for(int i=0;i<7;i++) {
		name[0] = 0;
	}
}


// Byte 8
// 1 bit - True, we CAN change our address. 128..247	
// False, we can't change our own address.
bool CAName::getArbitratyAddrBit(void) { return name[7] >> 7; }    


void CAName::setArbitratyAddrBit(bool AABit) { 
 
	name[7] &= ~(1<<7);								//Clear bit
	name[7] = name[7] | ((AABit & 0x1)<<7);
}


// 3 bit - Assigned by committee. Tractor, car, boat..
byte CAName::getIndGroup(void) { return (name[7]>>4 & 0x7); }

void CAName::setIndGroup(byte indGroup) {
	
	name[7] &= ~(0x7<<4);								//Clear bits
	name[7] = name[7] | ((indGroup & 0x7)<<4);
}


// 4 bit - System instance, like engine1 or engine2.
byte CAName::getSystemInst(void) { return name[7] & 0xF; }

void CAName::setSystemInst(byte sysInst) {
	
	name[7] &= ~(0xF);							//Clear bits
	name[7] = name[7] | (sysInst & 0xF);
}


//Byte7
// 7 bit - Assigned by committee.
byte CAName::getVehSys(void) { return (name[6] >> 1); }

void CAName::setVehSys(byte vehSys) {

	name[6] = 0;								//Clear bits
	name[6] = name[6] | (vehSys << 1);
}


//Byte6
// 8 bit - Assigned by committee. 0..127 absolute definition.
// 128+ need other fields for definition.
byte CAName::getFunction(void) { return name[5]; }

void CAName::setFunction(byte funct) { name[5] = funct; }


//Byte5
// 5 bit - Instance of this function. (Fuel level?)
byte CAName::getFunctInst(void) { return name[4] >> 3; }

void CAName::setFunctInst(byte functInst) {

	name[4] &= ~(0x1F << 3);								//Clear bits
	name[4] = name[4] | ((functInst & 0x1F) << 3);
}


// 3 bit - What processor instance are we?
byte CAName::getECUInst(void) { return name[4] & 0x7; }

void CAName::setECUInst(byte inst) {

	name[4] &= ~(0x7);
	name[4] = name[4] | (inst & 0x7);
}


//Byte4/3
// 11 bit - Assigned by committee. Who made this thing?
uint16_t CAName::getManufCode(void) { return name[3]<<3 | (name[2] >> 5);  }

void CAName::setManufCode(uint16_t manfCode) {

	name[2] &= ~(0x7<<5); //Clear bits byte 3
	name[3] = 0; //Clear bits byte 4
	name[2] = name[2] | (manfCode<<3 & 0xE0);
	name[3] = manfCode>>3;
}


//Byte3/2/1
// 21 bit - Unique Fixed value. Product ID & Serial number kinda' thing.
uint32_t CAName::getID(void) { return (name[2] & 0x1F) << 16 | (name[1] << 8) | name[0]; }

void CAName::setID(uint32_t value) {

	name[0] = value & 0xFF;
	name[1] = (value >> 8) & 0xFF;
	name[2] &= ~(0x1F); //Clear bits
	name[2] = name[2] | ((value >> 16) & 0x1F);
}


// 64 bit - Pass back the packed up 64 bits that this makes up as our name.
byte* CAName::getName(void) {						
	for (int i=0;i<7;i++) {
		nameBuff[i] = name[i];
	}
	return nameBuff;
}


// If we want to decode one?
void CAName::setName(byte* namePtr) {
	
	for (int i=0;i<7;i++) {
		name[i] = namePtr[i];
	}
}
	
			
		
//  -----------  CA class  -----------


CA::CA(ECU* inECU)
	: linkListObj(), CAName() {
	
	ourECU		= inECU;					// Pointer back to our "boss".
	ourPGN		= 0;						// Default to zero.
	ourAddrCat	= nonConfig;			// These three are just defaults.
	defAddress	= 0;						// Whom ever we end up "in", will set these up.
	address		= NULL_ADDR;			// 'Cause we don't have one?
   dataBytes	= NULL;					// So we can use resizeBuff().
   setNumBytes(DEF_NUM_DATA_BYTES);	// Set the default size.
   intervaTimer.reset();				// Default to off.
}


// Basically we recycle the buffer.
CA::~CA(void) { resizeBuff(0,&dataBytes); }


int CA::getNumBytes(void) { return numBytes; }


void CA::setNumBytes(int inNumBytes) {

	if (resizeBuff(inNumBytes,&dataBytes)) {
      numBytes = inNumBytes;
   } else {
   	numBytes = 0;
   }
}

				
// How we deal with addressing.
adderCat CA::getAddrCat(void) { return ourAddrCat; }


void CA::setAddrCat(adderCat inAddrCat) { ourAddrCat = inAddrCat; }


//	nonConfig
void CA::setNonConfigAddr(byte inAddr) {

	address = inAddr;
	setAddrCat(nonConfig);
}

	

// serviceConfig
// commandConfig
// selfConfig
byte CA::getAddr(void) { return address; }



void CA::setAddr(byte inAddr) { address = inAddr; }
	


byte CA::getDefAddr(void) { return defAddress; }



void CA::setDefAddr(byte inAddr) {

	defAddress = inAddr;
}


// arbitraryConfig
void CA::requestForAddressClaim(void) {
	
	//uint32_t addr;
	
	//addr = ourECU->makeHeader(REQ_MESSAGE,6,address);
	//sendMessage();

}


void CA::addressClaimed(void) {

}


void CA::cannotClaimAddress(void) {

}


void CA::commandedAddress(void) {

}


void CA::setSendInterval(float inMs) {

   if (inMs>0) {
      intervaTimer.setTime(inMs);
   } else {
      intervaTimer.reset();
   }
}
 

float CA::getSendInterval(void) {  return intervaTimer.getTime(); }
 

// Same as idle, but called by the ECU.	 
void  CA::idleTime(void) {

   if (intervaTimer.ding()) {
      sendMessage();
      intervaTimer.stepTime();
   }
}



		
				