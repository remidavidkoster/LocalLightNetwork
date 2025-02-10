#include "NRF24L01P.h"
#include "stm32g4xx_hal.h"
#include "stdbool.h"
#include "main.h"

uint8_t mirf_ADDR_LEN = 5;

// In sending mode.
uint8_t PTX;

// Channel 0 - 127 or 0 - 84 in the US.
uint8_t channel;

// Payload width in uint8_ts. default 16 max 32.
uint8_t payloadSize;


uint8_t softSpiTransfer(uint8_t shOut) {
	uint8_t shIn = 0;
	for (int i = 0; i < 8; i++) {
		// Data high / low
		if (shOut > 127) NRF_MOSI_GPIO_Port->BSRR = NRF_MOSI_Pin;
		else NRF_MOSI_GPIO_Port->BSRR = NRF_MOSI_Pin << 16;

		shIn += ((NRF_MISO_GPIO_Port->IDR & NRF_MISO_Pin) != (uint32_t)GPIO_PIN_RESET);

		// Clock high / low
		NRF_CLK_GPIO_Port->BSRR = NRF_CLK_Pin;
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");

		NRF_CLK_GPIO_Port->BSRR = NRF_CLK_Pin << 16;
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");
		asm("nop");


		shIn <<= (i != 7);
		shOut <<= 1;
	}
	return shIn;
}


void transferSync(uint8_t *dataout, uint8_t *datain, uint8_t len) {
	uint8_t i;
	for (i = 0; i < len; i++) {
		datain[i] = softSpiTransfer(dataout[i]);
	}
}

void transmitSync(uint8_t *dataout, uint8_t len) {
	uint8_t i;
	for (i = 0; i < len; i++) {
		softSpiTransfer(dataout[i]);
	}
}

// Initializes pins to communicate with the MiRF module
// Should be called in the early initializing phase at startup.
void init() {
	ceLow();
	csnHigh();
}

// Sets the important registers in the MiRF module and powers the module
// in receiving mode
// NB: channel and payload must be set now.
void config(uint8_t dynamicPayload) {
	// Set RF channel
	configRegister(RF_CH, channel);

	// Set length of incoming payload
	configRegister(RX_PW_P0, dynamicPayload ? 32 : payloadSize);
	configRegister(RX_PW_P1, dynamicPayload ? 32 : payloadSize);

	if (dynamicPayload) enableDynamicPayload();

	configRegister(EN_AA, 0);
	configRegister(SETUP_RETR, 0);

	// configRegister(RF_SETUP, 0B00100110);
	// Start receiver
	powerUpRx();
	flushRx();
}



void enableDynamicPayload(){
	csnLow();
	softSpiTransfer(W_REGISTER | (REGISTER_MASK & DYNPD));
	softSpiTransfer(0b00111111);
	csnHigh();

	csnLow();
	softSpiTransfer(W_REGISTER | (REGISTER_MASK & FEATURE));
	softSpiTransfer(0b00000101);
	csnHigh();
}

uint8_t getPayloadLength() {
	csnLow();
	softSpiTransfer(R_RX_PL_WID);  // Request payload width
	uint8_t length = softSpiTransfer(0xFF);  // Read response
	csnHigh();


	// Ensure payload length is valid (1-32 bytes)
	if (length > 32) {
		flushRx();  // Flush the buffer if corrupt data is detected
		return 0;
	}

	return length;
}


// Sets the receiving address
void setAddressWidth(uint8_t width){

	mirf_ADDR_LEN = width;
	ceLow();

	configRegister(SETUP_AW, width - 2);

	ceHigh();
}


// Sets the receiving address
void setRADDR(uint8_t * adr) {
	ceLow();
	writeRegister(RX_ADDR_P1, adr, mirf_ADDR_LEN);
	ceHigh();
}

// Sets the transmitting address
void setTADDR(uint8_t * adr) {
	// RX_ADDR_P0 must be set to the sending addr for auto ack to work.

	writeRegister(RX_ADDR_P0, adr, mirf_ADDR_LEN);
	writeRegister(TX_ADDR, adr, mirf_ADDR_LEN);
}

// Checks if data is available for reading
bool dataReady() {

	// See note in getData() function - just checking RX_DR isn't good enough
	uint8_t status = getStatus();

	// We can short circuit on RX_DR, but if it's not set, we still need
	// to check the FIFO for any pending packets
	if (status & (1 << RX_DR)) return 1;
	return !rxFifoEmpty();
}

bool rxFifoEmpty() {
	uint8_t fifoStatus = 0;

	readRegister(FIFO_STATUS, &fifoStatus, sizeof(fifoStatus));
	return (fifoStatus & (1 << RX_EMPTY));
}



