#include <stdint.h>

#define MEMORY_MAX (1 << 16)
uint16_t memory[MEMORY_MAX]; /* 65536 locations */

enum RegistersEnum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, /* program counter */
    R_COND,
    R_COUNT
};

/*
OP CODES - Left 4 bits of 16 bits in register represent the op code
tells CPU what task to do, rest of bits are for parameters to operate with
*/
enum
{
    OP_BR = 0, /* branch */
    OP_ADD,    /* add  */
    OP_LD,     /* load */
    OP_ST,     /* store */
    OP_JSR,    /* jump register */
    OP_AND,    /* bitwise and */
    OP_LDR,    /* load register */
    OP_STR,    /* store register */
    OP_RTI,    /* unused */
    OP_NOT,    /* bitwise not */
    OP_LDI,    /* load indirect */
    OP_STI,    /* store indirect */
    OP_JMP,    /* jump */
    OP_RES,    /* reserved (unused) */
    OP_LEA,    /* load effective address */
    OP_TRAP    /* execute trap */
};

// CONDITION FLAGS
// Provides info about recently executed calculation
enum
{
    FL_POS = 1 << 0, /* P */
    FL_ZRO = 1 << 1, /* Z */
    FL_NEG = 1 << 2, /* N */
};

enum
{
    TRAP_GETC  = 0x20,
    TRAP_OUT   = 0x21,
    TRAP_PUTS  = 0x22,
    TRAP_IN    = 0x23,
    TRAP_PUTSP = 0x24,
    TRAP_HALT  = 0x25,
};

// Pads to 16 bit. 0s = positive value, 1s = negative value
uint16_t sign_extend(uint16_t x, int bit_count)
{
    if ((x >> (bit_count - 1)) & 1)
    {
        x |= (0xFFFF << bit_count);
    }
    return x;
}

