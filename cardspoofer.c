#define F_CPU 16000000UL

#include <avr/io.h> 
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
//#include <avr/eeprom.h> //TODO
//#include <avr/sleep.h> //TODO

#define enc_pina 1 //PC1/PCINT8
#define enc_pinb 0 //PC0/PCINT9
#define encButtonPin 2 //PB2
#define coil_pina 0 //PB0
#define coil_pinb 1 //PB1
#define en_pin 5 //PB5
#define lcd_rsPin 3 //PC3
#define lcd_rwPin 4 //PC4
#define lcd_ePin 5 //PC5

int clockSpeed = 500;
int clockHalf = 0;
//unsigned long long tick = 0;
long long encDis = 0;
long long encDisBig = 0;
long long encDis_prev = 0;
int enc_prevstate = 0;
int startButton = 1;
int encButton = 1;
int push_rotate = 0;
long long pushRotateDisBig = 0;
long long pushRotateDis = 0;

int columnSums[4];

/*
ISR(TIMER2_OVF_vect){ // timer0 overflow interrupt triggered every ~.016ms
	tick++;
}
*/
ISR(PCINT1_vect){ //encoder interrupt
	int pinState = PINC;
	int enc_A = (pinState >> enc_pina) & 1; //check encoder input a
	int enc_B = (pinState >> enc_pinb) & 1; //check encoder input b
	startButton = (pinState >> 2) & 1;
	encDis_prev = encDisBig;
	//note - inverted
	if ((enc_A == 1)&&(enc_B == 1)){//		0,0 - state 1
		if (enc_prevstate == 4){
			encDisBig++;
		}else if (enc_prevstate == 2){
			encDisBig--;
		}
		enc_prevstate = 1;
	}else if ((enc_A == 0)&&(enc_B == 1)){//	1,0 - state 2
		if (enc_prevstate == 1){
			encDisBig++;
		}else if (enc_prevstate == 3){
			encDisBig--;
		}
		enc_prevstate = 2;
	}else if ((enc_A == 0)&&(enc_B == 0)){//	1,1 - state 3
		if (enc_prevstate == 2){
			encDisBig++;
		}else if (enc_prevstate == 4){
			encDisBig--;
		}
		enc_prevstate = 3;
	}else if ((enc_A == 1)&&(enc_B == 0)){//	0,1 - state 4
		if (enc_prevstate == 3){
			encDisBig++;
		}else if (enc_prevstate == 1){
			encDisBig--;
		}
		enc_prevstate = 4;
	}
	if (((PINB >> encButtonPin) & 1) == 0){
		push_rotate = 1;
		pushRotateDisBig = pushRotateDis + (encDisBig - encDis_prev);
	}
	encDis = (encDisBig-pushRotateDisBig)/4;
	pushRotateDis = pushRotateDisBig/4;
}

/*
void delayMicroseconds(unsigned long long us){
	unsigned long long beginTime = tick;
	unsigned long long ticks = us/16;
	while(tick-beginTime < ticks);
}

void delay(unsigned long long ms){
	unsigned long long beginTime = tick;
	unsigned long long ticks = ms*125;
	while(tick-beginTime < ticks/2);
	//1/0.016 = 62.5 = 125/2
}
*/
void lcd_command(char i){
	PORTD = i; //put data on output Port
	PORTC &= ~(1 << lcd_rsPin); //D/I=LOW : send instruction
	PORTC &= ~(1 << lcd_rwPin); //R/W=LOW : Write
	PORTC |= 1 << lcd_ePin;
	_delay_ms(1); //enable pulse width >= 300ns
	PORTC &= ~(1 << lcd_ePin); //Clock enable: falling edge
}

void lcd_write(char i){
	PORTD = i; //put data on output Port
	PORTC |= 1 << lcd_rsPin; //D/I=HIGH : send data
	PORTC &= ~(1 << lcd_rwPin); //R/W=LOW : Write
	PORTC |= 1 << lcd_ePin;
	_delay_ms(1); //enable pulse width >= 300ns
	PORTC &= ~(1 << lcd_ePin); //Clock enable: falling edge
}

