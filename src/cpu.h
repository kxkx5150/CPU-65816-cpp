#ifndef _65C816_H_
#define _65C816_H_
#include "snes.h"
#include "utils/port.h"
#include <vector>

#define Carry      1
#define Zero       2
#define IRQ        4
#define Decimal    8
#define IndexFlag  16
#define MemoryFlag 32
#define Overflow   64
#define Negative   128
#define Emulation  256

#define AL   A.B.l
#define AH   A.B.h
#define XL   X.B.l
#define XH   X.B.h
#define YL   Y.B.l
#define YH   Y.B.h
#define SL   S.B.l
#define SH   S.B.h
#define DL   D.B.l
#define DH   D.B.h
#define PL   P.B.l
#define PH   P.B.h
#define PBPC PC.xPBPC
#define PCw  PC.W.xPC
#define PCh  PC.B.xPCh
#define PCl  PC.B.xPCl
#define PB   PC.B.xPB

#define SetCarry()      (ICPU._Carry = 1)
#define ClearCarry()    (ICPU._Carry = 0)
#define SetZero()       (ICPU._Zero = 0)
#define ClearZero()     (ICPU._Zero = 1)
#define SetIRQ()        (Registers.PL |= IRQ)
#define ClearIRQ()      (Registers.PL &= ~IRQ)
#define SetDecimal()    (Registers.PL |= Decimal)
#define ClearDecimal()  (Registers.PL &= ~Decimal)
#define SetIndex()      (Registers.PL |= IndexFlag)
#define ClearIndex()    (Registers.PL &= ~IndexFlag)
#define SetMemory()     (Registers.PL |= MemoryFlag)
#define ClearMemory()   (Registers.PL &= ~MemoryFlag)
#define SetOverflow()   (ICPU._Overflow = 1)
#define ClearOverflow() (ICPU._Overflow = 0)
#define SetNegative()   (ICPU._Negative = 0x80)
#define ClearNegative() (ICPU._Negative = 0)
#define CheckCarry()    (ICPU._Carry)
#define CheckZero()     (ICPU._Zero == 0)
#define CheckIRQ()      (Registers.PL & IRQ)
#define CheckDecimal()  (Registers.PL & Decimal)
#define CheckIndex()    (Registers.PL & IndexFlag)
#define CheckMemory()   (Registers.PL & MemoryFlag)
#define CheckOverflow() (ICPU._Overflow)
#define CheckNegative() (ICPU._Negative & 0x80)
#define SetFlags(f)     (Registers.P.W |= (f))
#define ClearFlags(f)   (Registers.P.W &= ~(f))
#define CheckFlag(f)    (Registers.PL & (f))
#define CHECK_FOR_IRQ_CHANGE()                                                                                         \
    if (Timings.IRQFlagChanging) {                                                                                     \
        if (Timings.IRQFlagChanging & IRQ_TRIGGER_NMI) {                                                               \
            NMIPending            = TRUE;                                                                              \
            Timings.NMITriggerPos = Cycles + 6;                                                                        \
        }                                                                                                              \
        if (Timings.IRQFlagChanging & IRQ_CLEAR_FLAG)                                                                  \
            ClearIRQ();                                                                                                \
        else if (Timings.IRQFlagChanging & IRQ_SET_FLAG)                                                               \
            SetIRQ();                                                                                                  \
        Timings.IRQFlagChanging = IRQ_NONE;                                                                            \
    }
#define CHECK_FOR_IRQ()                                                                                                \
    {                                                                                                                  \
    }

enum s9xwrap_t
{
    WRAP_NONE,
    WRAP_BANK,
    WRAP_PAGE
};
typedef enum
{
    NONE   = 0,
    READ   = 1,
    WRITE  = 2,
    MODIFY = 3,
    JUMP   = 5,
    JSR    = 8
} AccessMode;
typedef union
{
    struct
    {
        uint8 xPCl, xPCh, xPB, z;
    } B;
    struct
    {
        uint16 xPC, d;
    } W;
    uint32 xPBPC;
} PC_t;

class SCPUState {
  private:
    typedef void (SCPUState::*op_handler)(void);
    typedef struct
    {
        op_handler opcode_handler;
    } op_hndls;
    std::vector<op_hndls> S9xOpcodesM1X1;
    std::vector<op_hndls> S9xOpcodesE1;
    std::vector<op_hndls> S9xOpcodesM1X0;
    std::vector<op_hndls> S9xOpcodesM0X0;
    std::vector<op_hndls> S9xOpcodesM0X1;
    std::vector<op_hndls> S9xOpcodesSlow;

  public:
    SNESX *snes = nullptr;

  public:
    typedef union
    {
        struct
        {
            uint8 l, h;
        } B;
        uint16 W;
    } pair;

    struct SRegisters
    {
        uint8 DB;
        pair  P;
        pair  A;
        pair  D;
        pair  S;
        pair  X;
        pair  Y;
        PC_t  PC;
    } Registers;

