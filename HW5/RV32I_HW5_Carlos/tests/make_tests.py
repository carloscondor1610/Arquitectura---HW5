"""Genera programas de prueba RAW RV32I (.bin).
Este archivo SOLO construye las pruebas; el simulador C++ no ensambla.
Uso: python tests/make_tests.py
"""
from pathlib import Path
import struct

OUT = Path(__file__).resolve().parent

def check_imm(n, bits):
    assert -(1 << (bits - 1)) <= n < (1 << (bits - 1)), (n, bits)

def R(funct7, rs2, rs1, funct3, rd):
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | 0x33

def I(imm, rs1, funct3, rd, opcode=0x13):
    check_imm(imm, 12)
    return ((imm & 0xFFF) << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | opcode

def S(imm, rs2, rs1, funct3):
    check_imm(imm, 12)
    v = imm & 0xFFF
    return ((v >> 5) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | ((v & 31) << 7) | 0x23

def B(imm, rs2, rs1, funct3):
    check_imm(imm, 13)
    assert not imm & 1
    v = imm & 0x1FFF
    return (((v >> 12) & 1) << 31) | (((v >> 5) & 63) << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (((v >> 1) & 15) << 8) | (((v >> 11) & 1) << 7) | 0x63

def U(upper20, rd, opcode):
    assert 0 <= upper20 < (1 << 20)
    return (upper20 << 12) | (rd << 7) | opcode

def J(imm, rd):
    check_imm(imm, 21)
    assert not imm & 1
    v = imm & 0x1FFFFF
    return (((v >> 20) & 1) << 31) | (((v >> 1) & 1023) << 21) | (((v >> 11) & 1) << 20) | (((v >> 12) & 255) << 12) | (rd << 7) | 0x6F

class Program:
    def __init__(self, name):
        self.name = name
        self.words = []
        self.labels = {}
        self.fixups = []
    def at(self, label):
        if label in self.labels: raise ValueError(label)
        self.labels[label] = len(self.words)*4
    def emit(self, word):
        self.words.append(word)
    def addi(self, rd, rs1, imm): self.emit(I(imm, rs1, 0, rd))
    def opimm(self, rd, rs1, imm, f3): self.emit(I(imm,rs1,f3,rd))
    def slli(self, rd, rs1, sh): self.emit(I(sh,rs1,1,rd))
    def srli(self, rd, rs1, sh): self.emit(I(sh,rs1,5,rd))
    def srai(self, rd, rs1, sh): self.emit(I((0x20<<5)|sh,rs1,5,rd))
    def reg(self, rd, rs1, rs2, f3, f7=0): self.emit(R(f7,rs2,rs1,f3,rd))
    def lui(self, rd, upper): self.emit(U(upper, rd, 0x37))
    def auipc(self, rd, upper): self.emit(U(upper, rd, 0x17))
    def load(self, rd, base, imm, f3): self.emit(I(imm,base,f3,rd,0x03))
    def store(self, src, base, imm, f3): self.emit(S(imm,src,base,f3))
    def branch(self, rs1,rs2,f3,label):
        self.fixups.append((len(self.words), 'b', label, rs1, rs2, f3))
        self.emit(0)
    def jal(self, rd, label):
        self.fixups.append((len(self.words), 'j', label, rd))
        self.emit(0)
    def jalr(self, rd, base, imm): self.emit(I(imm,base,0,rd,0x67))
    def ebreak(self): self.emit(0x00100073)
    def save(self):
        for fix in self.fixups:
            pos, typ, label, *args = fix
            delta = self.labels[label] - pos*4
            self.words[pos] = B(delta,args[1],args[0],args[2]) if typ == 'b' else J(delta,args[0])
        path = OUT / self.name
        path.write_bytes(b''.join(struct.pack('<I', word) for word in self.words))
        print(f'{path.name}: {len(self.words)} instructions ({path.stat().st_size} bytes)')

# 01: Suma 1 + 2 + ... + 5, resultado 15 en memoria [0x1000].
p = Program('01_suma_1_a_5.bin')
p.addi(5,0,0)   # t0=0  suma
p.addi(6,0,1)   # t1=1  i
p.addi(7,0,6)   # t2=6  limite exclusivo
p.at('loop')
p.reg(5,5,6,0)  # add t0,t0,t1
p.addi(6,6,1)
p.branch(6,7,4,'loop')  # blt t1,t2,loop
p.lui(28,1)     # t3 = 0x1000
p.store(5,28,0,2)
p.ebreak()
p.save()

# 02: 22 operaciones de ALU, comparaciones y shift inmediato.
p = Program('02_alu_y_comparaciones.bin')
p.addi(1,0,-8)
p.addi(2,0,3)
p.reg(3,1,2,0)         # add=-5
p.reg(4,2,1,0,0x20)    # sub=11
p.reg(5,2,2,1)         # sll=24
p.reg(6,1,2,2)         # slt=1
p.reg(7,1,2,3)         # sltu=0
p.reg(8,1,2,4)         # xor=-5
p.reg(9,1,2,5)         # srl
p.reg(10,1,2,5,0x20)  # sra=-1
p.reg(11,1,2,6)        # or
p.reg(12,1,2,7)        # and=0
p.slli(13,2,2)
p.srli(14,1,2)
p.srai(15,1,2)
p.opimm(16,1,0,2)      # slti 1
p.opimm(17,1,-1,3)     # sltiu 1
p.opimm(18,2,7,4)      # xori 4
p.opimm(19,2,8,6)      # ori 11
p.opimm(20,1,3,7)      # andi 0
p.auipc(21,1)
p.lui(22,0x12345)
p.addi(0,0,99)         # x0 no se modifica
p.ebreak()
p.save()

# 03: load/store 8/16/32, signo, y little-endian.
p = Program('03_memoria_little_endian.bin')
p.lui(5,0x12345)
p.addi(5,5,0x678)     # t0=0x12345678
p.lui(6,1)            # t1=0x1000
p.store(5,6,0,2)     # sw -> 78 56 34 12
p.load(7,6,0,0)      # lb 0x78
p.load(28,6,3,4)     # lbu 0x12
p.load(29,6,0,1)     # lh 0x5678
p.load(30,6,2,5)     # lhu 0x1234
p.load(31,6,0,2)     # lw 0x12345678
p.addi(5,0,-128)
p.store(5,6,4,0)     # sb 0x80
p.load(10,6,4,0)     # lb -128
p.load(11,6,4,4)     # lbu 128
p.store(5,6,6,1)     # sh 0xFF80
p.load(12,6,6,1)     # lh -128
p.load(13,6,6,5)     # lhu 65408
p.ebreak()
p.save()

# 04: funcion y retorno jal/jalr, 5+4+3+2+1=15.
p = Program('04_funcion_jal_jalr.bin')
p.addi(10,0,5)                 # a0 = 5
p.jal(1,'triangular')           # ra = pc+4
p.lui(5,1)
p.store(10,5,0,2)
p.ebreak()
p.at('triangular')
p.addi(6,0,0)
p.at('sumar')
p.branch(10,0,0,'terminar') # beq a0,zero
p.reg(6,6,10,0)
p.addi(10,10,-1)
p.jal(0,'sumar')
p.at('terminar')
p.addi(10,6,0)
p.jalr(0,1,0)                 # ret
p.save()

# 05: Prueba de TODAS las comparaciones de branch, con caminos tomados y no tomados.
p = Program('05_branches_signed_unsigned.bin')
p.addi(1,0,-1)
p.addi(2,0,1)
p.branch(1,2,0,'fail')  # beq: no
p.branch(1,2,1,'L1')    # bne: si
p.addi(20,0,99)
p.at('L1')
p.branch(1,2,4,'L2')    # blt: si
p.addi(20,0,99)
p.at('L2')
p.branch(1,2,5,'fail')  # bge signed: no
p.branch(2,1,5,'L3')    # bge signed: si
p.addi(20,0,99)
p.at('L3')
p.branch(1,2,6,'fail')  # bltu: no
p.branch(2,1,6,'L4')    # bltu: si
p.addi(20,0,99)
p.at('L4')
p.branch(1,2,7,'L5')    # bgeu: si
p.addi(20,0,99)
p.at('L5')
p.branch(2,1,7,'fail')  # bgeu: no
p.addi(20,0,37)
p.ebreak()
p.at('fail')
p.addi(20,0,-1)
p.ebreak()
p.save()

# 06: Archivo no valido para demostrar el trap.
(OUT / '06_instruccion_invalida.bin').write_bytes(struct.pack('<I', 0xffffffff))
# 07: unaligned LW para validar acceso desalineado.
p = Program('07_lw_desalineado.bin')
p.lui(6,1)
p.load(5,6,1,2)
p.ebreak()
p.save()

# 08: Version original y propia inspirada en los objetivos de riscvtest.s.
# No se afirma que sea el mismo codigo del enlace del enunciado.
p = Program('08_riscvtest_propio.bin')
p.addi(5,0,12)
p.addi(6,0,10)
p.reg(7,5,6,0)       # 22
p.reg(8,7,6,0,0x20)  # 12
p.reg(9,5,6,7)       # AND = 8
p.reg(10,5,6,6)      # OR = 14
p.reg(11,6,5,2)      # SLT = 1
p.addi(12,7,3)       # 25
p.addi(13,0,100)     # mem[100]
p.store(12,13,0,2)
p.load(14,13,0,2)
p.branch(12,14,0,'ok')
p.addi(12,0,-1)      # ruta incorrecta
p.at('ok')
p.jal(0,'fin')
p.addi(12,0,-1)      # ruta omitida
p.at('fin')
p.ebreak()
p.save()

# 09: Environment calls opcionales 1=imprimir entero, 10=salir.
p = Program('09_ecall_opcional.bin')
p.addi(10,0,25)
p.addi(17,0,1)
p.emit(0x00000073)
p.addi(17,0,10)
p.emit(0x00000073)
p.save()