void lcd_init(){
	PORTC &= ~(1 << lcd_ePin);
	_delay_ms(45); //Wait >40 msec after power is applied
	lcd_command(0x30); //lcd_command 0x30 = Wake up
	_delay_us(5); //must wait 5ms, busy flag not available
	lcd_command(0x30); //lcd_command 0x30 = Wake up #2
	_delay_us(160); //must wait 160us, busy flag not available
	lcd_command(0x30); //lcd_command 0x30 = Wake up #3
	_delay_us(160); //must wait 160us, busy flag not available
	lcd_command(0x38); //Function set: 8-bit/2-line
	lcd_command(0x10); //Set cursor
	lcd_command(0x0c); //Display ON; Cursor ON
	lcd_command(0x06); //Entry mode set
}

void writeBit(int lowOrHigh){
	if(lowOrHigh == 1){
		if(clockHalf == 0){
			PORTB &= ~(1 << coil_pinb);
			PORTB |= 1 << coil_pina;
			_delay_us(clockSpeed);
			PORTB &= ~(1 << coil_pina);
			PORTB |= 1 << coil_pinb;
			_delay_us(clockSpeed);
			//clockHalf = 1;
		}else{
			PORTB &= ~(1 << coil_pina);
			PORTB |= 1 << coil_pinb;
			_delay_us(clockSpeed);
			PORTB &= ~(1 << coil_pinb);
			PORTB |= 1 << coil_pina;
			_delay_us(clockSpeed);
			//clockHalf = 0;
		}
	}else{
		if(clockHalf == 0){
			PORTB &= ~(1 << coil_pinb);
			PORTB |= 1 << coil_pina;
			_delay_us(clockSpeed * 2);
			clockHalf = 1;
		}else{
			PORTB &= ~(1 << coil_pina);
			PORTB |= 1 << coil_pinb;
			_delay_us(clockSpeed * 2);
			clockHalf = 0;
		}
	}
}

void writeLow(){
	PORTB &= ~(1 << coil_pina) & ~(1 << coil_pinb) & ~(1 << en_pin); //write low to both coil pins and disable drivers;
}

void writeLRC(){
	int lrcBitSum = 0;
	for (int i=0; i<4; i++){
		if (columnSums[i]%2==0) { //If sum is even, bit is 0
			writeBit(0);
		}else{ //If sum is odd, bit is 1
			writeBit(1);
			lrcBitSum += 1;
		}
		columnSums[i]=0; //Reset the sum now that bit has been written
	}
	if (lrcBitSum%2==0){ //If the sum of the LRC bits is even write a 1
		writeBit(1);
	}else{ //If the sum of the LRC bits is odd, write a 0
		writeBit(0);
	}
}

