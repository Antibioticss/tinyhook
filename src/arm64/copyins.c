#ifdef __aarch64__

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// opcode masks / bases
#define OPC_MASK_26     0x7C000000u
#define OPC_B           0x14000000u   // unconditional branch (imm26)
#define OPC_BL          0x94000000u   // branch with link (imm26)
#define OPC_ADR_MASK    0x9F000000u
#define OPC_ADR         0x10000000u   // ADR
#define OPC_ADRP        0x90000000u   // ADRP

#define OPC_MOVZ        0xD2800000u
#define OPC_MOVK        0xF2800000u

#define OPC_BR          0xD61F0000u
#define OPC_BLR         0xD63F0000u

extern void __builtin___clear_cache(char *begin, char *end);

static inline uint32_t read32(const void *p) {
    uint32_t v;
    memcpy(&v, p, sizeof(v));
    return v;
}
static inline void write32(void *p, uint32_t v) {
    memcpy(p, &v, sizeof(v));
}

// MARK: - helpers

static inline bool branch26_fits(int64_t delta) {
    // branch's signed range: imm26<<2 -> signed 28-bit offset: ±(2^27)
    int64_t min = -(1LL << 27);
    int64_t max =  (1LL << 27) - 4;
    return (delta >= min && delta <= max && (delta % 4 == 0));
}

// ADR/ADRP helpers: encode fields for ADR/ADRP
static inline bool adr_imm_fits(int64_t imm) {
    int64_t min = -(1LL << 20);
    int64_t max =  (1LL << 20) - 1;
    return (imm >= min && imm <= max);
}

static inline bool adrp_imm_fits(int64_t imm_pages) {
    int64_t min = -(1LL << 20);
    int64_t max =  (1LL << 20) - 1;
    return (imm_pages >= min && imm_pages <= max);
}

static inline uint32_t encode_adr(uint8_t rd, int64_t imm) {
    uint32_t imm_u = (uint32_t)(imm & 0x1FFFFF); // lower 21 bits
    uint32_t immlo = (imm_u & 0x3);
    uint32_t immhi = (imm_u >> 2) & 0x7FFFFu;
    return OPC_ADR | (immlo << 29) | (immhi << 5) | (rd & 0x1Fu);
}

static inline uint32_t encode_adrp(uint8_t rd, int64_t imm_pages) {
    uint32_t imm_u = (uint32_t)(imm_pages & 0x1FFFFF);
    uint32_t immlo = (imm_u & 0x3);
    uint32_t immhi = (imm_u >> 2) & 0x7FFFFu;
    return OPC_ADRP | (immlo << 29) | (immhi << 5) | (rd & 0x1Fu);
}

static inline uint32_t encode_movz(uint8_t rd, uint16_t imm16, uint8_t hw) {
    return OPC_MOVZ | (((uint32_t)imm16) << 5) | (((uint32_t)hw) << 21) | (rd & 0x1Fu);
}
static inline uint32_t encode_movk(uint8_t rd, uint16_t imm16, uint8_t hw) {
    return OPC_MOVK | (((uint32_t)imm16) << 5) | (((uint32_t)hw) << 21) | (rd & 0x1Fu);
}

static inline uint32_t encode_br_reg(uint8_t rn) { return OPC_BR | (rn & 0x1Fu); }
static inline uint32_t encode_blr_reg(uint8_t rn) { return OPC_BLR | (rn & 0x1Fu); }

// decode ADR/ADRP immediate to absolute address
static inline int64_t decode_adr_imm(uint32_t insn, uint64_t pc, bool is_adrp) {
    uint32_t immlo = (insn >> 29) & 0x3u;
    uint32_t immhi = (insn >> 5) & 0x7FFFFu;
    uint32_t imm = (immhi << 2) | immlo; // 21 bits
    // sign-extend 21 bits
    int64_t signed21 = (imm & (1 << 20)) ? ((int64_t)imm | ~((1LL << 21) - 1)) : (int64_t)imm;
    if (is_adrp) {
        return ((int64_t)(pc & ~0xFFFULL)) + (signed21 << 12);
    } else {
        return (int64_t)pc + signed21;
    }
}

// MARK: - copy instruction