    struct SICPU
    {
        std::vector<op_hndls> *S9xOpcodes;
        uint8                 *S9xOpLengths;
        uint8                  _Carry;
        uint8                  _Zero;
        uint8                  _Negative;
        uint8                  _Overflow;
        uint32                 ShiftedPB;
        uint32                 ShiftedDB;
        uint32                 Frame;
        uint32                 FrameAdvanceCount;
    };
    SICPU ICPU;

  public:
    uint32 Flags;
    int32  Cycles;
    int32  PrevCycles;
    int32  V_Counter;
    uint8 *PCBase;
    bool8  NMIPending;
    bool8  IRQLine;
    bool8  IRQTransition;
    bool8  IRQLastState;
    bool8  IRQExternal;
    int32  IRQPending;
    int32  MemSpeed;
    int32  MemSpeedx2;
    int32  FastROMSpeed;
    bool8  InDMA;
    bool8  InHDMA;
    bool8  InDMAorHDMA;
    bool8  InWRAMDMAorHDMA;
    uint8  HDMARanInDMA;
    int32  CurrentDMAorHDMAChannel;
    uint8  WhichEvent;
    int32  NextEvent;
    bool8  WaitingForInterrupt;
    uint32 AutoSaveTimer;
    bool8  SRAMModified;

  public:
    SCPUState(SNESX *_snes);
    void create_op_table();

  public:
    void S9xResetCPU(void);
    void S9xSoftResetCPU(void);
    void S9xReset(void);
    void S9xSoftReset(void);
    void S9xMainLoop(void);
    void S9xReschedule(void);
    void S9xDoHEventProcessing(void);
    void S9xOpcode_IRQ(void);
    void S9xOpcode_NMI(void);

    void SetZN(uint16 Work16);
    void SetZN(uint8 Work8);
    void ADC(uint16 Work16);
    void ADC(uint8 Work8);
    void AND(uint16 Work16);
    void AND(uint8 Work8);
    void ASL16(uint32 OpAddress, s9xwrap_t w);
    void ASL8(uint32 OpAddress);
    void BIT(uint16 Work16);
    void BIT(uint8 Work8);
    void CMP(uint16 val);
    void CMP(uint8 val);
    void CPX(uint16 val);
    void CPX(uint8 val);
    void CPY(uint16 val);
    void CPY(uint8 val);
    void DEC16(uint32 OpAddress, s9xwrap_t w);
    void DEC8(uint32 OpAddress);
    void EOR(uint16 val);
    void EOR(uint8 val);
    void INC16(uint32 OpAddress, s9xwrap_t w);
    void INC8(uint32 OpAddress);
    void LDA(uint16 val);
    void LDA(uint8 val);
    void LDX(uint16 val);
    void LDX(uint8 val);
    void LDY(uint16 val);
    void LDY(uint8 val);
    void LSR16(uint32 OpAddress, s9xwrap_t w);
    void LSR8(uint32 OpAddress);
    void ORA(uint16 val);
    void ORA(uint8 val);
    void ROL16(uint32 OpAddress, s9xwrap_t w);
    void ROL8(uint32 OpAddress);
    void ROR16(uint32 OpAddress, s9xwrap_t w);
    void ROR8(uint32 OpAddress);
    void SBC(uint16 Work16);
    void SBC(uint8 Work8);
    void STA16(uint32 OpAddress, enum s9xwrap_t w);
    void STA8(uint32 OpAddress);
    void STX16(uint32 OpAddress, enum s9xwrap_t w);
    void STX8(uint32 OpAddress);
    void STY16(uint32 OpAddress, enum s9xwrap_t w);
    void STY8(uint32 OpAddress);
    void STZ16(uint32 OpAddress, enum s9xwrap_t w);
    void STZ8(uint32 OpAddress);
    void TSB16(uint32 OpAddress, enum s9xwrap_t w);
    void TSB8(uint32 OpAddress);
    void TRB16(uint32 OpAddress, enum s9xwrap_t w);
    void TRB8(uint32 OpAddress);