void writeChar(char character){
	switch (character){
		case '0': // 00001
			writeBit(0);
			writeBit(0);
			writeBit(0);
			writeBit(0);
			writeBit(1);		 
			break;
			
		case '1': // 10000
			writeBit(1);
			columnSums[0]+=1;
			writeBit(0);
			writeBit(0);
			writeBit(0);
			writeBit(0);
			break;
			
		case '2': // 01000
			writeBit(0);
			writeBit(1);
			columnSums[1]+=1;
			writeBit(0);
			writeBit(0);
			writeBit(0);
			break;
			
		case '3': // 11001
			writeBit(1);
			columnSums[0]+=1;
			writeBit(1);
			columnSums[1]+=1;
			writeBit(0);
			writeBit(0);
			writeBit(1);
			break;
		case '4': // 00100
			writeBit(0);
			writeBit(0);
			writeBit(1);
			columnSums[2]+=1;
			writeBit(0);
			writeBit(0);
			break;
			
		case '5': // 10101
			writeBit(1);
			columnSums[0]+=1;
			writeBit(0);
			writeBit(1);
			columnSums[2]+=1;
			writeBit(0);
			writeBit(1);
			break;
			
		case '6': // 01101
			writeBit(0);
			writeBit(1);
			columnSums[1]+=1;
			writeBit(1);
			columnSums[2]+=1;
			writeBit(0);
			writeBit(1);
			break;
			
		case '7': // 11100
			writeBit(1);
			columnSums[0]+=1;
			writeBit(1);
			columnSums[1]+=1;
			writeBit(1);
			columnSums[2]+=1;
			writeBit(0);
			writeBit(0);
			break;
			
		case '8': // 00010
			writeBit(0);
			writeBit(0);
			writeBit(0);
			writeBit(1);
			columnSums[3]+=1;
			writeBit(0);
			break;
			
		case '9': // 10011
			writeBit(1);
			columnSums[0]+=1;
			writeBit(0);
			writeBit(0);
			writeBit(1);
			columnSums[3]+=1;
			writeBit(1);
			break;
			
		case ':': // 01011
			writeBit(0);
			writeBit(1);
			columnSums[1]+=1;
			writeBit(0);
			writeBit(1);
			columnSums[3]+=1;
			writeBit(1);
			break;
			
		case ';': // 11010
			writeBit(1);
			columnSums[0]+=1;
			writeBit(1);
			columnSums[1]+=1;
			writeBit(0);
			writeBit(1);
			columnSums[3]+=1;
			writeBit(0);
			break;
			
		case '<': // 00111
			writeBit(0);
			writeBit(0);
			writeBit(1);
			columnSums[2]+=1;
			writeBit(1);
			columnSums[3]+=1;
			writeBit(1);
			break;
			
		case '=': // 10110
			writeBit(1);
			columnSums[0]+=1;
			writeBit(0);
			writeBit(1);
			columnSums[2]+=1;
			writeBit(1);
			columnSums[3]+=1;
			writeBit(0);
			break;
			
		case '>': // 01110
			writeBit(0);
			writeBit(1);
			columnSums[1]+=1;
			writeBit(1);
			columnSums[2]+=1;
			writeBit(1);
			columnSums[3]+=1;
			writeBit(0);
			break;
			
		case '?': // 11111
			writeBit(1);
			columnSums[0]+=1;
			writeBit(1);
			columnSums[1]+=1;
			writeBit(1);
			columnSums[2]+=1;
			writeBit(1);
			columnSums[3]+=1;
			writeBit(1);
			break;
	}
}

class ID{
	public:
		char* id;
		char* name;
		int duplicate;
		
		void writeId(){
			int length = strlen(id);
			for(int i=0;i<8;i++){
				if(i>=length){
					lcd_write(' ');
				}else{
					lcd_write(id[i]);
				}
			}
		}
		void writeName(){
			int length = strlen(name);
			for(int i=0;i<8;i++){
				if(i>=length){
					lcd_write(' ');
				}else{
					lcd_write(name[i]);
				}
			}
		}
		
		void spoof(){
			PORTB |= (1<<en_pin); //enable drivers
			for(int i=0; i<26; i++){
				writeBit(0);
			}
			char duplicateChar = (char)(duplicate%10)+48;
			char *prec = ";000";
			char *succ = "?";
			for(int i=0; i<4; i++){
				writeChar(prec[i]);
			}
			for(int i=0; i<7; i++){
				writeChar(id[i]);
			}
			writeChar(duplicateChar);
			for(int i=0; i<2; i++){
				writeChar(succ[i]);
			}
			writeLRC();
			for(int i=0; i<20; i++){
				writeBit(0);
			}
			writeLow();
			_delay_ms(500);
		}
};

