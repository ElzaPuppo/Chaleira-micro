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
int temperaturafinal=236;//valor de referência
int estado = 1;
int set_point = 236;//valor inicial somente

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;
    ini_clk();
    ini_portas();
    ini_timerA0();
    ini_timerA1();
    ini_USCI_B0_SPI();
    ini_max();

    __enable_interrupt();

    do{
           __bis_SR_register(LPM0_bits + GIE);

        } while(1);
}
void ini_clk(void)
{
    DCOCTL = CALDCO_1MHZ;// MCLK = 1Mhz
    BCSCTL1 = CALBC1_1MHZ; // SMCLK = 1Mhz
    BCSCTL2 = DIVS0;//+ DIVS1;//sem dividir
}
void ini_portas(void)
{

    P1DIR = BIT0+BIT5+BIT2+BIT6+BIT7;
        /*P1.0 = CS do CI
        P1.1 = entrada do encoder
        P1.2 = saída do led 1 para indicar funcionamento do encoder
        P1.3 = entrada do botao para acionar incoder
        P1.4 = entrada do encoder
        P1.5 = saida do clock pro CI
        P1.6 = saída led 2 indicando aquecimento acionado
        P1.7 = saida da palavra pro CI
	P2.0 = entrada/saída do sensor*/

    P1OUT = BIT4 + BIT1 + BIT3 ; //resistor de pull-up
    P1REN = BIT4 + BIT1 + BIT3 ; //habilita resistor


    P1IE |= BIT3 + BIT4; //habilita interrupções
    P1IES = BIT3 + BIT4; //borda de descida
    P1IFG = 0x00;

    P2DIR = 0xFF; //todos os bits como saída
    P2OUT = 0x00;

}
void ini_timerA0(void)
{
    /*
     * TIMER A com clock de 1 MHz
     * modo UP/DOWN
     *  n = td*clk   ; n = 0.0005 * clk
     */
    TA0CTL = TASSEL1 + MC0  + MC1;// up and down mode ; CLK
    TA0CCTL1 = OUTMOD2;// modo que inverte o sinal quando TAR =TA0CCR1
    TA0CCTL2 = CCIE;// HABILITA INT. DO CLK2

    TA0CCR0 = 500;// TIMER CONTA ATÉ ESTE VALOR SEMPRE, DEFINE O VALOR DA FREQUENCIA
    TA0CCR1 = 250;//CLK1
    TA0CCR2 = 250;//CLK2

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

    UCB0CTL1 |= UCSWRST; // Deixa a interface inativa ou em estado de RESET pra poder conFigurar

    UCB0CTL0 = UCMSB + UCMST + UCSYNC + UCCKPH;

    UCB0CTL1 = UCSSEL1  + UCSWRST;
    UCB0BR0 = 4; // Fator de divisao 1
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
        envio(0x0C,0x01);//shutdown
        envio(0x01,0x00);
        envio(0x02,0x00);
        envio(0x03,0x00);
        envio(0x04,0x00);
        config_display(set_point);

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

#pragma vector=TIMER0_A1_VECTOR
__interrupt void TA_RTI(void)
{

    TACCTL2 &= ~CCIFG;

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


