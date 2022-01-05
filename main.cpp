#include "mbed.h"
#include "TextLCD.h"

I2C rtc(PTC9, PTC8);                                                        //I2C driver za rtc
BusOut status_led(D8,D9,D10);                                               //Statusne ledice
Serial pc(USBTX, USBRX);                                                    //Debug
TextLCD lcd(PTE20, PTE21, PTE22, PTE23, PTE29, PTE30, TextLCD::LCD16x2);    //Driver za LCD -> paralelna komunikacija
InterruptIn tipkalo(D2);                                                    //Prekidno tipkalo za generiranje koda
Serial HC06(PTE0,PTE1);                                                     //Driver za bluetooth komunikaciju
Timeout main_screen;                                                        //Timeout za povratak na početni screen
                                                           
#define RTC_SLAVE_ADR           0x51
#define RTC_SEC_ADR             0x04
#define RTC_DAY_ADR             0x07   

#define LED_RED                (1U << 2)
#define LED_GREEN              (1U << 1)
#define LED_BLUE               (1U << 0)  

#define CONNECTING_SCREEN       0
#define GENERATE_CODE_SCREEN    1
#define LOOP_SCREEN             2
#define MAIN_SCREEN             3
#define LOCKED_SCREEN           4 

#define SENDING_TIME            5000
#define GC_HOLD_TIME            30
#define MSG_HOLD_TIME           5   

#define DOT_START_COL           11 
#define DOT_NUMBER              3
#define DOT_FREQ                0.5f   
#define SENDING_DOT_START_COL   6

#define RTC_FREQ                100000
#define UART_FREQ               9600
#define UART_DF                 8

#define DATA_FROM_RTC           8
#define GENERATED_CODE_SIZE     13

#define ASCII_OFFSET            48
#define SAVE_MASK               0x0F

//Maksimalne i minimalne vrijednosti primljenih podataka nakon obrade
#define HOUR_VALUE_MIN      0
#define HOUR_VALUE_MAX      23
#define MINUTE_VALUE_MIN    0
#define MINUTE_VALUE_MAX    59
#define SECONDS_VALUE_MIN   0
#define SECONDS_VALUE_MAX   59
#define DAY_VALUE_MIN       1
#define DAY_VALUE_MAX       31
#define WEEK_VALUE_MIN      0
#define WEEK_VALUE_MAX      6
#define MONTH_VALUE_MIN     1
#define MONTH_VALUE_MAX     12
#define YEAR_VALUE_MIN      0
#define YEAR_VALUE_MAX      99

/*******************Prototype functions***************************/
void getData(char *rtc_data);                                                 //Funkcija koja čita podatke s RTC modula (od sekundi do godina)
bool generateCode(char *generated_code);                                      //Funkcija koja generira kod
void RTCsetTime(char hour, char min, char sec);                               //Funkcija za namještavnja vremena RTC modula
void RTCsetDate(char day, char month, char year, char week = 0);              //Funkcija za namještavanje datuma RTC modula
uint8_t constrain(uint8_t value, uint8_t min, uint8_t max);                   //Funkcija za ograničavanje vrijednosti koje se mogu poslati u RTC modul da budu realne vrijednosti datuma i vremena
uint8_t decimal2BCD(uint8_t value);                                           //Pretvorba decimalne vrijednosti u BCD vrijednost koja se šalje u RTC modul
void init();                                                                  //Funkcija koja inicijalizira module
void rxInterrupt();                                                           //Prekidna funkcija
void connecting();                                                              
void stateMachine();  
void generateCodeFlag();
void sendingData(char *generated_code); 
void mainScreenFlag();
/****************************************************************/

/*************Global variables**************/                                               
char generated_code[GENERATED_CODE_SIZE]="";
int8_t current_screen=CONNECTING_SCREEN;
bool is_data_sent=false;
/******************************************/


int main()
{
    init();
   
    while(1)
    {
        stateMachine();
    }
}


void stateMachine()
{
       switch(current_screen)
       {
           case CONNECTING_SCREEN:
           connecting();
           break;
           
           case GENERATE_CODE_SCREEN:
           tipkalo.fall(NULL);
           bool check = generateCode(generated_code);
           
           if(check)
           {
               lcd.cls();
               lcd.printf("ERROR");
               wait(MSG_HOLD_TIME);
               current_screen = MAIN_SCREEN;
               break;
           }
           sendingData(generated_code);
           break;
            
           case LOOP_SCREEN:
           break; 
            
           case MAIN_SCREEN:
           tipkalo.fall(&generateCodeFlag);
           lcd.cls();
           lcd.printf("    Generator");
           lcd.locate(0,1);
           lcd.printf("      koda");   
           current_screen = LOOP_SCREEN;
           break;    
            
           case LOCKED_SCREEN:
           main_screen.detach();
           lcd.cls();
           lcd.printf("Standby/Locked");
           current_screen = LOOP_SCREEN;
           break;  
           
           default:
           current_screen=LOCKED_SCREEN;
           break;
       }
}

void connecting()
{   
    status_led = RED_LED; 
    HC06.putc('?');
    lcd.cls();
    lcd.printf("Povezivanje");

    while(current_screen == CONNECTING_SCREEN)
     {
        lcd.locate(DOT_START_COL,0);
        for(uint8_t column = 0; column < DOT_NUMBER; column++)
        {
            lcd.printf(".");
            HC06.putc('?');
            wait(DOT_FREQ);
        }
        lcd.locate(DOT_START_COL,0);
        lcd.printf("   ");
        wait(DOT_FREQ);
    }
}