void setup(){
	DDRB = 0b111011;
	DDRD = 0xFF;
	DDRC = 0b111000;
	PORTB &= ~(1<<en_pin); //disable drivers
	
	PCMSK1 |= (1 << PCINT8)|(1 << PCINT9)|(1 << PCINT10); //set interrupts on PCINT8 (PC0) and PCINT9 (PC1) for encoder on channel 1, and PCINT10(PC2) for button	
	PCICR |= (1 << PCIE1);//Enable channel 1 (PCINTs 8 and 9)
	sei();
	/*
	//TCCR2B |= (1<<CS22)|(1<<CS21)|(1<<CS20); //set the timer0 prescaler to 1024 <- overflow triggered every 16ms at 16mhz <- p.164
	TCCR2B |= (1<<CS22)|(1<<CS21); TCCR2B &= ~(1<<CS20); //prescaler to 256
	// 1 / ( (16000000/1024) / (2^8) ) * 1000 = 16.384ms
	//         ^hertz  ^prescale ^bits = 61.03515625 hertz
	// 1 / ( (16000000/256) / (2^8) ) * 1000 = 0.016ms
	
	TIMSK2 |= (1<<TOIE2); //enable overflow interrupts for timer0
	TCNT2 = 0; //reset and init counter
	*/
	lcd_init();
}

char numToChar(unsigned int n){
	return (char)(((int)'0')+n%10);
}

void interface(){
	//bob 2046637
	ID andrew;
	andrew.id="1867016";
	andrew.name="Andrew";
	andrew.duplicate = 1;
	
	ID ez;
	ez.id="1855865";
	ez.name="EZ";
	ez.duplicate = 3;
	
	ID morgan;
	morgan.id="5831853";
	morgan.name="Morgan";
	morgan.duplicate = 1;
	
	ID bryce;
	bryce.id="1852995";
	bryce.name="Bryce";
	bryce.duplicate = 1;
		
	ID maxh;
	maxh.id="1873138";
	maxh.name="MaxH-R/S";
	maxh.duplicate = 1;
	
	ID justin;
	justin.id="1708913";
	justin.name="Justin";
	justin.duplicate = 1;
	
	ID roxy;
	roxy.id="1867202";
	roxy.name="Roxy-S/H";
	roxy.duplicate = 1;
	
	ID bp;
	bp.id="2046637";
	bp.name="bp";
	bp.duplicate = 1;
	
	ID ethan;
	ethan.id="1859419";
	ethan.name="Ethan";
	ethan.duplicate = 1;
	
	ID lauren;
	lauren.id="1856179";
	lauren.name="Lauren";
	lauren.duplicate = 1;
	
	ID kiona;
	kiona.id="1857660";
	kiona.name="Kiona";
	kiona.duplicate = 1;

	ID idList[] = {ethan, maxh, justin, bp, morgan, lauren, roxy, kiona, andrew, bryce, ez};
	int length=11;
	ID selected;
	
	int prev_encButton = 1;
	
	while(1){
		int selectedIndex;
		int cardNum = pushRotateDis;
		
		if (encDis<=0){
			selectedIndex=(encDis%length)+length-1;
		}else{
			selectedIndex=encDis%length;
		}
		selected = idList[selectedIndex];
		
		lcd_command(0x01); //home
		_delay_us(700);

		
		selected.writeName();
		lcd_command(0xc0); //nextLine
		_delay_us(700);
		selected.writeId();
		encButton = (PINB >> encButtonPin) & 1;
		if ((encButton == 1) && (prev_encButton == 0)){
				if (push_rotate == 1){
					push_rotate = 0;
					pushRotateDis = 0;
				}else{
					//released enc button
				}
		}
		prev_encButton = encButton;
		if(startButton==0){
			lcd_command(0x01); //clear screen
			selected.spoof();
			//test();
		}
		_delay_ms(100);
		
	}
}

int main(){
	setup();
	while(1){
		//char top[]=" No IDs ";
		//char bottom[] = "Entered ";
		//for(int i=0;i<8;i++){
		//	lcd_write(top[i]);
		//}
		//lcd_command(0xc0); //nextLine
		//_delay_ms(1);
		//for(int i=0;i<8;i++){
		//	lcd_write(bottom[i]);
		//}
		interface();
	}
	return 0;
}