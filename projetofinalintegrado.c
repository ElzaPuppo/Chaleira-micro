#include <msp430.h> 

#define SYNC BIT0
void ini_clk(void);
void ini_portas(void);
void ini_timerA0(void);
void ini_timerA1(void);
void ini_USCI_B0_SPI(void);
void ini_max(void);
void config_display(int valor);
void envio(int reg,int data);
void check(int valor);
void reset_18B20(void);
void envia_byte_18B20(unsigned char data);
void leitura_bytes_18B20(unsigned char num_bytes);
void ini_18B20(void);
void leitura_temp_18B20(void);
void temporiza(unsigned int tempo);

int estado = 1;
int set_point = 355;
unsigned char sensor_data[9], temp_int = 0, g_data=0;
unsigned int temp_dec = 0;
int temperaturafinal = 0;

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    ini_clk();
    ini_portas();
    ini_timerA0();
    ini_timerA1();
    ini_USCI_B0_SPI();
    ini_max();
    ini_18B20();
    __enable_interrupt();

    do{
         __bis_SR_register(LPM0_bits + GIE);//treco que tava antes, tem a ver com o envio de palavras

        } while(1);
}

void leitura_temp_18B20(void){

    unsigned int i = 0, mask_bit, aux = 0;

    reset_18B20();

    envia_byte_18B20(0xCC); // Master issues Skip ROM command.
    envia_byte_18B20(0x44); // Master issues Convert T command.

    // Delay por SW. Melhor usar Timer com int. habilitada para isso.
    for(i=0;i<=20;i++){
        temporiza(50000);  // 20x50ms ~ 1s
    }

    reset_18B20();

    envia_byte_18B20(0xCC); // Master issues Skip ROM command.
    envia_byte_18B20(0xBE);

    leitura_bytes_18B20(9);

    temp_int = (sensor_data[0] >> 4) + (sensor_data[1] << 4);

    aux = sensor_data[0] & 0x0F; // Filtra parte de interesse
    mask_bit = 0x02;
    temp_dec = 0;
    if(aux & mask_bit)  temp_dec += 125; // Calcula a parte decimal como numero inteiro
    mask_bit = mask_bit << 1;
    if(aux & mask_bit)  temp_dec += 250;
    mask_bit = mask_bit << 1;
    if(aux & mask_bit)  temp_dec += 500;

    temperaturafinal=((int)temp_int)+(temp_dec/100);

    config_display(temperaturafinal);
}
void ini_18B20(void){
    reset_18B20();

    envia_byte_18B20(0xCC); // Master issues Skip ROM command.
    envia_byte_18B20(0x4E); // Master issues Write Scratchpad command.

    // Envia bytes de configuracao de Alarme (TH e TL) e Resolucao
    envia_byte_18B20(0x3F);  // Configura TH
    envia_byte_18B20(0x00);  // Configura TL
    envia_byte_18B20(0x7F);  // Ajusta resolucao para 12 bits

    reset_18B20();
}


void leitura_bytes_18B20(unsigned char num_bytes)
{

    unsigned char m = 0, n= 0, bit_mask;

    P2OUT &= ~BIT0;  // Saida nivel baixo
    temporiza(10);

    for(m=0; m <= (num_bytes - 1); m++)
    {
        bit_mask = 0x01;

        for(n=0;n<=7;n++)
        {
            P2DIR |= BIT0;
            temporiza(5);
            P2DIR &= ~BIT0;
            temporiza(10);
            if(P2IN & BIT0)
            {
                sensor_data[m] |= bit_mask;
                bit_mask = bit_mask << 1;
            }
            temporiza(60);
        }
    }
}