void update_flags(uint16_t r)
{
    if (reg[r] == 0)
    {
        reg[R_COND] = FL_ZRO;
    }
    else if (reg[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
        reg[R_COND] = FL_NEG;
    }
    else
    {
        reg[R_COND] = FL_POS;
    }
}

// REGISTERS ARRAY
uint16_t reg[R_COUNT];

int main(int argc, const char *argv[])
{

    // LOAD ARGUMENTS
    if (argc < 2)
    {
        /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }
    for (int j = 1; j < argc; ++j)
    {
        if (!read_image(argv[j]))
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    @{Setup}

    /* since exactly one condition flag should be set at any given time, set the Z flag */
    reg[R_COND] = FL_ZRO;

    /* set the PC to starting position */
    /* 0x3000 is the default */
    enum
    {
        PC_START = 0x3000
    };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running)
    {
        /* FETCH */
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch (op)
        {
        case OP_ADD:
        {
            /* destination register (DR) */
            uint16_t r0 = (instr >> 9) & 0x7;
            /* first operand (SR1) */
            uint16_t r1 = (instr >> 6) & 0x7;
            /* whether we are in immediate mode */
            uint16_t imm_flag = (instr >> 5) & 0x1;

            if (imm_flag)
            {
                uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                reg[r0] = reg[r1] + imm5;
            }
            else
            {
                uint16_t r2 = instr & 0x7;
                reg[r0] = reg[r1] + reg[r2];
            }

            update_flags(r0);
        }
        break;

        case OP_AND:
        {
            uint16_t r0 = (instr >> 9) & 0x7;
            uint16_t r1 = (instr >> 6) & 0x7;
            uint16_t imm_flag = (instr >> 5) & 0x1;

            if (imm_flag)
            {
                uint16_t imm5 = sign_extend(instr & 0x1F, 5);
                reg[r0] = reg[r1] & imm5;
            }
            else
            {
                uint16_t r2 = instr & 0x7;
                reg[r0] = reg[r1] & reg[r2];
            }
            update_flags(r0);
        }
        break;

        case OP_NOT:
        {
            uint16_t r0 = (instr >> 9) & 0x7;
            uint16_t r1 = (instr >> 6) & 0x7;

            reg[r0] = ~reg[r1];
            update_flags(r0);
        }
        break;

        case OP_BR:
        {
            uint16_t cond_flag = (instr >> 9) & 0x7;
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
            if (cond_flag & reg[R_COND])
            {
                reg[R_PC] += pc_offset;
            }
        }
        break;

        case OP_JMP:
        {
            uint16_t base_r = (instr >> 6) & 0x7;
            reg[R_PC] = reg[base_r];
        }
        break;

        case OP_JSR:
        {
            uint16_t long_flag = (instr >> 11) & 1;
            reg[R_R7] = reg[R_PC];
            if (long_flag)
            {
                uint16_t long_pc_offset = sign_extend(instr & 0x7FF, 11);
                reg[R_PC] += long_pc_offset;
            }
            else
            {
                uint16_t base_r = (instr >> 6) & 0x7;
                reg[R_PC] = reg[base_r];
            }
        }
        break;

        case OP_LD:
        {
            uint16_t dr = (instr >> 9) & 0x7;
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);

            reg[dr] = mem_read(reg[R_PC] + pc_offset);
            update_flags(dr);
        }
        break;

        case OP_LDI:
        {
            /* destination register (DR) */
            uint16_t r0 = (instr >> 9) & 0x7;
            /* PCoffset 9*/
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
            /* add pc_offset to the current PC, look at that memory location to get the final address */
            reg[r0] = mem_read(mem_read(reg[R_PC] + pc_offset));
            update_flags(r0);
        }
        break;

        case OP_LDR:
        {
            uint16_t dr = (instr >> 9) & 0x7;
            uint16_t base_r = (instr >> 6) & 0x7;
            uint16_t offset = sign_extend(instr & 0x3F, 6);
            reg[dr] = mem_read(reg[base_r] + offset);
            update_flags(dr);
        }
        break;

        case OP_LEA:
        {
            uint16_t r0 = (instr >> 9) & 0x7;
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
            reg[r0] = reg[R_PC] + pc_offset;
            update_flags(r0);
        }
        break;

        case OP_ST:
        {
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
            uint16_t sr = (instr >> 9) & 0x7;
            mem_write(reg[R_PC] + pc_offset, reg[sr]);
        }
        break;

        case OP_STI:
        {
            uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
            uint16_t sr = (instr >> 9) & 0x7;
            mem_write(mem_read(reg[R_PC] + pc_offset), reg[sr]);
        }
        break;

        case OP_STR:
        {
            uint16_t base_r = (instr >> 6) & 0x7;
            uint16_t offset = sign_extend(instr & 0x3F, 6);
            uint16_t sr = (instr >> 9) & 0x7;
            mem_write(reg[base_r] + offset, reg[sr]);
        }
        break;

        case OP_TRAP:
        {
            reg[R_R7] = reg[R_PC];
            switch (instr & 0xFF)
            {
                case TRAP_GETC:
                    reg[R_R0] = (uint16_t)getchar();
                    update_flags(R_R0);
                    break;
                case TRAP_OUT:
                    putc((char)reg[R_R0], stdout);
                    fflush(stdout);
                    break;
                case TRAP_PUTS:
                {
                    uint16_t *c = memory + reg[R_R0];
                    while (*c)
                    {
                        putc((char)*c, stdout);
                        ++c;
                    }
                    fflush(stdout);
                }
                break;
                case TRAP_IN:
                {
                    printf("Enter a character: ");
                    char ch = getchar();
                    putc(ch, stdout);
                    fflush(stdout);
                    reg[R_R0] = (uint16_t)ch;
                    update_flags(R_R0);
                }
                break;
                case TRAP_PUTSP:
                {
                    uint16_t *c = memory + reg[R_R0];
                    while (*c)
                    {
                        char char1 = (*c) & 0xFF;
                        putc(char1, stdout);
                        char char2 = (*c) >> 8;
                        if (char2) putc(char2, stdout);
                        ++c;
                    }
                    fflush(stdout);
                }
                break;
                case TRAP_HALT:
                    puts("HALT");
                    fflush(stdout);
                    running = 0;
                    break;
            }
        }
        break;

        case OP_RES:
            exit(1);

        case OP_RTI:
            exit(1);

        default:
            break;
        }
    }
    exit(0);
}
