#include "cpu.h"

void CPU::makeopcodetable()
{
    for (int c = 0; c < 256; c++) {
        for (int d = 0; d < 5; d++) {
            opcodes[c][d] = &CPU::badopcode;
        }
    }

    /* LDA group */
    opcodes[0xA9][0] = opcodes[0xA9][2] = opcodes[0xA9][4] = &CPU::ldaImm8;
    opcodes[0xA9][1] = opcodes[0xA9][3] = &CPU::ldaImm16;
    opcodes[0xA5][0] = opcodes[0xA5][2] = opcodes[0xA5][4] = &CPU::ldaZp8;
    opcodes[0xA5][1] = opcodes[0xA5][3] = &CPU::ldaZp16;
    opcodes[0xB5][0] = opcodes[0xB5][2] = opcodes[0xB5][4] = &CPU::ldaZpx8;
    opcodes[0xB5][1] = opcodes[0xB5][3] = &CPU::ldaZpx16;
    opcodes[0xA3][0] = opcodes[0xA3][2] = opcodes[0xA3][4] = &CPU::ldaSp8;
    opcodes[0xA3][1] = opcodes[0xA3][3] = &CPU::ldaSp16;
    opcodes[0xB3][0] = opcodes[0xB3][2] = opcodes[0xB3][4] = &CPU::ldaSIndirecty8;
    opcodes[0xB3][1] = opcodes[0xB3][3] = &CPU::ldaSIndirecty16;
    opcodes[0xAD][0] = opcodes[0xAD][2] = opcodes[0xAD][4] = &CPU::ldaAbs8;
    opcodes[0xAD][1] = opcodes[0xAD][3] = &CPU::ldaAbs16;
    opcodes[0xBD][0] = opcodes[0xBD][2] = opcodes[0xBD][4] = &CPU::ldaAbsx8;
    opcodes[0xBD][1] = opcodes[0xBD][3] = &CPU::ldaAbsx16;
    opcodes[0xB9][0] = opcodes[0xB9][2] = opcodes[0xB9][4] = &CPU::ldaAbsy8;
    opcodes[0xB9][1] = opcodes[0xB9][3] = &CPU::ldaAbsy16;
    opcodes[0xAF][0] = opcodes[0xAF][2] = opcodes[0xAF][4] = &CPU::ldaLong8;
    opcodes[0xAF][1] = opcodes[0xAF][3] = &CPU::ldaLong16;
    opcodes[0xBF][0] = opcodes[0xBF][2] = opcodes[0xBF][4] = &CPU::ldaLongx8;
    opcodes[0xBF][1] = opcodes[0xBF][3] = &CPU::ldaLongx16;
    opcodes[0xB2][0] = opcodes[0xB2][2] = opcodes[0xB2][4] = &CPU::ldaIndirect8;
    opcodes[0xB2][1] = opcodes[0xB2][3] = &CPU::ldaIndirect16;
    opcodes[0xA1][0] = opcodes[0xA1][2] = opcodes[0xA1][4] = &CPU::ldaIndirectx8;
    opcodes[0xA1][1] = opcodes[0xA1][3] = &CPU::ldaIndirectx16;
    opcodes[0xB1][0] = opcodes[0xB1][2] = opcodes[0xB1][4] = &CPU::ldaIndirecty8;
    opcodes[0xB1][1] = opcodes[0xB1][3] = &CPU::ldaIndirecty16;
    opcodes[0xA7][0] = opcodes[0xA7][2] = opcodes[0xA7][4] = &CPU::ldaIndirectLong8;
    opcodes[0xA7][1] = opcodes[0xA7][3] = &CPU::ldaIndirectLong16;
    opcodes[0xB7][0] = opcodes[0xB7][2] = opcodes[0xB7][4] = &CPU::ldaIndirectLongy8;
    opcodes[0xB7][1] = opcodes[0xB7][3] = &CPU::ldaIndirectLongy16;
    /* LDX group */
    opcodes[0xA2][0] = opcodes[0xA2][1] = opcodes[0xA2][4] = &CPU::ldxImm8;
    opcodes[0xA2][2] = opcodes[0xA2][3] = &CPU::ldxImm16;
    opcodes[0xA6][0] = opcodes[0xA6][1] = opcodes[0xA6][4] = &CPU::ldxZp8;
    opcodes[0xA6][2] = opcodes[0xA6][3] = &CPU::ldxZp16;
    opcodes[0xB6][0] = opcodes[0xB6][1] = opcodes[0xB6][4] = &CPU::ldxZpy8;
    opcodes[0xB6][2] = opcodes[0xB6][3] = &CPU::ldxZpy16;
    opcodes[0xAE][0] = opcodes[0xAE][1] = opcodes[0xAE][4] = &CPU::ldxAbs8;
    opcodes[0xAE][2] = opcodes[0xAE][3] = &CPU::ldxAbs16;
    opcodes[0xBE][0] = opcodes[0xBE][1] = opcodes[0xBE][4] = &CPU::ldxAbsy8;
    opcodes[0xBE][2] = opcodes[0xBE][3] = &CPU::ldxAbsy16;
    /* LDY group */
    opcodes[0xA0][0] = opcodes[0xA0][1] = opcodes[0xA0][4] = &CPU::ldyImm8;
    opcodes[0xA0][2] = opcodes[0xA0][3] = &CPU::ldyImm16;
    opcodes[0xA4][0] = opcodes[0xA4][1] = opcodes[0xA4][4] = &CPU::ldyZp8;
    opcodes[0xA4][2] = opcodes[0xA4][3] = &CPU::ldyZp16;
    opcodes[0xB4][0] = opcodes[0xB4][1] = opcodes[0xB4][4] = &CPU::ldyZpx8;
    opcodes[0xB4][2] = opcodes[0xB4][3] = &CPU::ldyZpx16;
    opcodes[0xAC][0] = opcodes[0xAC][1] = opcodes[0xAC][4] = &CPU::ldyAbs8;
    opcodes[0xAC][2] = opcodes[0xAC][3] = &CPU::ldyAbs16;
    opcodes[0xBC][0] = opcodes[0xBC][1] = opcodes[0xBC][4] = &CPU::ldyAbsx8;
    opcodes[0xBC][2] = opcodes[0xBC][3] = &CPU::ldyAbsx16;

    /* STA group */
    opcodes[0x85][0] = opcodes[0x85][2] = opcodes[0x85][4] = &CPU::staZp8;
    opcodes[0x85][1] = opcodes[0x85][3] = &CPU::staZp16;
    opcodes[0x95][0] = opcodes[0x95][2] = opcodes[0x95][4] = &CPU::staZpx8;
    opcodes[0x95][1] = opcodes[0x95][3] = &CPU::staZpx16;
    opcodes[0x8D][0] = opcodes[0x8D][2] = opcodes[0x8D][4] = &CPU::staAbs8;
    opcodes[0x8D][1] = opcodes[0x8D][3] = &CPU::staAbs16;
    opcodes[0x9D][0] = opcodes[0x9D][2] = opcodes[0x9D][4] = &CPU::staAbsx8;
    opcodes[0x9D][1] = opcodes[0x9D][3] = &CPU::staAbsx16;
    opcodes[0x99][0] = opcodes[0x99][2] = opcodes[0x99][4] = &CPU::staAbsy8;
    opcodes[0x99][1] = opcodes[0x99][3] = &CPU::staAbsy16;
    opcodes[0x8F][0] = opcodes[0x8F][2] = opcodes[0x8F][4] = &CPU::staLong8;
    opcodes[0x8F][1] = opcodes[0x8F][3] = &CPU::staLong16;
    opcodes[0x9F][0] = opcodes[0x9F][2] = opcodes[0x9F][4] = &CPU::staLongx8;
    opcodes[0x9F][1] = opcodes[0x9F][3] = &CPU::staLongx16;
    opcodes[0x92][0] = opcodes[0x92][2] = opcodes[0x92][4] = &CPU::staIndirect8;
    opcodes[0x92][1] = opcodes[0x92][3] = &CPU::staIndirect16;
    opcodes[0x81][0] = opcodes[0x81][2] = opcodes[0x81][4] = &CPU::staIndirectx8;
    opcodes[0x81][1] = opcodes[0x81][3] = &CPU::staIndirectx16;
    opcodes[0x91][0] = opcodes[0x91][2] = opcodes[0x91][4] = &CPU::staIndirecty8;
    opcodes[0x91][1] = opcodes[0x91][3] = &CPU::staIndirecty16;
    opcodes[0x87][0] = opcodes[0x87][2] = opcodes[0x87][4] = &CPU::staIndirectLong8;
    opcodes[0x87][1] = opcodes[0x87][3] = &CPU::staIndirectLong16;
    opcodes[0x97][0] = opcodes[0x97][2] = opcodes[0x97][4] = &CPU::staIndirectLongy8;
    opcodes[0x97][1] = opcodes[0x97][3] = &CPU::staIndirectLongy16;
    opcodes[0x83][0] = opcodes[0x83][2] = opcodes[0x83][4] = &CPU::staSp8;
    opcodes[0x83][1] = opcodes[0x83][3] = &CPU::staSp16;
    opcodes[0x93][0] = opcodes[0x93][2] = opcodes[0x93][4] = &CPU::staSIndirecty8;
    opcodes[0x93][1] = opcodes[0x93][3] = &CPU::staSIndirecty16;
    /* STX group */
    opcodes[0x86][0] = opcodes[0x86][1] = opcodes[0x86][4] = &CPU::stxZp8;
    opcodes[0x86][2] = opcodes[0x86][3] = &CPU::stxZp16;
    opcodes[0x96][0] = opcodes[0x96][1] = opcodes[0x96][4] = &CPU::stxZpy8;
    opcodes[0x96][2] = opcodes[0x96][3] = &CPU::stxZpy16;
    opcodes[0x8E][0] = opcodes[0x8E][1] = opcodes[0x8E][4] = &CPU::stxAbs8;
    opcodes[0x8E][2] = opcodes[0x8E][3] = &CPU::stxAbs16;
    /* STY group */
    opcodes[0x84][0] = opcodes[0x84][1] = opcodes[0x84][4] = &CPU::styZp8;
    opcodes[0x84][2] = opcodes[0x84][3] = &CPU::styZp16;
    opcodes[0x94][0] = opcodes[0x94][1] = opcodes[0x94][4] = &CPU::styZpx8;
    opcodes[0x94][2] = opcodes[0x94][3] = &CPU::styZpx16;
    opcodes[0x8C][0] = opcodes[0x8C][1] = opcodes[0x8C][4] = &CPU::styAbs8;
    opcodes[0x8C][2] = opcodes[0x8C][3] = &CPU::styAbs16;
    /* STZ group */
    opcodes[0x64][0] = opcodes[0x64][2] = opcodes[0x64][4] = &CPU::stzZp8;
    opcodes[0x64][1] = opcodes[0x64][3] = &CPU::stzZp16;
    opcodes[0x74][0] = opcodes[0x74][2] = opcodes[0x74][4] = &CPU::stzZpx8;
    opcodes[0x74][1] = opcodes[0x74][3] = &CPU::stzZpx16;
    opcodes[0x9C][0] = opcodes[0x9C][2] = opcodes[0x9C][4] = &CPU::stzAbs8;
    opcodes[0x9C][1] = opcodes[0x9C][3] = &CPU::stzAbs16;
    opcodes[0x9E][0] = opcodes[0x9E][2] = opcodes[0x9E][4] = &CPU::stzAbsx8;
    opcodes[0x9E][1] = opcodes[0x9E][3] = &CPU::stzAbsx16;

    opcodes[0x3A][0] = opcodes[0x3A][2] = opcodes[0x3A][4] = &CPU::deca8;
    opcodes[0x3A][1] = opcodes[0x3A][3] = &CPU::deca16;
    opcodes[0xCA][0] = opcodes[0xCA][1] = opcodes[0xCA][4] = &CPU::dex8;
    opcodes[0xCA][2] = opcodes[0xCA][3] = &CPU::dex16;
    opcodes[0x88][0] = opcodes[0x88][1] = opcodes[0x88][4] = &CPU::dey8;
    opcodes[0x88][2] = opcodes[0x88][3] = &CPU::dey16;
    opcodes[0x1A][0] = opcodes[0x1A][2] = opcodes[0x1A][4] = &CPU::inca8;
    opcodes[0x1A][1] = opcodes[0x1A][3] = &CPU::inca16;
    opcodes[0xE8][0] = opcodes[0xE8][1] = opcodes[0xE8][4] = &CPU::inx8;
    opcodes[0xE8][2] = opcodes[0xE8][3] = &CPU::inx16;
    opcodes[0xC8][0] = opcodes[0xC8][1] = opcodes[0xC8][4] = &CPU::iny8;
    opcodes[0xC8][2] = opcodes[0xC8][3] = &CPU::iny16;

    /* INC group */
    opcodes[0xE6][0] = opcodes[0xE6][2] = opcodes[0xE6][4] = &CPU::incZp8;
    opcodes[0xE6][1] = opcodes[0xE6][3] = &CPU::incZp16;
    opcodes[0xF6][0] = opcodes[0xF6][2] = opcodes[0xF6][4] = &CPU::incZpx8;
    opcodes[0xF6][1] = opcodes[0xF6][3] = &CPU::incZpx16;
    opcodes[0xEE][0] = opcodes[0xEE][2] = opcodes[0xEE][4] = &CPU::incAbs8;
    opcodes[0xEE][1] = opcodes[0xEE][3] = &CPU::incAbs16;
    opcodes[0xFE][0] = opcodes[0xFE][2] = opcodes[0xFE][4] = &CPU::incAbsx8;
    opcodes[0xFE][1] = opcodes[0xFE][3] = &CPU::incAbsx16;

    /* DEC group */
    opcodes[0xC6][0] = opcodes[0xC6][2] = opcodes[0xC6][4] = &CPU::decZp8;
    opcodes[0xC6][1] = opcodes[0xC6][3] = &CPU::decZp16;
    opcodes[0xD6][0] = opcodes[0xD6][2] = opcodes[0xD6][4] = &CPU::decZpx8;
    opcodes[0xD6][1] = opcodes[0xD6][3] = &CPU::decZpx16;
    opcodes[0xCE][0] = opcodes[0xCE][2] = opcodes[0xCE][4] = &CPU::decAbs8;
    opcodes[0xCE][1] = opcodes[0xCE][3] = &CPU::decAbs16;
    opcodes[0xDE][0] = opcodes[0xDE][2] = opcodes[0xDE][4] = &CPU::decAbsx8;
    opcodes[0xDE][1] = opcodes[0xDE][3] = &CPU::decAbsx16;

    /* AND group */
    opcodes[0x29][0] = opcodes[0x29][2] = opcodes[0x29][4] = &CPU::andImm8;
    opcodes[0x29][1] = opcodes[0x29][3] = &CPU::andImm16;
    opcodes[0x25][0] = opcodes[0x25][2] = opcodes[0x25][4] = &CPU::andZp8;
    opcodes[0x25][1] = opcodes[0x25][3] = &CPU::andZp16;
    opcodes[0x35][0] = opcodes[0x35][2] = opcodes[0x35][4] = &CPU::andZpx8;
    opcodes[0x35][1] = opcodes[0x35][3] = &CPU::andZpx16;
    opcodes[0x23][0] = opcodes[0x23][2] = opcodes[0x23][4] = &CPU::andSp8;
    opcodes[0x23][1] = opcodes[0x23][3] = &CPU::andSp16;
    opcodes[0x2D][0] = opcodes[0x2D][2] = opcodes[0x2D][4] = &CPU::andAbs8;
    opcodes[0x2D][1] = opcodes[0x2D][3] = &CPU::andAbs16;
    opcodes[0x3D][0] = opcodes[0x3D][2] = opcodes[0x3D][4] = &CPU::andAbsx8;
    opcodes[0x3D][1] = opcodes[0x3D][3] = &CPU::andAbsx16;
    opcodes[0x39][0] = opcodes[0x39][2] = opcodes[0x39][4] = &CPU::andAbsy8;
    opcodes[0x39][1] = opcodes[0x39][3] = &CPU::andAbsy16;
    opcodes[0x2F][0] = opcodes[0x2F][2] = opcodes[0x2F][4] = &CPU::andLong8;
    opcodes[0x2F][1] = opcodes[0x2F][3] = &CPU::andLong16;
    opcodes[0x3F][0] = opcodes[0x3F][2] = opcodes[0x3F][4] = &CPU::andLongx8;
    opcodes[0x3F][1] = opcodes[0x3F][3] = &CPU::andLongx16;
    opcodes[0x32][0] = opcodes[0x32][2] = opcodes[0x32][4] = &CPU::andIndirect8;
    opcodes[0x32][1] = opcodes[0x32][3] = &CPU::andIndirect16;
    opcodes[0x21][0] = opcodes[0x21][2] = opcodes[0x21][4] = &CPU::andIndirectx8;
    opcodes[0x21][1] = opcodes[0x21][3] = &CPU::andIndirectx16;
    opcodes[0x31][0] = opcodes[0x31][2] = opcodes[0x31][4] = &CPU::andIndirecty8;
    opcodes[0x31][1] = opcodes[0x31][3] = &CPU::andIndirecty16;
    opcodes[0x27][0] = opcodes[0x27][2] = opcodes[0x27][4] = &CPU::andIndirectLong8;
    opcodes[0x27][1] = opcodes[0x27][3] = &CPU::andIndirectLong16;
    opcodes[0x37][0] = opcodes[0x37][2] = opcodes[0x37][4] = &CPU::andIndirectLongy8;
    opcodes[0x37][1] = opcodes[0x37][3] = &CPU::andIndirectLongy16;

    /* EOR group */
    opcodes[0x49][0] = opcodes[0x49][2] = opcodes[0x49][4] = &CPU::eorImm8;
    opcodes[0x49][1] = opcodes[0x49][3] = &CPU::eorImm16;
    opcodes[0x45][0] = opcodes[0x45][2] = opcodes[0x45][4] = &CPU::eorZp8;
    opcodes[0x45][1] = opcodes[0x45][3] = &CPU::eorZp16;
    opcodes[0x55][0] = opcodes[0x55][2] = opcodes[0x55][4] = &CPU::eorZpx8;
    opcodes[0x55][1] = opcodes[0x55][3] = &CPU::eorZpx16;
    opcodes[0x43][0] = opcodes[0x43][2] = opcodes[0x43][4] = &CPU::eorSp8;
    opcodes[0x43][1] = opcodes[0x43][3] = &CPU::eorSp16;
    opcodes[0x4D][0] = opcodes[0x4D][2] = opcodes[0x4D][4] = &CPU::eorAbs8;
    opcodes[0x4D][1] = opcodes[0x4D][3] = &CPU::eorAbs16;
    opcodes[0x5D][0] = opcodes[0x5D][2] = opcodes[0x5D][4] = &CPU::eorAbsx8;
    opcodes[0x5D][1] = opcodes[0x5D][3] = &CPU::eorAbsx16;
    opcodes[0x59][0] = opcodes[0x59][2] = opcodes[0x59][4] = &CPU::eorAbsy8;
    opcodes[0x59][1] = opcodes[0x59][3] = &CPU::eorAbsy16;
    opcodes[0x4F][0] = opcodes[0x4F][2] = opcodes[0x4F][4] = &CPU::eorLong8;
    opcodes[0x4F][1] = opcodes[0x4F][3] = &CPU::eorLong16;
    opcodes[0x5F][0] = opcodes[0x5F][2] = opcodes[0x5F][4] = &CPU::eorLongx8;
    opcodes[0x5F][1] = opcodes[0x5F][3] = &CPU::eorLongx16;
    opcodes[0x52][0] = opcodes[0x52][2] = opcodes[0x52][4] = &CPU::eorIndirect8;
    opcodes[0x52][1] = opcodes[0x52][3] = &CPU::eorIndirect16;
    opcodes[0x41][0] = opcodes[0x41][2] = opcodes[0x41][4] = &CPU::eorIndirectx8;
    opcodes[0x41][1] = opcodes[0x41][3] = &CPU::eorIndirectx16;
    opcodes[0x51][0] = opcodes[0x51][2] = opcodes[0x51][4] = &CPU::eorIndirecty8;
    opcodes[0x51][1] = opcodes[0x51][3] = &CPU::eorIndirecty16;
    opcodes[0x47][0] = opcodes[0x47][2] = opcodes[0x47][4] = &CPU::eorIndirectLong8;
    opcodes[0x47][1] = opcodes[0x47][3] = &CPU::eorIndirectLong16;
    opcodes[0x57][0] = opcodes[0x57][2] = opcodes[0x57][4] = &CPU::eorIndirectLongy8;
    opcodes[0x57][1] = opcodes[0x57][3] = &CPU::eorIndirectLongy16;

    /* ORA group */
    opcodes[0x09][0] = opcodes[0x09][2] = opcodes[0x09][4] = &CPU::oraImm8;
    opcodes[0x09][1] = opcodes[0x09][3] = &CPU::oraImm16;
    opcodes[0x05][0] = opcodes[0x05][2] = opcodes[0x05][4] = &CPU::oraZp8;
    opcodes[0x05][1] = opcodes[0x05][3] = &CPU::oraZp16;
    opcodes[0x15][0] = opcodes[0x15][2] = opcodes[0x15][4] = &CPU::oraZpx8;
    opcodes[0x15][1] = opcodes[0x15][3] = &CPU::oraZpx16;
    opcodes[0x03][0] = opcodes[0x03][2] = opcodes[0x03][4] = &CPU::oraSp8;
    opcodes[0x03][1] = opcodes[0x03][3] = &CPU::oraSp16;
    opcodes[0x0D][0] = opcodes[0x0D][2] = opcodes[0x0D][4] = &CPU::oraAbs8;
    opcodes[0x0D][1] = opcodes[0x0D][3] = &CPU::oraAbs16;
    opcodes[0x1D][0] = opcodes[0x1D][2] = opcodes[0x1D][4] = &CPU::oraAbsx8;
    opcodes[0x1D][1] = opcodes[0x1D][3] = &CPU::oraAbsx16;
    opcodes[0x19][0] = opcodes[0x19][2] = opcodes[0x19][4] = &CPU::oraAbsy8;
    opcodes[0x19][1] = opcodes[0x19][3] = &CPU::oraAbsy16;
    opcodes[0x0F][0] = opcodes[0x0F][2] = opcodes[0x0F][4] = &CPU::oraLong8;
    opcodes[0x0F][1] = opcodes[0x0F][3] = &CPU::oraLong16;
    opcodes[0x1F][0] = opcodes[0x1F][2] = opcodes[0x1F][4] = &CPU::oraLongx8;
    opcodes[0x1F][1] = opcodes[0x1F][3] = &CPU::oraLongx16;
    opcodes[0x12][0] = opcodes[0x12][2] = opcodes[0x12][4] = &CPU::oraIndirect8;
    opcodes[0x12][1] = opcodes[0x12][3] = &CPU::oraIndirect16;
    opcodes[0x01][0] = opcodes[0x01][2] = opcodes[0x01][4] = &CPU::oraIndirectx8;
    opcodes[0x01][1] = opcodes[0x01][3] = &CPU::oraIndirectx16;
    opcodes[0x11][0] = opcodes[0x11][2] = opcodes[0x11][4] = &CPU::oraIndirecty8;
    opcodes[0x11][1] = opcodes[0x11][3] = &CPU::oraIndirecty16;
    opcodes[0x07][0] = opcodes[0x07][2] = opcodes[0x07][4] = &CPU::oraIndirectLong8;
    opcodes[0x07][1] = opcodes[0x07][3] = &CPU::oraIndirectLong16;
    opcodes[0x17][0] = opcodes[0x17][2] = opcodes[0x17][4] = &CPU::oraIndirectLongy8;
    opcodes[0x17][1] = opcodes[0x17][3] = &CPU::oraIndirectLongy16;

    /* ADC group */
    opcodes[0x69][0] = opcodes[0x69][2] = opcodes[0x69][4] = &CPU::adcImm8;
    opcodes[0x69][1] = opcodes[0x69][3] = &CPU::adcImm16;
    opcodes[0x65][0] = opcodes[0x65][2] = opcodes[0x65][4] = &CPU::adcZp8;
    opcodes[0x65][1] = opcodes[0x65][3] = &CPU::adcZp16;
    opcodes[0x75][0] = opcodes[0x75][2] = opcodes[0x75][4] = &CPU::adcZpx8;
    opcodes[0x75][1] = opcodes[0x75][3] = &CPU::adcZpx16;
    opcodes[0x63][0] = opcodes[0x63][2] = opcodes[0x63][4] = &CPU::adcSp8;
    opcodes[0x63][1] = opcodes[0x63][3] = &CPU::adcSp16;
    opcodes[0x6D][0] = opcodes[0x6D][2] = opcodes[0x6D][4] = &CPU::adcAbs8;
    opcodes[0x6D][1] = opcodes[0x6D][3] = &CPU::adcAbs16;
    opcodes[0x7D][0] = opcodes[0x7D][2] = opcodes[0x7D][4] = &CPU::adcAbsx8;
    opcodes[0x7D][1] = opcodes[0x7D][3] = &CPU::adcAbsx16;
    opcodes[0x79][0] = opcodes[0x79][2] = opcodes[0x79][4] = &CPU::adcAbsy8;
    opcodes[0x79][1] = opcodes[0x79][3] = &CPU::adcAbsy16;
    opcodes[0x6F][0] = opcodes[0x6F][2] = opcodes[0x6F][4] = &CPU::adcLong8;
    opcodes[0x6F][1] = opcodes[0x6F][3] = &CPU::adcLong16;
    opcodes[0x7F][0] = opcodes[0x7F][2] = opcodes[0x7F][4] = &CPU::adcLongx8;
    opcodes[0x7F][1] = opcodes[0x7F][3] = &CPU::adcLongx16;
    opcodes[0x72][0] = opcodes[0x72][2] = opcodes[0x72][4] = &CPU::adcIndirect8;
    opcodes[0x72][1] = opcodes[0x72][3] = &CPU::adcIndirect16;
    opcodes[0x61][0] = opcodes[0x61][2] = opcodes[0x61][4] = &CPU::adcIndirectx8;
    opcodes[0x61][1] = opcodes[0x61][3] = &CPU::adcIndirectx16;
    opcodes[0x71][0] = opcodes[0x71][2] = opcodes[0x71][4] = &CPU::adcIndirecty8;
    opcodes[0x71][1] = opcodes[0x71][3] = &CPU::adcIndirecty16;
    opcodes[0x73][0] = opcodes[0x73][2] = opcodes[0x73][4] = &CPU::adcsIndirecty8;
    opcodes[0x73][1] = opcodes[0x73][3] = &CPU::adcsIndirecty16;
    opcodes[0x67][0] = opcodes[0x67][2] = opcodes[0x67][4] = &CPU::adcIndirectLong8;
    opcodes[0x67][1] = opcodes[0x67][3] = &CPU::adcIndirectLong16;
    opcodes[0x77][0] = opcodes[0x77][2] = opcodes[0x77][4] = &CPU::adcIndirectLongy8;
    opcodes[0x77][1] = opcodes[0x77][3] = &CPU::adcIndirectLongy16;

    /* SBC group */
    opcodes[0xE9][0] = opcodes[0xE9][2] = opcodes[0xE9][4] = &CPU::sbcImm8;
    opcodes[0xE9][1] = opcodes[0xE9][3] = &CPU::sbcImm16;
    opcodes[0xE5][0] = opcodes[0xE5][2] = opcodes[0xE5][4] = &CPU::sbcZp8;
    opcodes[0xE5][1] = opcodes[0xE5][3] = &CPU::sbcZp16;
    opcodes[0xE3][0] = opcodes[0xE3][2] = opcodes[0xE3][4] = &CPU::sbcSp8;
    opcodes[0xE3][1] = opcodes[0xE3][3] = &CPU::sbcSp16;
    opcodes[0xF5][0] = opcodes[0xF5][2] = opcodes[0xF5][4] = &CPU::sbcZpx8;
    opcodes[0xF5][1] = opcodes[0xF5][3] = &CPU::sbcZpx16;
    opcodes[0xED][0] = opcodes[0xED][2] = opcodes[0xED][4] = &CPU::sbcAbs8;
    opcodes[0xED][1] = opcodes[0xED][3] = &CPU::sbcAbs16;
    opcodes[0xFD][0] = opcodes[0xFD][2] = opcodes[0xFD][4] = &CPU::sbcAbsx8;
    opcodes[0xFD][1] = opcodes[0xFD][3] = &CPU::sbcAbsx16;
    opcodes[0xF9][0] = opcodes[0xF9][2] = opcodes[0xF9][4] = &CPU::sbcAbsy8;
    opcodes[0xF9][1] = opcodes[0xF9][3] = &CPU::sbcAbsy16;
    opcodes[0xEF][0] = opcodes[0xEF][2] = opcodes[0xEF][4] = &CPU::sbcLong8;
    opcodes[0xEF][1] = opcodes[0xEF][3] = &CPU::sbcLong16;
    opcodes[0xFF][0] = opcodes[0xFF][2] = opcodes[0xFF][4] = &CPU::sbcLongx8;
    opcodes[0xFF][1] = opcodes[0xFF][3] = &CPU::sbcLongx16;
    opcodes[0xF2][0] = opcodes[0xF2][2] = opcodes[0xF2][4] = &CPU::sbcIndirect8;
    opcodes[0xF2][1] = opcodes[0xF2][3] = &CPU::sbcIndirect16;
    opcodes[0xE1][0] = opcodes[0xE1][2] = opcodes[0xE1][4] = &CPU::sbcIndirectx8;
    opcodes[0xE1][1] = opcodes[0xE1][3] = &CPU::sbcIndirectx16;
    opcodes[0xF1][0] = opcodes[0xF1][2] = opcodes[0xF1][4] = &CPU::sbcIndirecty8;
    opcodes[0xF1][1] = opcodes[0xF1][3] = &CPU::sbcIndirecty16;
    opcodes[0xE7][0] = opcodes[0xE7][2] = opcodes[0xE7][4] = &CPU::sbcIndirectLong8;
    opcodes[0xE7][1] = opcodes[0xE7][3] = &CPU::sbcIndirectLong16;
    opcodes[0xF7][0] = opcodes[0xF7][2] = opcodes[0xF7][4] = &CPU::sbcIndirectLongy8;
    opcodes[0xF7][1] = opcodes[0xF7][3] = &CPU::sbcIndirectLongy16;

    /* Transfer group */
    opcodes[0xAA][0] = opcodes[0xAA][1] = opcodes[0xAA][4] = &CPU::tax8;
    opcodes[0xAA][2] = opcodes[0xAA][3] = &CPU::tax16;
    opcodes[0xA8][0] = opcodes[0xA8][1] = opcodes[0xA8][4] = &CPU::tay8;
    opcodes[0xA8][2] = opcodes[0xA8][3] = &CPU::tay16;
    opcodes[0x8A][0] = opcodes[0x8A][2] = opcodes[0x8A][4] = &CPU::txa8;
    opcodes[0x8A][1] = opcodes[0x8A][3] = &CPU::txa16;
    opcodes[0x98][0] = opcodes[0x98][2] = opcodes[0x98][4] = &CPU::tya8;
    opcodes[0x98][1] = opcodes[0x98][3] = &CPU::tya16;
    opcodes[0x9B][0] = opcodes[0x9B][1] = opcodes[0x9B][4] = &CPU::txy8;
    opcodes[0x9B][2] = opcodes[0x9B][3] = &CPU::txy16;
    opcodes[0xBB][0] = opcodes[0xBB][1] = opcodes[0xBB][4] = &CPU::tyx8;
    opcodes[0xBB][2] = opcodes[0xBB][3] = &CPU::tyx16;
    opcodes[0xBA][0] = opcodes[0xBA][1] = opcodes[0xBA][4] = &CPU::tsx8;
    opcodes[0xBA][2] = opcodes[0xBA][3] = &CPU::tsx16;
    opcodes[0x9A][0] = opcodes[0x9A][1] = opcodes[0x9A][4] = &CPU::txs8;
    opcodes[0x9A][2] = opcodes[0x9A][3] = &CPU::txs16;

    /* Flag Group */
    opcodes[0x18][0] = opcodes[0x18][1] = opcodes[0x18][2] = opcodes[0x18][3] = opcodes[0x18][4] = &CPU::clc;
    opcodes[0xD8][0] = opcodes[0xD8][1] = opcodes[0xD8][2] = opcodes[0xD8][3] = opcodes[0xD8][4] = &CPU::cld;
    opcodes[0x58][0] = opcodes[0x58][1] = opcodes[0x58][2] = opcodes[0x58][3] = opcodes[0x58][4] = &CPU::cli;
    opcodes[0xB8][0] = opcodes[0xB8][1] = opcodes[0xB8][2] = opcodes[0xB8][3] = opcodes[0xB8][4] = &CPU::clv;
    opcodes[0x38][0] = opcodes[0x38][1] = opcodes[0x38][2] = opcodes[0x38][3] = opcodes[0x38][4] = &CPU::sec;
    opcodes[0xF8][0] = opcodes[0xF8][1] = opcodes[0xF8][2] = opcodes[0xF8][3] = opcodes[0xF8][4] = &CPU::sed;
    opcodes[0x78][0] = opcodes[0x78][1] = opcodes[0x78][2] = opcodes[0x78][3] = opcodes[0x78][4] = &CPU::sei;
    opcodes[0xFB][0] = opcodes[0xFB][1] = opcodes[0xFB][2] = opcodes[0xFB][3] = opcodes[0xFB][4] = &CPU::xce;
    opcodes[0xE2][0] = opcodes[0xE2][1] = opcodes[0xE2][2] = opcodes[0xE2][3] = opcodes[0xE2][4] = &CPU::sep;
    opcodes[0xC2][0] = opcodes[0xC2][1] = opcodes[0xC2][2] = opcodes[0xC2][3] = opcodes[0xC2][4] = &CPU::rep;

    /* Stack group */
    opcodes[0x8B][0] = opcodes[0x8B][1] = opcodes[0x8B][2] = opcodes[0x8B][3] = &CPU::phb;
    opcodes[0x8B][4]                                                          = &CPU::phbe;
    opcodes[0x4B][0] = opcodes[0x4B][1] = opcodes[0x4B][2] = opcodes[0x4B][3] = &CPU::phk;
    opcodes[0x4B][4]                                                          = &CPU::phke;
    opcodes[0xAB][0] = opcodes[0xAB][1] = opcodes[0xAB][2] = opcodes[0xAB][3] = &CPU::plb;
    opcodes[0xAB][4]                                                          = &CPU::plbe;
    opcodes[0x08][0] = opcodes[0x08][1] = opcodes[0x08][2] = opcodes[0x08][3] = &CPU::php;
    opcodes[0x08][4]                                                          = &CPU::php;
    opcodes[0x28][0] = opcodes[0x28][1] = opcodes[0x28][2] = opcodes[0x28][3] = &CPU::plp;
    opcodes[0x28][4]                                                          = &CPU::plp;
    opcodes[0x48][0] = opcodes[0x48][2] = opcodes[0x48][4] = &CPU::pha8;
    opcodes[0x48][1] = opcodes[0x48][3] = &CPU::pha16;
    opcodes[0xDA][0] = opcodes[0xDA][1] = opcodes[0xDA][4] = &CPU::phx8;
    opcodes[0xDA][2] = opcodes[0xDA][3] = &CPU::phx16;
    opcodes[0x5A][0] = opcodes[0x5A][1] = opcodes[0x5A][4] = &CPU::phy8;
    opcodes[0x5A][2] = opcodes[0x5A][3] = &CPU::phy16;
    opcodes[0x68][0] = opcodes[0x68][2] = opcodes[0x68][4] = &CPU::pla8;
    opcodes[0x68][1] = opcodes[0x68][3] = &CPU::pla16;
    opcodes[0xFA][0] = opcodes[0xFA][1] = opcodes[0xFA][4] = &CPU::plx8;
    opcodes[0xFA][2] = opcodes[0xFA][3] = &CPU::plx16;
    opcodes[0x7A][0] = opcodes[0x7A][1] = opcodes[0x7A][4] = &CPU::ply8;
    opcodes[0x7A][2] = opcodes[0x7A][3] = &CPU::ply16;
    opcodes[0xD4][0] = opcodes[0xD4][1] = opcodes[0xD4][2] = opcodes[0xD4][3] = opcodes[0xD4][4] = &CPU::pei;
    opcodes[0xF4][0] = opcodes[0xF4][1] = opcodes[0xF4][2] = opcodes[0xF4][3] = opcodes[0xF4][4] = &CPU::pea;
    opcodes[0x62][0] = opcodes[0x62][1] = opcodes[0x62][2] = opcodes[0x62][3] = opcodes[0x62][4] = &CPU::per;
    opcodes[0x0B][0] = opcodes[0x0B][1] = opcodes[0x0B][2] = opcodes[0x0B][3] = opcodes[0x0B][4] = &CPU::phd;
    opcodes[0x2B][0] = opcodes[0x2B][1] = opcodes[0x2B][2] = opcodes[0x2B][3] = opcodes[0x2B][4] = &CPU::pld;

    /* CMP group */
    opcodes[0xC9][0] = opcodes[0xC9][2] = opcodes[0xC9][4] = &CPU::cmpImm8;
    opcodes[0xC9][1] = opcodes[0xC9][3] = &CPU::cmpImm16;
    opcodes[0xC5][0] = opcodes[0xC5][2] = opcodes[0xC5][4] = &CPU::cmpZp8;
    opcodes[0xC5][1] = opcodes[0xC5][3] = &CPU::cmpZp16;
    opcodes[0xC3][0] = opcodes[0xC3][2] = opcodes[0xC3][4] = &CPU::cmpSp8;
    opcodes[0xC3][1] = opcodes[0xC3][3] = &CPU::cmpSp16;
    opcodes[0xD5][0] = opcodes[0xD5][2] = opcodes[0xD5][4] = &CPU::cmpZpx8;
    opcodes[0xD5][1] = opcodes[0xD5][3] = &CPU::cmpZpx16;
    opcodes[0xCD][0] = opcodes[0xCD][2] = opcodes[0xCD][4] = &CPU::cmpAbs8;
    opcodes[0xCD][1] = opcodes[0xCD][3] = &CPU::cmpAbs16;
    opcodes[0xDD][0] = opcodes[0xDD][2] = opcodes[0xDD][4] = &CPU::cmpAbsx8;
    opcodes[0xDD][1] = opcodes[0xDD][3] = &CPU::cmpAbsx16;
    opcodes[0xD9][0] = opcodes[0xD9][2] = opcodes[0xD9][4] = &CPU::cmpAbsy8;
    opcodes[0xD9][1] = opcodes[0xD9][3] = &CPU::cmpAbsy16;
    opcodes[0xCF][0] = opcodes[0xCF][2] = opcodes[0xCF][4] = &CPU::cmpLong8;
    opcodes[0xCF][1] = opcodes[0xCF][3] = &CPU::cmpLong16;
    opcodes[0xDF][0] = opcodes[0xDF][2] = opcodes[0xDF][4] = &CPU::cmpLongx8;
    opcodes[0xDF][1] = opcodes[0xDF][3] = &CPU::cmpLongx16;
    opcodes[0xD2][0] = opcodes[0xD2][2] = opcodes[0xD2][4] = &CPU::cmpIndirect8;
    opcodes[0xD2][1] = opcodes[0xD2][3] = &CPU::cmpIndirect16;
    opcodes[0xC1][0] = opcodes[0xC1][2] = opcodes[0xC1][4] = &CPU::cmpIndirectx8;
    opcodes[0xC1][1] = opcodes[0xC1][3] = &CPU::cmpIndirectx16;
    opcodes[0xD1][0] = opcodes[0xD1][2] = opcodes[0xD1][4] = &CPU::cmpIndirecty8;
    opcodes[0xD1][1] = opcodes[0xD1][3] = &CPU::cmpIndirecty16;
    opcodes[0xC7][0] = opcodes[0xC7][2] = opcodes[0xC7][4] = &CPU::cmpIndirectLong8;
    opcodes[0xC7][1] = opcodes[0xC7][3] = &CPU::cmpIndirectLong16;
    opcodes[0xD7][0] = opcodes[0xD7][2] = opcodes[0xD7][4] = &CPU::cmpIndirectLongy8;
    opcodes[0xD7][1] = opcodes[0xD7][3] = &CPU::cmpIndirectLongy16;

    /* CPX group */
    opcodes[0xE0][0] = opcodes[0xE0][1] = opcodes[0xE0][4] = &CPU::cpxImm8;
    opcodes[0xE0][2] = opcodes[0xE0][3] = &CPU::cpxImm16;
    opcodes[0xE4][0] = opcodes[0xE4][1] = opcodes[0xE4][4] = &CPU::cpxZp8;
    opcodes[0xE4][2] = opcodes[0xE4][3] = &CPU::cpxZp16;
    opcodes[0xEC][0] = opcodes[0xEC][1] = opcodes[0xEC][4] = &CPU::cpxAbs8;
    opcodes[0xEC][2] = opcodes[0xEC][3] = &CPU::cpxAbs16;

    /* CPY group */
    opcodes[0xC0][0] = opcodes[0xC0][1] = opcodes[0xC0][4] = &CPU::cpyImm8;
    opcodes[0xC0][2] = opcodes[0xC0][3] = &CPU::cpyImm16;
    opcodes[0xC4][0] = opcodes[0xC4][1] = opcodes[0xC4][4] = &CPU::cpyZp8;
    opcodes[0xC4][2] = opcodes[0xC4][3] = &CPU::cpyZp16;
    opcodes[0xCC][0] = opcodes[0xCC][1] = opcodes[0xCC][4] = &CPU::cpyAbs8;
    opcodes[0xCC][2] = opcodes[0xCC][3] = &CPU::cpyAbs16;

    /* Branch group */
    opcodes[0x90][0] = opcodes[0x90][1] = opcodes[0x90][2] = opcodes[0x90][3] = opcodes[0x90][4] = &CPU::bcc;
    opcodes[0xB0][0] = opcodes[0xB0][1] = opcodes[0xB0][2] = opcodes[0xB0][3] = opcodes[0xB0][4] = &CPU::bcs;
    opcodes[0xF0][0] = opcodes[0xF0][1] = opcodes[0xF0][2] = opcodes[0xF0][3] = opcodes[0xF0][4] = &CPU::beq;
    opcodes[0xD0][0] = opcodes[0xD0][1] = opcodes[0xD0][2] = opcodes[0xD0][3] = opcodes[0xD0][4] = &CPU::bne;
    opcodes[0x80][0] = opcodes[0x80][1] = opcodes[0x80][2] = opcodes[0x80][3] = opcodes[0x80][4] = &CPU::bra;
    opcodes[0x82][0] = opcodes[0x82][1] = opcodes[0x82][2] = opcodes[0x82][3] = opcodes[0x82][4] = &CPU::brl;
    opcodes[0x10][0] = opcodes[0x10][1] = opcodes[0x10][2] = opcodes[0x10][3] = opcodes[0x10][4] = &CPU::bpl;
    opcodes[0x30][0] = opcodes[0x30][1] = opcodes[0x30][2] = opcodes[0x30][3] = opcodes[0x30][4] = &CPU::bmi;
    opcodes[0x50][0] = opcodes[0x50][1] = opcodes[0x50][2] = opcodes[0x50][3] = opcodes[0x50][4] = &CPU::bvc;
    opcodes[0x70][0] = opcodes[0x70][1] = opcodes[0x70][2] = opcodes[0x70][3] = opcodes[0x70][4] = &CPU::bvs;

    /* Jump group */
    opcodes[0x4C][0] = opcodes[0x4C][1] = opcodes[0x4C][2] = opcodes[0x4C][3] = opcodes[0x4C][4] = &CPU::jmp;
    opcodes[0x5C][0] = opcodes[0x5C][1] = opcodes[0x5C][2] = opcodes[0x5C][3] = opcodes[0x5C][4] = &CPU::jmplong;
    opcodes[0x6C][0] = opcodes[0x6C][1] = opcodes[0x6C][2] = opcodes[0x6C][3] = opcodes[0x6C][4] = &CPU::jmpind;
    opcodes[0x7C][0] = opcodes[0x7C][1] = opcodes[0x7C][2] = opcodes[0x7C][3] = opcodes[0x7C][4] = &CPU::jmpindx;
    opcodes[0xDC][0] = opcodes[0xDC][1] = opcodes[0xDC][2] = opcodes[0xDC][3] = opcodes[0xDC][4] = &CPU::jmlind;
    opcodes[0x20][0] = opcodes[0x20][1] = opcodes[0x20][2] = opcodes[0x20][3] = &CPU::jsr;
    opcodes[0x20][4]                                                          = &CPU::jsre;
    opcodes[0xFC][0] = opcodes[0xFC][1] = opcodes[0xFC][2] = opcodes[0xFC][3] = &CPU::jsrIndx;
    opcodes[0xFC][4]                                                          = &CPU::jsrIndxe;
    opcodes[0x60][0] = opcodes[0x60][1] = opcodes[0x60][2] = opcodes[0x60][3] = &CPU::rts;
    opcodes[0x60][4]                                                          = &CPU::rtse;
    opcodes[0x6B][0] = opcodes[0x6B][1] = opcodes[0x6B][2] = opcodes[0x6B][3] = &CPU::rtl;
    opcodes[0x6B][4]                                                          = &CPU::rtle;
    opcodes[0x40][0] = opcodes[0x40][1] = opcodes[0x40][2] = opcodes[0x40][3] = opcodes[0x40][4] = &CPU::rti;
    opcodes[0x22][0] = opcodes[0x22][1] = opcodes[0x22][2] = opcodes[0x22][3] = &CPU::jsl;
    opcodes[0x22][4]                                                          = &CPU::jsle;

    /* Shift group */
    opcodes[0x0A][0] = opcodes[0x0A][2] = opcodes[0x0A][4] = &CPU::asla8;
    opcodes[0x0A][1] = opcodes[0x0A][3] = &CPU::asla16;
    opcodes[0x06][0] = opcodes[0x06][2] = opcodes[0x06][4] = &CPU::aslZp8;
    opcodes[0x06][1] = opcodes[0x06][3] = &CPU::aslZp16;
    opcodes[0x16][0] = opcodes[0x16][2] = opcodes[0x16][4] = &CPU::aslZpx8;
    opcodes[0x16][1] = opcodes[0x16][3] = &CPU::aslZpx16;
    opcodes[0x0E][0] = opcodes[0x0E][2] = opcodes[0x0E][4] = &CPU::aslAbs8;
    opcodes[0x0E][1] = opcodes[0x0E][3] = &CPU::aslAbs16;
    opcodes[0x1E][0] = opcodes[0x1E][2] = opcodes[0x1E][4] = &CPU::aslAbsx8;
    opcodes[0x1E][1] = opcodes[0x1E][3] = &CPU::aslAbsx16;

    opcodes[0x4A][0] = opcodes[0x4A][2] = opcodes[0x4A][4] = &CPU::lsra8;
    opcodes[0x4A][1] = opcodes[0x4A][3] = &CPU::lsra16;
    opcodes[0x46][0] = opcodes[0x46][2] = opcodes[0x46][4] = &CPU::lsrZp8;
    opcodes[0x46][1] = opcodes[0x46][3] = &CPU::lsrZp16;
    opcodes[0x56][0] = opcodes[0x56][2] = opcodes[0x56][4] = &CPU::lsrZpx8;
    opcodes[0x56][1] = opcodes[0x56][3] = &CPU::lsrZpx16;
    opcodes[0x4E][0] = opcodes[0x4E][2] = opcodes[0x4E][4] = &CPU::lsrAbs8;
    opcodes[0x4E][1] = opcodes[0x4E][3] = &CPU::lsrAbs16;
    opcodes[0x5E][0] = opcodes[0x5E][2] = opcodes[0x5E][4] = &CPU::lsrAbsx8;
    opcodes[0x5E][1] = opcodes[0x5E][3] = &CPU::lsrAbsx16;

    opcodes[0x2A][0] = opcodes[0x2A][2] = opcodes[0x2A][4] = &CPU::rola8;
    opcodes[0x2A][1] = opcodes[0x2A][3] = &CPU::rola16;
    opcodes[0x26][0] = opcodes[0x26][2] = opcodes[0x26][4] = &CPU::rolZp8;
    opcodes[0x26][1] = opcodes[0x26][3] = &CPU::rolZp16;
    opcodes[0x36][0] = opcodes[0x36][2] = opcodes[0x36][4] = &CPU::rolZpx8;
    opcodes[0x36][1] = opcodes[0x36][3] = &CPU::rolZpx16;
    opcodes[0x2E][0] = opcodes[0x2E][2] = opcodes[0x2E][4] = &CPU::rolAbs8;
    opcodes[0x2E][1] = opcodes[0x2E][3] = &CPU::rolAbs16;
    opcodes[0x3E][0] = opcodes[0x3E][2] = opcodes[0x3E][4] = &CPU::rolAbsx8;
    opcodes[0x3E][1] = opcodes[0x3E][3] = &CPU::rolAbsx16;

    opcodes[0x6A][0] = opcodes[0x6A][2] = opcodes[0x6A][4] = &CPU::rora8;
    opcodes[0x6A][1] = opcodes[0x6A][3] = &CPU::rora16;
    opcodes[0x66][0] = opcodes[0x66][2] = opcodes[0x66][4] = &CPU::rorZp8;
    opcodes[0x66][1] = opcodes[0x66][3] = &CPU::rorZp16;
    opcodes[0x76][0] = opcodes[0x76][2] = opcodes[0x76][4] = &CPU::rorZpx8;
    opcodes[0x76][1] = opcodes[0x76][3] = &CPU::rorZpx16;
    opcodes[0x6E][0] = opcodes[0x6E][2] = opcodes[0x6E][4] = &CPU::rorAbs8;
    opcodes[0x6E][1] = opcodes[0x6E][3] = &CPU::rorAbs16;
    opcodes[0x7E][0] = opcodes[0x7E][2] = opcodes[0x7E][4] = &CPU::rorAbsx8;
    opcodes[0x7E][1] = opcodes[0x7E][3] = &CPU::rorAbsx16;

    /* BIT group */
    opcodes[0x89][0] = opcodes[0x89][2] = opcodes[0x89][4] = &CPU::bitImm8;
    opcodes[0x89][1] = opcodes[0x89][3] = &CPU::bitImm16;
    opcodes[0x24][0] = opcodes[0x24][2] = opcodes[0x24][4] = &CPU::bitZp8;
    opcodes[0x24][1] = opcodes[0x24][3] = &CPU::bitZp16;
    opcodes[0x34][0] = opcodes[0x34][2] = opcodes[0x34][4] = &CPU::bitZpx8;
    opcodes[0x34][1] = opcodes[0x34][3] = &CPU::bitZpx16;
    opcodes[0x2C][0] = opcodes[0x2C][2] = opcodes[0x2C][4] = &CPU::bitAbs8;
    opcodes[0x2C][1] = opcodes[0x2C][3] = &CPU::bitAbs16;
    opcodes[0x3C][0] = opcodes[0x3C][2] = opcodes[0x3C][4] = &CPU::bitAbsx8;
    opcodes[0x3C][1] = opcodes[0x3C][3] = &CPU::bitAbsx16;

    /* Misc group */
    opcodes[0x00][0] = opcodes[0x00][1] = opcodes[0x00][2] = opcodes[0x00][3] = opcodes[0x00][4] = &CPU::brk;
    opcodes[0xEB][0] = opcodes[0xEB][1] = opcodes[0xEB][2] = opcodes[0xEB][3] = opcodes[0xEB][4] = &CPU::xba;
    opcodes[0xEA][0] = opcodes[0xEA][1] = opcodes[0xEA][2] = opcodes[0xEA][3] = opcodes[0xEA][4] = &CPU::nop;
    opcodes[0x5B][0] = opcodes[0x5B][1] = opcodes[0x5B][2] = opcodes[0x5B][3] = opcodes[0x5B][4] = &CPU::tcd;
    opcodes[0x7B][0] = opcodes[0x7B][1] = opcodes[0x7B][2] = opcodes[0x7B][3] = opcodes[0x7B][4] = &CPU::tdc;
    opcodes[0x1B][0] = opcodes[0x1B][1] = opcodes[0x1B][2] = opcodes[0x1B][3] = opcodes[0x1B][4] = &CPU::tcs;
    opcodes[0x3B][0] = opcodes[0x3B][1] = opcodes[0x3B][2] = opcodes[0x3B][3] = opcodes[0x3B][4] = &CPU::tsc;
    opcodes[0xCB][0] = opcodes[0xCB][1] = opcodes[0xCB][2] = opcodes[0xCB][3] = opcodes[0xCB][4] = &CPU::wai;
    opcodes[0x44][0] = opcodes[0x44][1] = opcodes[0x44][2] = opcodes[0x44][3] = opcodes[0x44][4] = &CPU::mvp;
    opcodes[0x54][0] = opcodes[0x54][1] = opcodes[0x54][2] = opcodes[0x54][3] = opcodes[0x54][4] = &CPU::mvn;
    opcodes[0x04][0] = opcodes[0x04][2] = opcodes[0x04][4] = &CPU::tsbZp8;
    opcodes[0x04][1] = opcodes[0x04][3] = &CPU::tsbZp16;
    opcodes[0x0C][0] = opcodes[0x0C][2] = opcodes[0x0C][4] = &CPU::tsbAbs8;
    opcodes[0x0C][1] = opcodes[0x0C][3] = &CPU::tsbAbs16;
    opcodes[0x14][0] = opcodes[0x14][2] = opcodes[0x14][4] = &CPU::trbZp8;
    opcodes[0x14][1] = opcodes[0x14][3] = &CPU::trbZp16;
    opcodes[0x1C][0] = opcodes[0x1C][2] = opcodes[0x1C][4] = &CPU::trbAbs8;
    opcodes[0x1C][1] = opcodes[0x1C][3] = &CPU::trbAbs16;
}