void temporiza(unsigned int tempo)
{
    /* Temporizacao em us
     *
     * Obs: Nao foi usado interrupcao devido a latencia de entrada/saida
     *      da RTI do Timer. Não é a mehor opção pra economizar energia.
     */

    TA0CCR0 = tempo;

    TA0CTL |= MC0; // Inicia contador

    TA0CCTL0 &= ~CCIFG;
    while( (~TA0CCTL0) & CCIFG ); // Sai do while quando temporizacao for atingida

    TA0CTL &= ~MC0; // Para contador
}
void ini_clk(void)
{
    DCOCTL = CALDCO_16MHZ;// Antes MCLK = 1Mhz
    BCSCTL1 = CALBC1_16MHZ; // Antes SMCLK = 1Mhz
    BCSCTL2 = DIVS0 + DIVS1; //2 MHz Antes era 500kHz
    BCSCTL3 = XCAP0 + XCAP1;
    __enable_interrupt();// Config. CPU para aceitar IRQ dos perif.
}
void ini_portas(void)
{

      /*P1.0 = CS do CI// agora é a entrada do sensor
        P1.1 = entrada do encoder
        P1.2 = saída do led 1 para indicar funcionamento do encoder
        P1.3 = entrada do botao para acionar incoder
        P1.4 = entrada do encoder
        P1.5 = saida do clock pro CI
        P1.6 = saída led 2 indicando aquecimento acionado
        P1.7 = saida da palavra pro CI*/

    P1DIR = BIT0 + BIT5+BIT2+BIT6+BIT7;
    P1OUT = BIT4 + BIT1 + BIT3 ; //resistor de pull-up
    P1REN = BIT4 + BIT1 + BIT3 ; //habilita resistor


    P1IE |= BIT3 + BIT4; //habilita interrupções
    P1IES = BIT3 + BIT4; //borda de descida
    P1IFG = 0x00;

    P2DIR = 0xFE; //todos os bits como saída
    P2OUT = 0x00;

}
void ini_timerA0(void)
{
    /*
     * TIMER A com clock de 1 MHz
     * modo UP/DOWN
     *  n = td*clk   ; n = 0.0005 * clk
     */
    //TA0CTL = TASSEL1 + MC0  + MC1;// up and down mode ; CLK
    //TA0CCTL1 = OUTMOD2;// modo que inverte o sinal quando TAR =TA0CCR1
    //TA0CCTL2 = CCIE;// HABILITA INT. DO CLK2

    //TA0CCR0 = 500;// TIMER CONTA ATÉ ESTE VALOR SEMPRE, DEFINE O VALOR DA FREQUENCIA
    //TA0CCR1 = 250;//CLK1
    //TA0CCR2 = 250;//CLK2
    TA0CTL = TASSEL1 + ID0;
}

void ini_timerA1(void)
{
    /*
     * TIMER A com clock de 62 500 Hz
     * modo UP/DOWN
     *
     */
      TA1CCR0=0;
      TA1CTL = TASSEL1 + MC0;// up mode até TA1CCR0
      TA1CCTL0 = CCIE;//habilita a interrupção


}

void ini_USCI_B0_SPI(void){

    /*
        Modo: SPI
        Clock: SMCLK
        8 bits de dados
    */

    UCB0CTL1 |= UCSWRST; // Deixa a interface inativa ou em estado de RESET pra poder consigurar

    UCB0CTL0 = UCMSB + UCMST + UCSYNC + UCCKPH;

    UCB0CTL1 = UCSSEL1  + UCSWRST;
    UCB0BR0 = 4; // Fator de divisao 4
    UCB0BR1 = 0;


    P1SEL |= BIT5 + BIT7; // Altera funcao dos pinos para UCB0CLK e UCB0SIMO
    P1SEL2 |= BIT5 + BIT7;

    UCB0CTL1 &= ~UCSWRST;//ativando de novo


    IFG2 &= ~UCB0TXIFG;

    IE2 |= UCB0TXIE;
}

void ini_max(){

       envio(0x00,0x00);//no-op
        envio(0x0B,0x03);//scan
        envio(0x0A,0x08);//intensity
        envio(0x09,0x0F);//decode
        envio(0x01,0x00);
        envio(0x02,0x00);
        envio(0x03,0x00);
        envio(0x04,0x00);
        envio(0x0C,0x01);//shutdown or not shutdown? that is the question
        envio(0x01,0x00);
        envio(0x02,0x00);
        envio(0x03,0x00);
        envio(0x04,0x00);
       // config_display(set_point);

}