    void Op69M1(void);
    void Op69M0(void);
    void Op69Slow(void);
    void Op65M1(void);
    void Op65M0(void);
    void Op65Slow(void);
    void Op75E1(void);
    void Op75E0M1(void);
    void Op75E0M0(void);
    void Op75Slow(void);
    void Op72E1(void);
    void Op72E0M1(void);
    void Op72E0M0(void);
    void Op72Slow(void);
    void Op61E1(void);
    void Op61E0M1(void);
    void Op61E0M0(void);
    void Op61Slow(void);
    void Op71E1(void);
    void Op71E0M1X1(void);
    void Op71E0M0X1(void);
    void Op71E0M1X0(void);
    void Op71E0M0X0(void);
    void Op71Slow(void);
    void Op67M1(void);
    void Op67M0(void);
    void Op67Slow(void);
    void Op77M1(void);
    void Op77M0(void);
    void Op77Slow(void);
    void Op6DM1(void);
    void Op6DM0(void);
    void Op6DSlow(void);
    void Op7DM1X1(void);
    void Op7DM0X1(void);
    void Op7DM1X0(void);
    void Op7DM0X0(void);
    void Op7DSlow(void);
    void Op79M1X1(void);
    void Op79M0X1(void);
    void Op79M1X0(void);
    void Op79M0X0(void);
    void Op79Slow(void);
    void Op6FM1(void);
    void Op6FM0(void);
    void Op6FSlow(void);
    void Op7FM1(void);
    void Op7FM0(void);
    void Op7FSlow(void);
    void Op63M1(void);
    void Op63M0(void);
    void Op63Slow(void);
    void Op73M1(void);
    void Op73M0(void);
    void Op73Slow(void);
    void Op29M1(void);
    void Op29M0(void);
    void Op29Slow(void);
    void Op25M1(void);
    void Op25M0(void);
    void Op25Slow(void);
    void Op35E1(void);
    void Op35E0M1(void);
    void Op35E0M0(void);
    void Op35Slow(void);
    void Op32E1(void);
    void Op32E0M1(void);
    void Op32E0M0(void);
    void Op32Slow(void);
    void Op21E1(void);
    void Op21E0M1(void);
    void Op21E0M0(void);
    void Op21Slow(void);
    void Op31E1(void);
    void Op31E0M1X1(void);
    void Op31E0M0X1(void);
    void Op31E0M1X0(void);
    void Op31E0M0X0(void);
    void Op31Slow(void);
    void Op27M1(void);
    void Op27M0(void);
    void Op27Slow(void);
    void Op37M1(void);
    void Op37M0(void);
    void Op37Slow(void);
    void Op2DM1(void);
    void Op2DM0(void);
    void Op2DSlow(void);
    void Op3DM1X1(void);
    void Op3DM0X1(void);
    void Op3DM1X0(void);
    void Op3DM0X0(void);
    void Op3DSlow(void);
    void Op39M1X1(void);
    void Op39M0X1(void);
    void Op39M1X0(void);
    void Op39M0X0(void);
    void Op39Slow(void);
    void Op2FM1(void);
    void Op2FM0(void);
    void Op2FSlow(void);
    void Op3FM1(void);
    void Op3FM0(void);
    void Op3FSlow(void);
    void Op23M1(void);
    void Op23M0(void);
    void Op23Slow(void);
    void Op33M1(void);
    void Op33M0(void);
    void Op33Slow(void);
    void Op0AM1(void);
    void Op0AM0(void);
    void Op0ASlow(void);
    void Op06M1(void);
    void Op06M0(void);
    void Op06Slow(void);
    void Op16E1(void);
    void Op16E0M1(void);
    void Op16E0M0(void);
    void Op16Slow(void);
    void Op0EM1(void);
    void Op0EM0(void);
    void Op0ESlow(void);
    void Op1EM1X1(void);
    void Op1EM0X1(void);
    void Op1EM1X0(void);
    void Op1EM0X0(void);
    void Op1ESlow(void);
    void Op89M1(void);
    void Op89M0(void);
    void Op89Slow(void);
    void Op24M1(void);
    void Op24M0(void);
    void Op24Slow(void);
    void Op34E1(void);
    void Op34E0M1(void);
    void Op34E0M0(void);
    void Op34Slow(void);
    void Op2CM1(void);
    void Op2CM0(void);
    void Op2CSlow(void);
    void Op3CM1X1(void);
    void Op3CM0X1(void);
    void Op3CM1X0(void);
    void Op3CM0X0(void);
    void Op3CSlow(void);
    void OpC9M1(void);
    void OpC9M0(void);
    void OpC9Slow(void);
    void OpC5M1(void);
    void OpC5M0(void);
    void OpC5Slow(void);
    void OpD5E1(void);
    void OpD5E0M1(void);
    void OpD5E0M0(void);
    void OpD5Slow(void);
    void OpD2E1(void);
    void OpD2E0M1(void);
    void OpD2E0M0(void);
    void OpD2Slow(void);
    void OpC1E1(void);
    void OpC1E0M1(void);
    void OpC1E0M0(void);
    void OpC1Slow(void);
    void OpD1E1(void);
    void OpD1E0M1X1(void);
    void OpD1E0M0X1(void);
    void OpD1E0M1X0(void);
    void OpD1E0M0X0(void);
    void OpD1Slow(void);
    void OpC7M1(void);
    void OpC7M0(void);
    void OpC7Slow(void);
    void OpD7M1(void);
    void OpD7M0(void);
    void OpD7Slow(void);
    void OpCDM1(void);
    void OpCDM0(void);
    void OpCDSlow(void);
    void OpDDM1X1(void);
    void OpDDM0X1(void);
    void OpDDM1X0(void);
    void OpDDM0X0(void);
    void OpDDSlow(void);
    void OpD9M1X1(void);
    void OpD9M0X1(void);
    void OpD9M1X0(void);
    void OpD9M0X0(void);
    void OpD9Slow(void);
    void OpCFM1(void);
    void OpCFM0(void);
    void OpCFSlow(void);
    void OpDFM1(void);
    void OpDFM0(void);
    void OpDFSlow(void);
    void OpC3M1(void);
    void OpC3M0(void);
    void OpC3Slow(void);
    void OpD3M1(void);
    void OpD3M0(void);
    void OpD3Slow(void);
    void OpE0X1(void);
    void OpE0X0(void);
    void OpE0Slow(void);
    void OpE4X1(void);
    void OpE4X0(void);
    void OpE4Slow(void);
    void OpECX1(void);
    void OpECX0(void);
    void OpECSlow(void);
    void OpC0X1(void);
    void OpC0X0(void);
    void OpC0Slow(void);
    void OpC4X1(void);
    void OpC4X0(void);
    void OpC4Slow(void);
    void OpCCX1(void);
    void OpCCX0(void);
    void OpCCSlow(void);
    void Op3AM1(void);
    void Op3AM0(void);
    void Op3ASlow(void);
    void OpC6M1(void);
    void OpC6M0(void);
    void OpC6Slow(void);
    void OpD6E1(void);
    void OpD6E0M1(void);
    void OpD6E0M0(void);
    void OpD6Slow(void);
    void OpCEM1(void);
    void OpCEM0(void);
    void OpCESlow(void);
    void OpDEM1X1(void);
    void OpDEM0X1(void);
    void OpDEM1X0(void);
    void OpDEM0X0(void);
    void OpDESlow(void);
    void Op49M1(void);
    void Op49M0(void);
    void Op49Slow(void);
    void Op45M1(void);
    void Op45M0(void);
    void Op45Slow(void);
    void Op55E1(void);
    void Op55E0M1(void);
    void Op55E0M0(void);
    void Op55Slow(void);
    void Op52E1(void);
    void Op52E0M1(void);
    void Op52E0M0(void);
    void Op52Slow(void);
    void Op41E1(void);
    void Op41E0M1(void);
    void Op41E0M0(void);
    void Op41Slow(void);
    void Op51E1(void);
    void Op51E0M1X1(void);
    void Op51E0M0X1(void);
    void Op51E0M1X0(void);
    void Op51E0M0X0(void);
    void Op51Slow(void);
    void Op47M1(void);
    void Op47M0(void);
    void Op47Slow(void);
    void Op57M1(void);
    void Op57M0(void);
    void Op57Slow(void);
    void Op4DM1(void);
    void Op4DM0(void);
    void Op4DSlow(void);
    void Op5DM1X1(void);
    void Op5DM0X1(void);
    void Op5DM1X0(void);
    void Op5DM0X0(void);
    void Op5DSlow(void);
    void Op59M1X1(void);
    void Op59M0X1(void);
    void Op59M1X0(void);
    void Op59M0X0(void);
    void Op59Slow(void);
    void Op4FM1(void);
    void Op4FM0(void);
    void Op4FSlow(void);
    void Op5FM1(void);
    void Op5FM0(void);
    void Op5FSlow(void);
    void Op43M1(void);
    void Op43M0(void);
    void Op43Slow(void);
    void Op53M1(void);
    void Op53M0(void);
    void Op53Slow(void);
    void Op1AM1(void);
    void Op1AM0(void);
    void Op1ASlow(void);
    void OpE6M1(void);
    void OpE6M0(void);
    void OpE6Slow(void);
    void OpF6E1(void);
    void OpF6E0M1(void);
    void OpF6E0M0(void);
    void OpF6Slow(void);
    void OpEEM1(void);
    void OpEEM0(void);
    void OpEESlow(void);
    void OpFEM1X1(void);
    void OpFEM0X1(void);
    void OpFEM1X0(void);
    void OpFEM0X0(void);
    void OpFESlow(void);
    void OpA9M1(void);
    void OpA9M0(void);
    void OpA9Slow(void);
    void OpA5M1(void);
    void OpA5M0(void);
    void OpA5Slow(void);
    void OpB5E1(void);
    void OpB5E0M1(void);
    void OpB5E0M0(void);
    void OpB5Slow(void);
    void OpB2E1(void);
    void OpB2E0M1(void);
    void OpB2E0M0(void);
    void OpB2Slow(void);
    void OpA1E1(void);
    void OpA1E0M1(void);
    void OpA1E0M0(void);
    void OpA1Slow(void);
    void OpB1E1(void);
    void OpB1E0M1X1(void);
    void OpB1E0M0X1(void);
    void OpB1E0M1X0(void);
    void OpB1E0M0X0(void);
    void OpB1Slow(void);
    void OpA7M1(void);
    void OpA7M0(void);
    void OpA7Slow(void);
    void OpB7M1(void);
    void OpB7M0(void);
    void OpB7Slow(void);
    void OpADM1(void);
    void OpADM0(void);
    void OpADSlow(void);
    void OpBDM1X1(void);
    void OpBDM0X1(void);
    void OpBDM1X0(void);
    void OpBDM0X0(void);
    void OpBDSlow(void);
    void OpB9M1X1(void);
    void OpB9M0X1(void);
    void OpB9M1X0(void);
    void OpB9M0X0(void);
    void OpB9Slow(void);
    void OpAFM1(void);
    void OpAFM0(void);
    void OpAFSlow(void);
    void OpBFM1(void);
    void OpBFM0(void);
    void OpBFSlow(void);
    void OpA3M1(void);
    void OpA3M0(void);
    void OpA3Slow(void);
    void OpB3M1(void);
    void OpB3M0(void);
    void OpB3Slow(void);
    void OpA2X1(void);
    void OpA2X0(void);
    void OpA2Slow(void);
    void OpA6X1(void);
    void OpA6X0(void);
    void OpA6Slow(void);
    void OpB6E1(void);
    void OpB6E0X1(void);
    void OpB6E0X0(void);
    void OpB6Slow(void);
    void OpAEX1(void);
    void OpAEX0(void);
    void OpAESlow(void);
    void OpBEX1(void);
    void OpBEX0(void);
    void OpBESlow(void);
    void OpA0X1(void);
    void OpA0X0(void);
    void OpA0Slow(void);
    void OpA4X1(void);
    void OpA4X0(void);
    void OpA4Slow(void);
    void OpB4E1(void);
    void OpB4E0X1(void);
    void OpB4E0X0(void);
    void OpB4Slow(void);
    void OpACX1(void);
    void OpACX0(void);
    void OpACSlow(void);
    void OpBCX1(void);
    void OpBCX0(void);
    void OpBCSlow(void);
    void Op4AM1(void);
    void Op4AM0(void);
    void Op4ASlow(void);
    void Op46M1(void);
    void Op46M0(void);
    void Op46Slow(void);
    void Op56E1(void);
    void Op56E0M1(void);
    void Op56E0M0(void);
    void Op56Slow(void);
    void Op4EM1(void);
    void Op4EM0(void);
    void Op4ESlow(void);
    void Op5EM1X1(void);
    void Op5EM0X1(void);
    void Op5EM1X0(void);
    void Op5EM0X0(void);
    void Op5ESlow(void);
    void Op09M1(void);
    void Op09M0(void);
    void Op09Slow(void);
    void Op05M1(void);
    void Op05M0(void);
    void Op05Slow(void);
    void Op15E1(void);
    void Op15E0M1(void);
    void Op15E0M0(void);
    void Op15Slow(void);
    void Op12E1(void);
    void Op12E0M1(void);
    void Op12E0M0(void);
    void Op12Slow(void);
    void Op01E1(void);
    void Op01E0M1(void);
    void Op01E0M0(void);
    void Op01Slow(void);
    void Op11E1(void);
    void Op11E0M1X1(void);
    void Op11E0M0X1(void);
    void Op11E0M1X0(void);
    void Op11E0M0X0(void);
    void Op11Slow(void);
    void Op07M1(void);
    void Op07M0(void);
    void Op07Slow(void);
    void Op17M1(void);
    void Op17M0(void);
    void Op17Slow(void);
    void Op0DM1(void);
    void Op0DM0(void);
    void Op0DSlow(void);
    void Op1DM1X1(void);
    void Op1DM0X1(void);
    void Op1DM1X0(void);
    void Op1DM0X0(void);
    void Op1DSlow(void);
    void Op19M1X1(void);
    void Op19M0X1(void);
    void Op19M1X0(void);
    void Op19M0X0(void);
    void Op19Slow(void);
    void Op0FM1(void);
    void Op0FM0(void);
    void Op0FSlow(void);
    void Op1FM1(void);
    void Op1FM0(void);
    void Op1FSlow(void);
    void Op03M1(void);
    void Op03M0(void);
    void Op03Slow(void);
    void Op13M1(void);
    void Op13M0(void);
    void Op13Slow(void);
    void Op2AM1(void);
    void Op2AM0(void);
    void Op2ASlow(void);
    void Op26M1(void);
    void Op26M0(void);
    void Op26Slow(void);
    void Op36E1(void);
    void Op36E0M1(void);
    void Op36E0M0(void);
    void Op36Slow(void);
    void Op2EM1(void);
    void Op2EM0(void);
    void Op2ESlow(void);
    void Op3EM1X1(void);
    void Op3EM0X1(void);
    void Op3EM1X0(void);
    void Op3EM0X0(void);
    void Op3ESlow(void);
    void Op6AM1(void);
    void Op6AM0(void);
    void Op6ASlow(void);
    void Op66M1(void);
    void Op66M0(void);
    void Op66Slow(void);
    void Op76E1(void);
    void Op76E0M1(void);
    void Op76E0M0(void);
    void Op76Slow(void);
    void Op6EM1(void);
    void Op6EM0(void);
    void Op6ESlow(void);
    void Op7EM1X1(void);
    void Op7EM0X1(void);
    void Op7EM1X0(void);
    void Op7EM0X0(void);
    void Op7ESlow(void);
    void OpE9M1(void);
    void OpE9M0(void);
    void OpE9Slow(void);
    void OpE5M1(void);
    void OpE5M0(void);
    void OpE5Slow(void);
    void OpF5E1(void);
    void OpF5E0M1(void);
    void OpF5E0M0(void);
    void OpF5Slow(void);
    void OpF2E1(void);
    void OpF2E0M1(void);
    void OpF2E0M0(void);
    void OpF2Slow(void);
    void OpE1E1(void);
    void OpE1E0M1(void);
    void OpE1E0M0(void);
    void OpE1Slow(void);
    void OpF1E1(void);
    void OpF1E0M1X1(void);
    void OpF1E0M0X1(void);
    void OpF1E0M1X0(void);
    void OpF1E0M0X0(void);
    void OpF1Slow(void);
    void OpE7M1(void);
    void OpE7M0(void);
    void OpE7Slow(void);
    void OpF7M1(void);
    void OpF7M0(void);
    void OpF7Slow(void);
    void OpEDM1(void);
    void OpEDM0(void);
    void OpEDSlow(void);
    void OpFDM1X1(void);
    void OpFDM0X1(void);
    void OpFDM1X0(void);
    void OpFDM0X0(void);
    void OpFDSlow(void);
    void OpF9M1X1(void);
    void OpF9M0X1(void);
    void OpF9M1X0(void);
    void OpF9M0X0(void);
    void OpF9Slow(void);
    void OpEFM1(void);
    void OpEFM0(void);
    void OpEFSlow(void);
    void OpFFM1(void);
    void OpFFM0(void);
    void OpFFSlow(void);
    void OpE3M1(void);
    void OpE3M0(void);
    void OpE3Slow(void);
    void OpF3M1(void);
    void OpF3M0(void);
    void OpF3Slow(void);
    void Op85M1(void);
    void Op85M0(void);
    void Op85Slow(void);
    void Op95E1(void);
    void Op95E0M1(void);
    void Op95E0M0(void);
    void Op95Slow(void);
    void Op92E1(void);
    void Op92E0M1(void);
    void Op92E0M0(void);
    void Op92Slow(void);
    void Op81E1(void);
    void Op81E0M1(void);
    void Op81E0M0(void);
    void Op81Slow(void);
    void Op91E1(void);
    void Op91E0M1X1(void);
    void Op91E0M0X1(void);
    void Op91E0M1X0(void);
    void Op91E0M0X0(void);
    void Op91Slow(void);
    void Op87M1(void);
    void Op87M0(void);
    void Op87Slow(void);
    void Op97M1(void);
    void Op97M0(void);
    void Op97Slow(void);
    void Op8DM1(void);
    void Op8DM0(void);
    void Op8DSlow(void);
    void Op9DM1X1(void);
    void Op9DM0X1(void);
    void Op9DM1X0(void);
    void Op9DM0X0(void);
    void Op9DSlow(void);
    void Op99M1X1(void);
    void Op99M0X1(void);
    void Op99M1X0(void);
    void Op99M0X0(void);
    void Op99Slow(void);
    void Op8FM1(void);
    void Op8FM0(void);
    void Op8FSlow(void);
    void Op9FM1(void);
    void Op9FM0(void);
    void Op9FSlow(void);
    void Op83M1(void);
    void Op83M0(void);
    void Op83Slow(void);
    void Op93M1(void);
    void Op93M0(void);
    void Op93Slow(void);
    void Op86X1(void);
    void Op86X0(void);
    void Op86Slow(void);
    void Op96E1(void);
    void Op96E0X1(void);
    void Op96E0X0(void);
    void Op96Slow(void);
    void Op8EX1(void);
    void Op8EX0(void);
    void Op8ESlow(void);
    void Op84X1(void);
    void Op84X0(void);
    void Op84Slow(void);
    void Op94E1(void);
    void Op94E0X1(void);
    void Op94E0X0(void);
    void Op94Slow(void);
    void Op8CX1(void);
    void Op8CX0(void);
    void Op8CSlow(void);
    void Op64M1(void);
    void Op64M0(void);
    void Op64Slow(void);
    void Op74E1(void);
    void Op74E0M1(void);
    void Op74E0M0(void);
    void Op74Slow(void);
    void Op9CM1(void);
    void Op9CM0(void);
    void Op9CSlow(void);
    void Op9EM1X1(void);
    void Op9EM0X1(void);
    void Op9EM1X0(void);
    void Op9EM0X0(void);
    void Op9ESlow(void);
    void Op14M1(void);
    void Op14M0(void);
    void Op14Slow(void);
    void Op1CM1(void);
    void Op1CM0(void);
    void Op1CSlow(void);
    void Op04M1(void);
    void Op04M0(void);
    void Op04Slow(void);
    void Op0CM1(void);
    void Op0CM0(void);
    void Op0CSlow(void);
    void Op90E0(void);
    void Op90E1(void);
    void Op90Slow(void);
    void OpB0E0(void);
    void OpB0E1(void);
    void OpB0Slow(void);
    void OpF0E0(void);
    void OpF0E1(void);
    void OpF0Slow(void);
    void Op30E0(void);
    void Op30E1(void);
    void Op30Slow(void);
    void OpD0E0(void);
    void OpD0E1(void);
    void OpD0Slow(void);
    void Op10E0(void);
    void Op10E1(void);
    void Op10Slow(void);
    void Op80E0(void);
    void Op80E1(void);
    void Op80Slow(void);
    void Op50E0(void);
    void Op50E1(void);
    void Op50Slow(void);
    void Op70E0(void);
    void Op70E1(void);
    void Op70Slow(void);
    void Op82(void);
    void Op82Slow(void);
    void Op18(void);
    void Op38(void);
    void OpD8(void);
    void OpF8(void);
    void Op58(void);
    void Op78(void);
    void OpB8(void);
    void OpCAX1(void);
    void OpCAX0(void);
    void OpCASlow(void);
    void Op88X1(void);
    void Op88X0(void);
    void Op88Slow(void);
    void OpE8X1(void);
    void OpE8X0(void);
    void OpE8Slow(void);
    void OpC8X1(void);
    void OpC8X0(void);
    void OpC8Slow(void);
    void OpEA(void);
    void OpF4E0(void);
    void OpF4E1(void);
    void OpF4Slow(void);
    void OpD4E0(void);
    void OpD4E1(void);
    void OpD4Slow(void);
    void Op62E0(void);
    void Op62E1(void);
    void Op62Slow(void);
    void Op48E1(void);
    void Op48E0M1(void);
    void Op48E0M0(void);
    void Op48Slow(void);
    void Op8BE1(void);
    void Op8BE0(void);
    void Op8BSlow(void);
    void Op0BE0(void);
    void Op0BE1(void);
    void Op0BSlow(void);
    void Op4BE1(void);
    void Op4BE0(void);
    void Op4BSlow(void);
    void Op08E0(void);
    void Op08E1(void);
    void Op08Slow(void);
    void OpDAE1(void);
    void OpDAE0X1(void);
    void OpDAE0X0(void);
    void OpDASlow(void);
    void Op5AE1(void);
    void Op5AE0X1(void);
    void Op5AE0X0(void);
    void Op5ASlow(void);
    void Op68E1(void);
    void Op68E0M1(void);
    void Op68E0M0(void);
    void Op68Slow(void);
    void OpABE1(void);
    void OpABE0(void);
    void OpABSlow(void);
    void Op2BE0(void);
    void Op2BE1(void);
    void Op2BSlow(void);
    void Op28E1(void);
    void Op28E0(void);
    void Op28Slow(void);
    void OpFAE1(void);
    void OpFAE0X1(void);
    void OpFAE0X0(void);
    void OpFASlow(void);
    void Op7AE1(void);
    void Op7AE0X1(void);
    void Op7AE0X0(void);
    void Op7ASlow(void);
    void OpAAX1(void);
    void OpAAX0(void);
    void OpAASlow(void);
    void OpA8X1(void);
    void OpA8X0(void);
    void OpA8Slow(void);
    void Op5B(void);
    void Op1B(void);
    void Op7B(void);
    void Op3B(void);
    void OpBAX1(void);
    void OpBAX0(void);
    void OpBASlow(void);
    void Op8AM1(void);
    void Op8AM0(void);
    void Op8ASlow(void);
    void Op9A(void);
    void Op9BX1(void);
    void Op9BX0(void);
    void Op9BSlow(void);
    void Op98M1(void);
    void Op98M0(void);
    void Op98Slow(void);
    void OpBBX1(void);
    void OpBBX0(void);
    void OpBBSlow(void);
    void OpFB(void);
    void Op00(void);
    void Op02(void);
    void OpDC(void);
    void OpDCSlow(void);
    void Op5C(void);
    void Op5CSlow(void);
    void Op4C(void);
    void Op4CSlow(void);
    void Op6C(void);
    void Op6CSlow(void);
    void Op7C(void);
    void Op7CSlow(void);
    void Op22E1(void);
    void Op22E0(void);
    void Op22Slow(void);
    void Op6BE1(void);
    void Op6BE0(void);
    void Op6BSlow(void);
    void Op20E1(void);
    void Op20E0(void);
    void Op20Slow(void);
    void OpFCE1(void);
    void OpFCE0(void);
    void OpFCSlow(void);
    void Op60E1(void);
    void Op60E0(void);
    void Op60Slow(void);
    void Op54X1(void);
    void Op54X0(void);
    void Op54Slow(void);
    void Op44X1(void);
    void Op44X0(void);
    void Op44Slow(void);
    void OpC2(void);
    void OpC2Slow(void);
    void OpE2(void);
    void OpE2Slow(void);
    void OpEB(void);
    void Op40Slow(void);
    void OpCB(void);
    void OpDB(void);
    void Op42(void);