size_t copy_instruction(void **src_ptr, void **dst_ptr, int len) {
    uint8_t *src = (uint8_t *)(*src_ptr);
    uint8_t *dst = (uint8_t *)(*dst_ptr);
    uint8_t *orig_dst = dst;
    int offset = 0;

    while (offset < len) {
        uint32_t insn = read32(src + offset);
        uint64_t src_addr = (uint64_t)(src + offset);
        uint64_t dst_addr = (uint64_t)(dst);

        // B / BL
        if ((insn & OPC_MASK_26) == OPC_B) {
            bool is_bl = (insn & 0x80000000u) != 0; // link bit set -> BL
            uint32_t imm26 = insn & 0x03FFFFFFu;
            int64_t simm = (imm26 & (1 << 25)) ? ((int64_t)imm26 | ~((1LL<<26)-1)) : imm26;
            int64_t target = (int64_t)src_addr + (simm << 2);
            int64_t delta = target - (int64_t)dst_addr;

            if (branch26_fits(delta)) {
                int64_t imm = delta >> 2;
                uint32_t new_insn = (is_bl ? OPC_BL : OPC_B) | ((uint32_t)imm & 0x03FFFFFFu);
                write32(dst, new_insn);
                dst += 4;
            } else {
                // fallback expanded sequence (load full address into tmp then BR/BLR)
                uint8_t tmp = 17;
                uint64_t addr = (uint64_t)target;
                uint16_t p0 = (addr >> 0) & 0xFFFF;
                uint16_t p1 = (addr >> 16) & 0xFFFF;
                uint16_t p2 = (addr >> 32) & 0xFFFF;
                uint16_t p3 = (addr >> 48) & 0xFFFF;
                write32(dst + 0, encode_movz(tmp, p0, 0));
                write32(dst + 4, encode_movk(tmp, p1, 1));
                write32(dst + 8, encode_movk(tmp, p2, 2));
                write32(dst + 12, encode_movk(tmp, p3, 3));
                if (is_bl) {
                    write32(dst + 16, encode_blr_reg(tmp));
                } else {
                    write32(dst + 16, encode_br_reg(tmp));
                }
                dst += 20;
            }
            offset += 4;
            continue;
        }

        // ADR / ADRP: try single-instruction at dst, otherwise build MOVZ/MOVK
        if ((insn & OPC_ADR_MASK) == OPC_ADRP || (insn & OPC_ADR_MASK) == OPC_ADR) {
            bool is_adrp = ((insn & OPC_ADR_MASK) == OPC_ADRP);
            uint8_t rd = insn & 0x1Fu;
            int64_t target = decode_adr_imm(insn, src_addr, is_adrp);

            if (is_adrp) {
                // for ADRP, compute page difference from dst page
                uint64_t tgt_page = ((uint64_t)target) & ~0xFFFULL;
                uint64_t dst_page = (dst_addr) & ~0xFFFULL;
                int64_t imm_pages = (int64_t)( (int64_t)tgt_page - (int64_t)dst_page ) >> 12;
                // check if fits in signed 21-bit imm_pages
                if (adrp_imm_fits(imm_pages)) {
                    uint32_t new_insn = encode_adrp(rd, imm_pages);
                    write32(dst, new_insn);
                    dst += 4;
                } else {
                    // fallback: emit full 64-bit immediate into rd
                    uint64_t addr = (uint64_t)target;
                    uint16_t p0 = (addr >> 0) & 0xFFFF;
                    uint16_t p1 = (addr >> 16) & 0xFFFF;
                    uint16_t p2 = (addr >> 32) & 0xFFFF;
                    uint16_t p3 = (addr >> 48) & 0xFFFF;
                    write32(dst + 0, encode_movz(rd, p0, 0));
                    write32(dst + 4, encode_movk(rd, p1, 1));
                    write32(dst + 8, encode_movk(rd, p2, 2));
                    write32(dst + 12, encode_movk(rd, p3, 3));
                    dst += 16;
                }
            } else {
                // ADR: immediate is relative to PC (dst_addr) in bytes
                int64_t imm_bytes = (int64_t)target - (int64_t)dst_addr;
                if (adr_imm_fits(imm_bytes)) {
                    uint32_t new_insn = encode_adr(rd, imm_bytes);
                    write32(dst, new_insn);
                    dst += 4;
                } else {
                    uint64_t addr = (uint64_t)target;
                    uint16_t p0 = (addr >> 0) & 0xFFFF;
                    uint16_t p1 = (addr >> 16) & 0xFFFF;
                    uint16_t p2 = (addr >> 32) & 0xFFFF;
                    uint16_t p3 = (addr >> 48) & 0xFFFF;
                    write32(dst + 0, encode_movz(rd, p0, 0));
                    write32(dst + 4, encode_movk(rd, p1, 1));
                    write32(dst + 8, encode_movk(rd, p2, 2));
                    write32(dst + 12, encode_movk(rd, p3, 3));
                    dst += 16;
                }
            }
            offset += 4;
            continue;
        }

        // Default: copy verbatim
        write32(dst, insn);
        dst += 4;
        offset += 4;
    }

    __builtin___clear_cache((char*)orig_dst, (char*)dst);

    *src_ptr = src + offset;
    *dst_ptr = dst;

    return (size_t)(dst - orig_dst);
}

#endif
