#include <LC_llama2000.h>
#include <resizeBuff.h>



int pack16(byte hiByte,int lowByte) {
  
   struct int16 {
      byte lowByte;
      byte hiByte;
   };
   int16*   bytes;
   int      value;

   value = 0;                    // Shut up compiler.
   bytes = (int16*)&value;
   bytes->lowByte = lowByte;
   bytes->hiByte = hiByte;
   return(value);
}

uint32_t pack32(byte hiByte,byte byte2,byte byte1,byte lowByte) {
  
   struct int32 {
      byte byte0;
      byte byte1;
      byte byte2;
      byte byte3;
   };
   int32*   bytes;
   uint32_t value;

   value = 0;                    // Shut up compiler.
   bytes = (int32*)&value;
   bytes->byte3 = hiByte;
   bytes->byte2 = byte2;
   bytes->byte1 = byte1;
   bytes->byte0 = lowByte;
   return(value);
}


// ************ llama_NMEA2000 ************


llama2000::llama2000(byte inECUInst,int inResetPin,int inIntPin)
  : ECU() {

  resetPin = inResetPin;
  intPin   = inIntPin;
}


llama2000::~llama2000(void) {  }


bool llama2000::begin(int inCSPin) {
	ECUname	aName;
	
	aName.setID(6387);										// Device ID. We make these up. You get 21 bits. (2097151 or less)
	aName.setManufCode(0);									// This would be assigned to you by NMEA people.
	aName.setECUInst(0);										// First ECU (Electronic control unit.)
	aName.setFunctInst(0);									// First depth transducer.
	aName.setFunction(DEV_FUNC_GP_TRANS);				// Transducer of some sort.
	aName.setVehSys(DEV_CLASS_INST);						//	We are an instrument.
	aName.setSystemInst(0);									// We are the first of our device class.
	aName.setIndGroup(Marine);								// What kind of machine are we ridin' on?
	ECU::begin(&aName,45,arbitraryConfig);				// Here's our name, default address and address category.													
	pinMode(resetPin, OUTPUT);								// Setup our reset pin.
	delay(50);													// Sit for a bit..
	digitalWrite(resetPin, LOW);							// Set reset low.
	delay(50);													// Set for a bit, again.
	digitalWrite(resetPin, HIGH);							// Flick it high and hold there.
	hookup();													// Hook ourselves into the ideler queue.
	return CAN.begin(500E3);								// Fire up the hardware.
}


void llama2000::sendMsg(message* outMsg) {
	
   uint32_t CANID;
   int		numBytes;
   
   /*
   Serial.println("Sending..");
   outMsg->showMessage();
   Serial.println();
   Serial.println();
   */
   
   if (outMsg) {
		CANID = outMsg->getCANID();
		numBytes = outMsg->getNumBytes();
		CAN.beginExtendedPacket(CANID);
		for (int i=0;i<numBytes;i++) {
			CAN.write(outMsg->getDataByte(i));
		}
		CAN.endPacket();
	}
}


void llama2000::recieveMsg(void) {

	message	newMsg;
	int		i;
	
	if (CAN.parsePacket()) {										// If we got a packet..
		newMsg.setCANID(CAN.packetId());							// Decode the packet and save the info.
		newMsg.setNumBytes(CAN.packetDlc());					// Read and set up the buff size.
		i = 0;															// Starting at zero..
		while (CAN.available()&&i<newMsg.getNumBytes()) {	// While we have a byte to read and a place to put it..
			newMsg.setDataByte(i,CAN.read());					// Read and store the byte into the message.
			i++;															// Bump of the storage index.
		}																	//
		handleMsg(&newMsg);											// All stored, let our ECU deal with it.
	}
}


void llama2000::idle(void) {

	ECU::idle();
	recieveMsg();
}

// ************* msgHandler *************


msgHandler::msgHandler(ECU* inECU)
   : CA(inECU) { }


msgHandler::~msgHandler(void) { }


void msgHandler::sendMsg(message* outMsg) {

	if (ourECU) {
		ourECU->sendMsg(outMsg);
	}
}



// ************* waterSpeedObj *************


waterSpeedObj::waterSpeedObj(ECU* inECU)
   : msgHandler(inECU) {

  knots   = 0;
  speedMap.setValues(0,1023,0,(1023*1.943844)*0.01);
}


waterSpeedObj::~waterSpeedObj(void) {  }


bool waterSpeedObj::handleMsg(message* inMsg) {

  unsigned int rawSpeed;
  
  if (inMsg->getPGN()==0x1F503) {
  	rawSpeed = (unsigned int) pack16(inMsg->getDataByte(2),inMsg->getDataByte(1));
  	knots = speedMap.map(rawSpeed);
  	return true;
  }
  return false;
}

  
float waterSpeedObj::getSpeed(void) { return knots; }



// ************* waterDepthObj *************


waterDepthObj::waterDepthObj(ECU* inECU)
   : msgHandler(inECU) {

  ourPGN  = 0x1F50B;
  feet   = 0;
}


waterDepthObj::~waterDepthObj(void) {  }