    uint8  Immediate8Slow(AccessMode a);
    uint8  Immediate8(AccessMode a);
    uint16 Immediate16Slow(AccessMode a);
    uint16 Immediate16(AccessMode a);
    uint32 RelativeSlow(AccessMode a);
    uint32 Relative(AccessMode a);
    uint32 RelativeLongSlow(AccessMode a);
    uint32 RelativeLong(AccessMode a);
    uint32 AbsoluteIndexedIndirectSlow(AccessMode a);
    uint32 AbsoluteIndexedIndirect(AccessMode a);
    uint32 AbsoluteIndirectLongSlow(AccessMode a);
    uint32 AbsoluteIndirectLong(AccessMode a);
    uint32 AbsoluteIndirectSlow(AccessMode a);
    uint32 AbsoluteIndirect(AccessMode a);
    uint32 AbsoluteSlow(AccessMode a);
    uint32 Absolute(AccessMode a);
    uint32 AbsoluteLongSlow(AccessMode a);
    uint32 AbsoluteLong(AccessMode a);
    uint32 DirectSlow(AccessMode a);
    uint32 Direct(AccessMode a);
    uint32 DirectIndirectSlow(AccessMode a);
    uint32 DirectIndirectE0(AccessMode a);
    uint32 DirectIndirectE1(AccessMode a);
    uint32 DirectIndirectIndexedSlow(AccessMode a);
    uint32 DirectIndirectIndexedE0X0(AccessMode a);
    uint32 DirectIndirectIndexedE0X1(AccessMode a);
    uint32 DirectIndirectIndexedE1(AccessMode a);
    uint32 DirectIndirectLongSlow(AccessMode a);
    uint32 DirectIndirectLong(AccessMode a);
    uint32 DirectIndirectIndexedLongSlow(AccessMode a);
    uint32 DirectIndirectIndexedLong(AccessMode a);
    uint32 DirectIndexedXSlow(AccessMode a);
    uint32 DirectIndexedXE0(AccessMode a);
    uint32 DirectIndexedXE1(AccessMode a);
    uint32 DirectIndexedYSlow(AccessMode a);
    uint32 DirectIndexedYE0(AccessMode a);
    uint32 DirectIndexedYE1(AccessMode a);
    uint32 DirectIndexedIndirectSlow(AccessMode a);
    uint32 DirectIndexedIndirectE0(AccessMode a);
    uint32 DirectIndexedIndirectE1(AccessMode a);
    uint32 AbsoluteIndexedXSlow(AccessMode a);
    uint32 AbsoluteIndexedXX0(AccessMode a);
    uint32 AbsoluteIndexedXX1(AccessMode a);
    uint32 AbsoluteIndexedYSlow(AccessMode a);
    uint32 AbsoluteIndexedYX0(AccessMode a);
    uint32 AbsoluteIndexedYX1(AccessMode a);
    uint32 AbsoluteLongIndexedXSlow(AccessMode a);
    uint32 AbsoluteLongIndexedX(AccessMode a);
    uint32 StackRelativeSlow(AccessMode a);
    uint32 StackRelative(AccessMode a);
    uint32 StackRelativeIndirectIndexedSlow(AccessMode a);
    uint32 StackRelativeIndirectIndexed(AccessMode a);


