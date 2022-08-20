#ifndef _CPU_H_
#define _CPU_H_

#include <cstdint>

typedef union
{
    uint16_t w;
    struct
    {
        uint8_t l, h;
    } b;
} reg;

class SNES;
class CPU {
  public:
    typedef void (CPU::*opcode_func)(void);
    void (CPU::*opcodes[256][5])(void);

  public:
    SNES *snes = nullptr;

    struct PPP
    {
        int c, z, i, d, b, v, n, m, x, e;
    } p;

    reg a, x, y, s;

    uint32_t pbr = 0, dbr = 0;
    uint16_t pc = 0, dp = 0;

    int      cpumode = 0;
    uint8_t  opcode  = 0;
    int      cycles  = 0;
    int      setzf   = 0;
    int      inwai   = 0;
    uint32_t addr    = 0;

  public:
    CPU(SNES *_snes);

    void exec();

  public:
    void setzn8(int v);
    void setzn16(int v);
    void ADC8(uint16_t tempw, uint8_t temp);
    void ADC16(uint16_t tempw, uint32_t templ);
    void ADCBCD8(uint16_t tempw, uint8_t temp);
    void ADCBCD16(uint16_t tempw, uint32_t templ);
    void SBC8(uint16_t tempw, uint8_t temp);
    void SBC16(uint16_t tempw, uint32_t templ);
    void SBCBCD8(uint16_t tempw, uint8_t temp);
    void SBCBCD16(uint16_t tempw, uint32_t templ);


  public: /* Addressing modes */
    uint32_t absolute();
    uint32_t absolutex();
    uint32_t absolutey();
    uint32_t absolutelong();
    uint32_t absolutelongx();
    uint32_t zeropage();
    uint32_t zeropagex();
    uint32_t zeropagey();
    uint32_t stack();
    uint32_t indirect();
    uint32_t indirectx();
    uint32_t jindirectx();
    uint32_t indirecty();
    uint32_t sindirecty();
    uint32_t indirectl();
    uint32_t indirectly();