void getData(uint8_t * data) {
	// Reads payload uint8_ts into data array

	uint8_t length = getPayloadLength();

	if (!length) return;

	csnLow();                               // Pull down chip select
	softSpiTransfer(R_RX_PAYLOAD);            // Send cmd to read rx payload
	transferSync(data, data, length); // Read payload
	csnHigh();                               // Pull up chip select
	// NVI: per product spec, p 67, note c:
	// "The RX_DR IRQ is asserted by a new packet arrival event. The procedure
	// for handling this interrupt should be: 1) read payload through SPI,
	// 2) clear RX_DR IRQ, 3) read FIFO_STATUS to check if there are more
	// payloads available in RX FIFO, 4) if there are more data in RX FIFO,
	// repeat from step 1)."
	// So if we're going to clear RX_DR here, we need to check the RX FIFO
	// in the dataReady() function
	configRegister(STATUS, (1 << RX_DR)); // Reset status register
}

// Clocks only one uint8_t into the given MiRF register
void configRegister(uint8_t reg, uint8_t value) {
	csnLow();
	softSpiTransfer(W_REGISTER | (REGISTER_MASK & reg));
	softSpiTransfer(value);
	csnHigh();
}


// Reads an array of uint8_ts from the given start position in the MiRF registers.
void readRegister(uint8_t reg, uint8_t * value, uint8_t len) {
	csnLow();
	softSpiTransfer(R_REGISTER | (REGISTER_MASK & reg));
	transferSync(value, value, len);
	csnHigh();
}

// Writes an array of uint8_ts into inte the MiRF registers.
void writeRegister(uint8_t reg, uint8_t * value, uint8_t len) {
	csnLow();
	softSpiTransfer(W_REGISTER | (REGISTER_MASK & reg));
	transmitSync(value, len);
	csnHigh();
}


// Sends a data package to the default address. Be sure to send the correct
// amount of uint8_ts as configured as payload on the receiver.
void send(uint8_t * value, uint8_t length) {
	uint8_t status;

	while (PTX) {
		status = getStatus();

		if ((status & ((1 << TX_DS)  | (1 << MAX_RT)))) {
			PTX = 0;
			break;
		}
	}                  // Wait until last paket is send

	ceLow();

	powerUpTx();                   // Set to transmitter mode , Power up

	csnLow();                      // Pull down chip select
	softSpiTransfer(FLUSH_TX);     // Write cmd to flush tx fifo
	csnHigh();                     // Pull up chip select

	csnLow();                      // Pull down chip select
	softSpiTransfer(W_TX_PAYLOAD); // Write cmd to write payload
	transmitSync(value, length); // Write payload
	csnHigh();                     // Pull up chip select

	ceHigh();                      // Start transmission
}

// Test if chip is still sending.
// When sending has finished return chip to listening.
bool isSending() {
	uint8_t status;
	if (PTX) {
		status = getStatus();

		// If sending successful (TX_DS) or max retries exceded (MAX_RT).
		if ((status & ((1 << TX_DS)  | (1 << MAX_RT)))) {
			powerUpRx();
			return false;
		}
		return true;
	}
	return false;
}

uint8_t getStatus() {
	uint8_t rv = 0;
	readRegister(STATUS, &rv, 1);
	return rv;
}

void powerUpRx() {
	PTX = 0;
	ceLow();
	configRegister(CONFIG, mirf_CONFIG | ((1 << PWR_UP) | (1 << PRIM_RX)));
	ceHigh();
	configRegister(STATUS, (1 << TX_DS) | (1 << MAX_RT));
}

void flushRx() {
	csnLow();
	softSpiTransfer(FLUSH_RX);
	csnHigh();
}

void powerUpTx() {
	PTX = 1;
	configRegister(CONFIG, mirf_CONFIG | ((1 << PWR_UP) | (0 << PRIM_RX)));
}

void ceHigh() {
	HAL_GPIO_WritePin(NRF_CE_GPIO_Port, NRF_CE_Pin, GPIO_PIN_SET);
}

void ceLow() {
	HAL_GPIO_WritePin(NRF_CE_GPIO_Port, NRF_CE_Pin, GPIO_PIN_RESET);
}

void csnHigh() {
	HAL_GPIO_WritePin(NRF_SS_GPIO_Port, NRF_SS_Pin, GPIO_PIN_SET);
}

void csnLow() {
	HAL_GPIO_WritePin(NRF_SS_GPIO_Port, NRF_SS_Pin, GPIO_PIN_RESET);
}

void clkHigh() {
	HAL_GPIO_WritePin(NRF_CLK_GPIO_Port, NRF_CLK_Pin, GPIO_PIN_SET);
}

void clkLow() {
	HAL_GPIO_WritePin(NRF_CLK_GPIO_Port, NRF_CLK_Pin, GPIO_PIN_RESET);
}


void outHigh() {
	HAL_GPIO_WritePin(NRF_MOSI_GPIO_Port, NRF_MOSI_Pin, GPIO_PIN_SET);
}

void outLow() {
	HAL_GPIO_WritePin(NRF_MOSI_GPIO_Port, NRF_MOSI_Pin, GPIO_PIN_RESET);
}

uint8_t readInPin() {
	return HAL_GPIO_ReadPin(NRF_MISO_GPIO_Port, NRF_MISO_Pin);
}

void powerDown() {
	ceLow();
	configRegister(CONFIG, mirf_CONFIG);
}

void setDataStuff(uint8_t dataStuff) {
	configRegister(RF_SETUP, dataStuff);
}