    bool CheckEmulation();
    void S9xFixCycles();

  public:
    uint8 S9xOpLengthsM0X0[256] = {
        2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 3, 2, 4, 2, 2,
        2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 1, 2, 2, 2, 3, 2, 2, 2, 1, 3,
        1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 4, 3, 3, 4, 1, 2, 3, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3,
        4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 3, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2,
        2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 3, 2, 3, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1,
        3, 1, 1, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3,
        3, 4, 3, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4};
    uint8 S9xOpLengthsM0X1[256] = {
        2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 3, 2, 4, 2, 2,
        2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 1, 2, 2, 2, 3, 2, 2, 2, 1, 3,
        1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 4, 3, 3, 4, 1, 2, 3, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3,
        4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 3, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2,
        2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1,
        3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3,
        3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4};
    uint8 S9xOpLengthsM1X0[256] = {
        2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 3, 2, 4, 2, 2,
        2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 1, 2, 2, 2, 3, 2, 2, 2, 1, 2,
        1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 4, 3, 3, 4, 1, 2, 3, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3,
        4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 3, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2,
        2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 3, 2, 3, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1,
        3, 1, 1, 3, 3, 3, 4, 3, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3,
        3, 4, 3, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4};
    uint8 S9xOpLengthsM1X1[256] = {
        2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 3, 2, 4, 2, 2,
        2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 1, 2, 2, 2, 3, 2, 2, 2, 1, 2,
        1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 4, 3, 3, 4, 1, 2, 3, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3,
        4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 3, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2,
        2, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1,
        3, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 3, 1, 1, 3, 3,
        3, 4, 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 3, 3, 3, 4, 2, 2, 2, 2, 3, 2, 2, 2, 1, 3, 1, 1, 3, 3, 3, 4};


    //
};
#endif