  public: /* Instructions */
    void inca8();
    void inca16();
    void inx8();
    void inx16();
    void iny8();
    void iny16();
    void deca8();
    void deca16();
    void dex8();
    void dex16();
    void dey8();
    void dey16();
    ;
    /* INC group */
    void incZp8();
    void incZp16();
    void incZpx8();
    void incZpx16();
    void incAbs8();
    void incAbs16();
    void incAbsx8();
    void incAbsx16();
    ;
    /* DEC group */
    void decZp8();
    void decZp16();
    void decZpx8();
    void decZpx16();
    void decAbs8();
    void decAbs16();
    void decAbsx8();
    void decAbsx16();
    ;
    /* Flag group */
    void clc();
    void cld();
    void cli();
    void clv();
    void sec();
    void sed();
    void sei();
    void xce();
    void sep();
    void rep();
    ;
    /* Transfer group */
    void tax8();
    void tay8();
    void txa8();
    void tya8();
    void tsx8();
    void txs8();
    void txy8();
    void tyx8();
    void tax16();
    void tay16();
    void txa16();
    void tya16();
    void tsx16();
    void txs16();
    void txy16();
    void tyx16();
    ;
    /* LDX group */
    void ldxImm8();
    void ldxZp8();
    void ldxZpy8();
    void ldxAbs8();
    void ldxAbsy8();
    void ldxImm16();
    void ldxZp16();
    void ldxZpy16();
    void ldxAbs16();
    void ldxAbsy16();
    ;
    /* LDY group */
    void ldyImm8();
    void ldyZp8();
    void ldyZpx8();
    void ldyAbs8();
    void ldyAbsx8();
    void ldyImm16();
    void ldyZp16();
    void ldyZpx16();
    void ldyAbs16();
    void ldyAbsx16();
    ;
    /* LDA group */
    void ldaImm8();
    void ldaZp8();
    void ldaZpx8();
    void ldaSp8();
    void ldaSIndirecty8();
    void ldaAbs8();
    void ldaAbsx8();
    void ldaAbsy8();
    void ldaLong8();
    void ldaLongx8();
    void ldaIndirect8();
    void ldaIndirectx8();
    void ldaIndirecty8();
    void ldaIndirectLong8();
    void ldaIndirectLongy8();
    void ldaImm16();
    void ldaZp16();
    void ldaZpx16();
    void ldaSp16();
    void ldaSIndirecty16();
    void ldaAbs16();
    void ldaAbsx16();
    void ldaAbsy16();
    void ldaLong16();
    void ldaLongx16();
    void ldaIndirect16();
    void ldaIndirectx16();
    void ldaIndirecty16();
    void ldaIndirectLong16();
    void ldaIndirectLongy16();
    ;
    /* STA group */
    void staZp8();
    void staZpx8();
    void staAbs8();
    void staAbsx8();
    void staAbsy8();
    void staLong8();
    void staLongx8();
    void staIndirect8();
    void staIndirectx8();
    void staIndirecty8();
    void staIndirectLong8();
    void staIndirectLongy8();
    void staSp8();
    void staSIndirecty8();
    void staZp16();
    void staZpx16();
    void staAbs16();
    void staAbsx16();
    void staAbsy16();
    void staLong16();
    void staLongx16();
    void staIndirect16();
    void staIndirectx16();
    void staIndirecty16();
    void staIndirectLong16();
    void staIndirectLongy16();
    void staSp16();
    void staSIndirecty16();
    ;
    /* STX group */
    void stxZp8();
    void stxZpy8();
    void stxAbs8();
    void stxZp16();
    void stxZpy16();
    void stxAbs16();
    ;
    /* STY group */
    void styZp8();
    void styZpx8();
    void styAbs8();
    void styZp16();
    void styZpx16();
    void styAbs16();
    ;
    /* STZ group */
    void stzZp8();
    void stzZpx8();
    void stzAbs8();
    void stzAbsx8();
    void stzZp16();
    void stzZpx16();
    void stzAbs16();
    void stzAbsx16();
    ;
    /* ADC group */
    void adcImm8();
    void adcZp8();
    void adcZpx8();
    void adcSp8();
    void adcAbs8();
    void adcAbsx8();
    void adcAbsy8();
    void adcLong8();
    void adcLongx8();
    void adcIndirect8();
    void adcIndirectx8();
    void adcIndirecty8();
    void adcsIndirecty8();
    void adcIndirectLong8();
    void adcIndirectLongy8();
    void adcImm16();
    void adcZp16();
    void adcZpx16();
    void adcSp16();
    void adcAbs16();
    void adcAbsx16();
    void adcAbsy16();
    void adcLong16();
    void adcLongx16();
    void adcIndirect16();
    void adcIndirectx16();
    void adcIndirecty16();
    void adcsIndirecty16();
    void adcIndirectLong16();
    void adcIndirectLongy16();
    ;
    /* SBC group */
    void sbcImm8();
    void sbcZp8();
    void sbcZpx8();
    void sbcSp8();
    void sbcAbs8();
    void sbcAbsx8();
    void sbcAbsy8();
    void sbcLong8();
    void sbcLongx8();
    void sbcIndirect8();
    void sbcIndirectx8();
    void sbcIndirecty8();
    void sbcIndirectLong8();
    void sbcIndirectLongy8();
    void sbcImm16();
    void sbcZp16();
    void sbcZpx16();
    void sbcSp16();
    void sbcAbs16();
    void sbcAbsx16();
    void sbcAbsy16();
    void sbcLong16();
    void sbcLongx16();
    void sbcIndirect16();
    void sbcIndirectx16();
    void sbcIndirecty16();
    void sbcIndirectLong16();
    void sbcIndirectLongy16();
    ;
    /* EOR group */
    void eorImm8();
    void eorZp8();
    void eorZpx8();
    void eorSp8();
    void eorAbs8();
    void eorAbsx8();
    void eorAbsy8();
    void eorLong8();
    void eorLongx8();
    void eorIndirect8();
    void eorIndirectx8();
    void eorIndirecty8();
    void eorIndirectLong8();
    void eorIndirectLongy8();
    void eorImm16();
    void eorZp16();
    void eorZpx16();
    void eorSp16();
    void eorAbs16();
    void eorAbsx16();
    void eorAbsy16();
    void eorLong16();
    void eorLongx16();
    void eorIndirect16();
    void eorIndirectx16();
    void eorIndirecty16();
    void eorIndirectLong16();
    void eorIndirectLongy16();
    ;
    /* AND group */
    void andImm8();
    void andZp8();
    void andZpx8();
    void andSp8();
    void andAbs8();
    void andAbsx8();
    void andAbsy8();
    void andLong8();
    void andLongx8();
    void andIndirect8();
    void andIndirectx8();
    void andIndirecty8();
    void andIndirectLong8();
    void andIndirectLongy8();
    void andImm16();
    void andZp16();
    void andZpx16();
    void andSp16();
    void andAbs16();
    void andAbsx16();
    void andAbsy16();
    void andLong16();
    void andLongx16();
    void andIndirect16();
    void andIndirectx16();
    void andIndirecty16();
    void andIndirectLong16();
    void andIndirectLongy16();
    ;
    /* ORA group */
    void oraImm8();
    void oraZp8();
    void oraZpx8();
    void oraSp8();
    void oraAbs8();
    void oraAbsx8();
    void oraAbsy8();
    void oraLong8();
    void oraLongx8();
    void oraIndirect8();
    void oraIndirectx8();
    void oraIndirecty8();
    void oraIndirectLong8();
    void oraIndirectLongy8();
    void oraImm16();
    void oraZp16();
    void oraZpx16();
    void oraSp16();
    void oraAbs16();
    void oraAbsx16();
    void oraAbsy16();
    void oraLong16();
    void oraLongx16();
    void oraIndirect16();
    void oraIndirectx16();
    void oraIndirecty16();
    void oraIndirectLong16();
    void oraIndirectLongy16();
    ;
    /* BIT group */
    void bitImm8();
    void bitImm16();
    void bitZp8();
    void bitZp16();
    void bitZpx8();
    void bitZpx16();
    void bitAbs8();
    void bitAbs16();
    void bitAbsx8();
    void bitAbsx16();
    ;
    /* CMP group */
    void cmpImm8();
    void cmpZp8();
    void cmpZpx8();
    void cmpSp8();
    void cmpAbs8();
    void cmpAbsx8();
    void cmpAbsy8();
    void cmpLong8();
    void cmpLongx8();
    void cmpIndirect8();
    void cmpIndirectx8();
    void cmpIndirecty8();
    void cmpIndirectLong8();
    void cmpIndirectLongy8();
    void cmpImm16();
    void cmpZp16();
    void cmpSp16();
    void cmpZpx16();
    void cmpAbs16();
    void cmpAbsx16();
    void cmpAbsy16();
    void cmpLong16();
    void cmpLongx16();
    void cmpIndirect16();
    void cmpIndirectx16();
    void cmpIndirecty16();
    void cmpIndirectLong16();
    void cmpIndirectLongy16();
    ;
    /* Stack Group */
    void phb();
    void phbe();
    void phk();
    void phke();
    void pea();
    void pei();
    void per();
    void phd();
    void pld();
    void pha8();
    void pha16();
    void phx8();
    void phx16();
    void phy8();
    void phy16();
    void pla8();
    void pla16();
    void plx8();
    void plx16();
    void ply8();
    void ply16();
    void plb();
    void plbe();
    void plp();
    void plpe();
    void php();
    void phpe();
    ;
    /* CPX group */
    void cpxImm8();
    void cpxImm16();
    void cpxZp8();
    void cpxZp16();
    void cpxAbs8();
    void cpxAbs16();
    ;
    /* CPY group */
    void cpyImm8();
    void cpyImm16();
    void cpyZp8();
    void cpyZp16();
    void cpyAbs8();
    void cpyAbs16();
    ;
    /* Branch group */
    void bcc();
    void bcs();
    void beq();
    void bne();
    void bpl();
    void bmi();
    void bvc();
    void bvs();
    void bra();
    void brl();
    ;
    /* Jump group */
    void jmp();
    void jmplong();
    void jmpind();
    void jmpindx();
    void jmlind();
    void jsr();
    void jsre();
    void jsrIndx();
    void jsrIndxe();
    void jsl();
    void jsle();
    void rtl();
    void rtle();
    void rts();
    void rtse();
    void rti();
    ;
    /* Shift group */
    void asla8();
    void asla16();
    void aslZp8();
    void aslZp16();
    void aslZpx8();
    void aslZpx16();
    void aslAbs8();
    void aslAbs16();
    void aslAbsx8();
    void aslAbsx16();
    void lsra8();
    void lsra16();
    void lsrZp8();
    void lsrZp16();
    void lsrZpx8();
    void lsrZpx16();
    void lsrAbs8();
    void lsrAbs16();
    void lsrAbsx8();
    void lsrAbsx16();
    void rola8();
    void rola16();
    void rolZp8();
    void rolZp16();
    void rolZpx8();
    void rolZpx16();
    void rolAbs8();
    void rolAbs16();
    void rolAbsx8();
    void rolAbsx16();
    void rora8();
    void rora16();
    void rorZp8();
    void rorZp16();
    void rorZpx8();
    void rorZpx16();
    void rorAbs8();
    void rorAbs16();
    void rorAbsx8();
    void rorAbsx16();
    ;
    /* Misc group */
    void xba();
    void nop();
    void tcd();
    void tdc();
    void tcs();
    void tsc();
    void trbZp8();
    void trbZp16();
    void trbAbs8();
    void trbAbs16();
    void tsbZp8();
    void tsbZp16();
    void tsbAbs8();
    void tsbAbs16();
    void wai();
    void mvp();
    void mvn();
    void brk();

    void reset65c816();

    void badopcode();
    void updatecpumode();

    void nmi65c816();
    void irq65c816();
    void makeopcodetable();
};
#endif