void rxInterrupt()
{
    const char simbol = HC06.getc();
    switch (simbol) {
        case '?':
            status_led |= BLUE_LED;
            for(int i=0; i<BUFFER_SIZE; i++)
                HC06.putc(generated_code[i]);
            is_data_sent = true;
            status_led &=~BLUE_LED;
            break;

        case 'N':
            status_led = RED_LED;
            current_screen = LOCKED_SCREEN;
            break;

        case 'R':
            status_led = GREEN_LED;
            current_screen = MAIN_SCREEN;
            break;
    }
}

void generateCodeFlag()
{
    current_screen = GENERATE_CODE_SCREEN;
}

void sendingData(char *generated_code)
{   
    Timer counter;  
    lcd.cls();
    lcd.printf("Slanje");
    status_led |= BLUE_LED;
    counter.start();

    while(counter.read_ms()< SENDING_TIME && !is_data_sent){
        lcd.locate(SENDING_DOT_START_COL,0);
        for(uint8_t column = 0; column < DOT_NUMBER; column++) {
            lcd.printf(".");
            HC06.putc('1');
            wait(DOT_FREQ);
            if(is_data_sent) {break;}
        }
        lcd.locate(SENDING_DOT_START_COL,0);
        lcd.printf("   ");
        wait(DOT_FREQ);
    }
    vrijeme.stop();
    vrijeme.reset();
    lcd.cls();
    
    if(is_data_sent){
        lcd.printf("Generirani kod:");
        lcd.locate(0,1);
        lcd.printf("%s",generated_code);
        main_screen.attach(&mainScreenFlag,GC_HOLD_TIME);
        current_screen = LOOP_SCREEN;
    } else {
        lcd.printf("Nije se moguce");
        lcd.locate(0,1);
        lcd.printf("spojiti!");
        wait(MSG_HOLD_TIME);
        current_screen=MAIN_SCREEN;
    }
    is_data_sent = false;
}


void mainScreenFlag()
{
    current_screen = MAIN_SCREEN;
}


void getData(char *rtc_data)
{
    rtc.write(RTC_SLAVE_ADR  << 1, RTC_SEC_ADR, 1, true);               //Postavljanje početne adrese za čitanje
    rtc.read((RTC_SLAVE_ADR  << 1)| 0x01,rtc_data, DATA_FROM_RTC-1,false);   //Čitanje podataka od postavljenje adrese i nadalje
}

bool generateCode(char *generated_data)
{
    char data_in[DATA_FROM_RTC]="";
    getData(data_in);

    uint8_t j = 0;

    for (uint8_t i = 0; i < GENERATED_CODE_SIZE-1; i+= 2) {
        if(j==4) //preskakanje weekdays vrijednosti
            continue;

        generated_data[i]= ((data_in[j] >> 4) & SAVE_MASK) + ASCII_OFFSET;        //Izlučivanje desetinke i pretvorba u ASCII format
        generated_data[i+1]= (data_in[j] & SAVE_MASK) + ASCII_OFFSET;             //Izlučivanje jedinke i pretvorba u ASCII fromat
        j++;
    }

    for(int i=0; i<GENERATED_CODE_SIZE-1; i++) {                        //Provjera vrijednosti da li su znakovi u rasponu od 0 do 9
        if(data[i] < '0' || data[i] > '9')
            return 1;
    }
    return 0;
}



void init()
{
    HC06.baud(UART_FREQ);
    HC06.format(UART_DF,SerialBase::None);
    rtc.frequency(RTC_FREQ);
    status_led = LED_RED;
    HC06.attach(&rxInterrupt, Serial::RxIrq);
}


void RTCsetTime(char hour, char min, char sec)
{
    const char bytes_to_send = 4;

    hour = constrain(hour, HOUR_VALUE_MIN, HOUR_VALUE_MAX);
    min = constrain(min, MINUTE_VALUE_MIN, MINUTE_VALUE_MAX);
    sec = constrain(sec, SECONDS_VALUE_MIN, SECONDS_VALUE_MAX);

    const char data[bytes_to_send] = {SECONDS_ADR,
                                      Decimal2BCD(sec),
                                      Decimal2BCD(min),
                                      Decimal2BCD(hour)
                                     };

    rtc.write(SLAVE_ADR << 1,data, bytes_to_send, false);
}

void RTCsetDate(char day, char month, char year, char week)
{
    const char bytes_to_send = 5;

    day = constrain(day, DAY_VALUE_MIN, DAY_VALUE_MAX);
    week = constrain(week, WEEK_VALUE_MIN, WEEK_VALUE_MAX);
    month = constrain(month, MONTH_VALUE_MIN, MONTH_VALUE_MAX);
    year = constrain(year, YEAR_VALUE_MIN, YEAR_VALUE_MAX);

    const char data[bytes_to_send] = {DAY_ADR,
                                      Decimal2BCD(day),
                                      Decimal2BCD(week),
                                      Decimal2BCD(month),
                                      Decimal2BCD(year)
                                     };

    rtc.write(SLAVE_ADR << 1, data, bytes_to_send, false);
}

uint8_t constrain(uint8_t value, uint8_t min, uint8_t max)
{
    if(value < min) return min;
    else if(value > max) return max;
    else return value;
}


uint8_t decimal2BCD(uint8_t value)
{
    return ( (value/10) << 4 ) | (value%10); //npr 25dec -> 2 = 0010 << 4  | 5 = 0101  -> 0010 0101
}