bool waterDepthObj::handleMsg(message* inMsg) {

	unsigned int rawDepth;
	if (inMsg->getPGN()==0x1F50B) {
		rawDepth = (unsigned int) pack32(inMsg->getDataByte(4),inMsg->getDataByte(3),inMsg->getDataByte(2),inMsg->getDataByte(1));
		rawDepth = rawDepth / 100.0;		// Give meters.
		feet = rawDepth * 3.28084;			// Give feet
		return true;
	}
	return false;
}

  
float waterDepthObj::getDepth(void) { return feet; }



// ************* waterTempObj *************


waterTempObj::waterTempObj(ECU* inECU)
   : msgHandler(inECU) {

   ourPGN   = 0x1FD08;
   degF     = 0;
}


waterTempObj::~waterTempObj(void) {  }


bool waterTempObj::handleMsg(message* inMsg) {

   unsigned int rawTemp;

	if (inMsg->getPGN()==0x1FD08) {
   	rawTemp  = (unsigned int) pack16(inMsg->getDataByte(4),inMsg->getDataByte(3));
   	degF  = rawTemp / 100.0;         // Give kelvan.
   	degF  = (degF * 1.8) - 459.67;   // Give degF.
   	return true;
   }
   return false;
}

  
float waterTempObj::getTemp(void) { return degF; }


// ************* fluidLevelObj *************


 fluidLevelObj::fluidLevelObj(ECU* inECU) 
   : msgHandler(inECU) {
   
   ourPGN		= 0x1F211;
	fluidType	= fuel;  // This is 0.
   level       = 0;
   setSendInterval(2500);
}
 

fluidLevelObj::~fluidLevelObj(void) {  }

tankType fluidLevelObj::getTankType() { return fluidType; }

void fluidLevelObj::setTankType(tankType inType) { fluidType = inType; }

float fluidLevelObj::getLevel(void) { return level; }

void fluidLevelObj::setLevel(float inLevel) { level = inLevel; }

float fluidLevelObj::getCapacity(void) { return capacity; }

void fluidLevelObj::setCapacity(float inCapacity) { capacity = inCapacity; }


void fluidLevelObj::sendMsg(void) {
   
   int16_t  tempInt;
   int32_t  tempLong;
	message	outMsg;
	byte		aByte;
	
	outMsg.setPGN(ourPGN);
   outMsg.setPriority(6);
   outMsg.setSourceAddr(ourECU->getAddr());
   
   aByte = 0/*instance*/ & 0b00001111;
   aByte = aByte<<4;
   aByte = aByte | ((byte)fluidType & 0b00001111);
   outMsg.setDataByte(0,aByte);
   
   tempInt = round(level*250);
   aByte = tempInt & 0x00FF;
   outMsg.setDataByte(1,aByte);
   
   aByte = (tempInt & 0xFF00)>>8;
   outMsg.setDataByte(2,aByte);
   
   tempLong = round(capacity * 10);
   aByte = tempLong & 0x000000FF;
   outMsg.setDataByte(3,aByte);
   
   aByte = (tempLong & 0x0000FF00)>>8;
   outMsg.setDataByte(4,aByte);
   
   aByte = (tempLong & 0x00FF0000)>>16;
   outMsg.setDataByte(5,aByte);
   
   aByte = (tempLong & 0xFF000000)>>24;
   outMsg.setDataByte(6,aByte);
   
   aByte = 0xFF; 
   outMsg.setDataByte(7,aByte);                           // Reserved, so..
   
   msgHandler::sendMsg(&outMsg);
}


// ************* airTempBarometer *************


airTempBarometer::airTempBarometer(ECU* inECU)
   : msgHandler(inECU) {

   degF	= 0;
   inHg	= 0;
   inHgSmooth = new runningAvg(6);
   if (inHgSmooth) {
   	Serial.println("Got a smoother.");

   } else {
   	Serial.println("No Smoother!");
   }
}


airTempBarometer::~airTempBarometer(void) { if (inHgSmooth) delete inHgSmooth; }


bool airTempBarometer::handleMsg(message* inMsg) {

   uint32_t	rawPa32;
   uint16_t	rawPa16;
   float		Pa;
	bool		success;
	
	success = false;
	if (inMsg->getPGN()==0x1FD0A) {
   	rawPa32  = pack32(inMsg->getDataByte(6),inMsg->getDataByte(5),inMsg->getDataByte(4),inMsg->getDataByte(3));
   	if (!isBlank(rawPa32)) {
   		Pa	= rawPa32 * 0.1;
   		inHg = inHgSmooth->addData(Pa*0.0002953);
   		success = true;
   	}
   } else if (inMsg->getPGN()==0x1FD06) {
   	rawPa16  = pack16(inMsg->getDataByte(6),inMsg->getDataByte(5));
   	if (!isBlank(rawPa16)) {
   		Pa	= rawPa16 * 100;
   		inHg = inHgSmooth->addData(Pa*0.0002953);
   		success = true;
		}
   } else if (inMsg->getPGN()==0x1FD07) {
		rawPa16  =  pack16(inMsg->getDataByte(7),inMsg->getDataByte(6));
   	if (!isBlank(rawPa16)) {
   		Pa	= rawPa16 * 100;
   		inHg = inHgSmooth->addData(Pa*0.0002953);
   		success = true;
		}
   }
   return success;
}

  
float airTempBarometer::getAirTemp(void) { return degF; }

float airTempBarometer::getInHg(void) { return inHg; }