void config_display(int valor){

   envio(0xF1,(valor/1000));
   envio(0xF2,((valor%1000)/100));
   envio(0xF3,(((valor%1000)%100)/10));
   envio(0xF4,(((valor%1000)%100)%10));

}

void envio(int reg,int data){

    P1OUT &= ~SYNC;
    UCB0TXBUF = reg ;
    while(UCB0STAT & UCBUSY);
    if(reg == 0xF3){
        data |= 0x80 ;
    }

    UCB0TXBUF = data;


    while(UCB0STAT & UCBUSY);

    P1OUT |= SYNC;
}

void check(int valor){

    if (temperaturafinal<set_point){

        P1OUT |= BIT6;
    }
    else P1OUT &= ~BIT6;
}

#pragma vector=TIMER1_A0_VECTOR
__interrupt void TA1_RTI(void)
{
    TA1CCR0 = 0;
    TA1CCTL0 &= ~CCIFG;

    if ((~P1IN) & BIT3)
    {
        if (estado == 0)
        {
            estado = 1;
            P1OUT &= ~BIT2;
            check(set_point);
            leitura_temp_18B20();// Ler variaveis "temp_int" e "temp_dec" com resultado
            temporiza(50000);
        }
        else
        {
            estado = 0;
            P1OUT |= BIT2;
            P1OUT &= ~BIT6;
        }
    }
    P1IFG &= ~BIT3;
    P1IE |=BIT3;//ativa novamente as interrupções da botao 1.3
}
void envia_byte_18B20(unsigned char data)
{
    unsigned char bit_mask = 0x01, i = 0;

      for(i=0; i <= 7; i++){

          if(data & bit_mask){ // Envia bit '1'
              P1DIR |= BIT0;  P1OUT &= ~BIT0;  // Saida nivel baixo
              temporiza(5);
              P1DIR &= ~BIT0;  // entrada
              temporiza(80);
          }else{
              P1DIR |= BIT0;  P1OUT &= ~BIT0;  // Saida nivel baixo
              temporiza(70);
              P1DIR &= ~BIT0;  // entrada
              temporiza(10);
          }
          bit_mask = bit_mask << 1;
      }
}


#pragma  vector=PORT1_VECTOR
__interrupt void P1_RTI(void)
{
    if(P1IFG & BIT3)//SE O BOTAO FOI PRESSIONADO
    {
        P1IE &= ~BIT3;
        P1IFG &= ~BIT3;
        TA1CCR0 = 5000;//debouncer de ~5 ms 1/1mega * 5000 = 5ms

    }
    else if ((P1IFG & BIT4) && (estado == 0)) //se tem interrupção na porta 4 foi encoder, mas só entra aqui se o estado do botão for 0= habilitado pra mudar o setpoint
    {
        P1IE &= ~BIT4;
        P1IFG &= ~BIT4;

       if((~P1IN)&BIT1)
       {
           if (set_point>0) set_point = set_point - 10;
       }
       else if ((~P1IN)&BIT4)
       {
           if(set_point<1000) set_point = set_point + 10;
       }
       //P1IFG &= ~BIT4;
       P1IE |= BIT4;//ativa novamente as interrupções do encoder
    }
    if(set_point>1000) set_point = 1000;
    if(set_point<1) set_point = 0;
    config_display(set_point);
}

#pragma vector=USCIAB0TX_VECTOR
__interrupt void RTI_USCI_B0_TX(void){

    IFG2 &= ~UCB0TXIFG;
}
void reset_18B20(void)
{
    P2DIR |= BIT0;
    P2OUT &= ~BIT0;  // Pino config. para saida nivel baixo
    temporiza(600);
    P2DIR &= ~BIT0;  // Pino config. para entrada
    temporiza(600);
}


