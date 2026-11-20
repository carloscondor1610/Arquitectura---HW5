"""Pruebas automaticas de regresion del simulador RV32I.
Uso: python tests/verify.py (despues de compilar C++)
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CANDIDATES = [ROOT / 'build' / 'riscv_sim', ROOT / 'build' / 'riscv_sim.exe',
              ROOT / 'riscv_sim.exe']
SIM = next((path for path in CANDIDATES if path.exists()), CANDIDATES[0])

def execute(filename, commands='run\nregs\nexit\n'):
    return subprocess.run([str(SIM), str(ROOT / 'tests' / filename)], input=commands,
                          text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          timeout=10, check=True).stdout

def parse_regs(output):
    return {int(num): int(value, 16) for num, value in re.findall(
        r'\bx(\d+)\s*=\s*0x([0-9A-Fa-f]{8})', output)}

def verify(name, filename, expected_regs, must_contain=(), commands='run\nregs\nexit\n'):
    text = execute(filename, commands)
    actual = parse_regs(text)
    for reg, expected in expected_regs.items():
        assert reg in actual, f'{filename}: x{reg} no encontrado'
        assert actual[reg] == expected, (filename, f'x{reg}', hex(actual[reg]), hex(expected))
    for part in must_contain:
        assert part in text, (filename, f'no contiene {part!r}')
    print(f'[OK] {name}')
    return text

try:
    assert SIM.exists(), f'Compila primero: g++ -std=c++17 src/main.cpp -o build/riscv_sim'
    verify('Suma 1..5 y bucle negativo', '01_suma_1_a_5.bin',
           {0:0, 5:15, 6:6, 7:6}, ['0x00001000 = 0x0000000F'],
           'run\nword 0x1000\nregs\nexit\n')
    verify('ALU RV32I + registros x0 y AUIPC', '02_alu_y_comparaciones.bin',
           {0:0,1:0xFFFFFFF8,2:3,3:0xFFFFFFFB,4:11,5:24,6:1,7:0,
            8:0xFFFFFFFB,9:0x1FFFFFFF,10:0xFFFFFFFF,11:0xFFFFFFFB,
            12:0,13:12,14:0x3FFFFFFE,15:0xFFFFFFFE,16:1,17:1,
            18:4,19:11,20:0,21:0x1050,22:0x12345000})
    verify('Load/store con signo, sin signo y little-endian', '03_memoria_little_endian.bin',
           {5:0xFFFFFF80,6:0x1000,7:0x78,28:0x12,29:0x5678,30:0x1234,
            31:0x12345678,10:0xFFFFFF80,11:0x80,12:0xFFFFFF80,13:0xFF80},
           ['78 56 34 12 80 00 80 FF'], 'run\nmem 0x1000 0x1007\nregs\nexit\n')
    verify('Llamada con JAL/JALR y funcion', '04_funcion_jal_jalr.bin',
           {1:8,10:15}, ['0x00001000 = 0x0000000F'], 'run\nword 0x1000\nregs\nexit\n')
    verify('Seis tipos de branch (signed/unsigned)', '05_branches_signed_unsigned.bin',
           {1:0xFFFFFFFF,2:1,20:37})
    assert 'Instruccion no soportada' in execute('06_instruccion_invalida.bin', 'run\nexit\n')
    print('[OK] Trap ante instruccion invalida')
    assert 'acceso desalineado' in execute('07_lw_desalineado.bin', 'run\nexit\n')
    print('[OK] Trap ante direccion desalineada')
    verify('riscvtest propio: mem[100] = 25', '08_riscvtest_propio.bin',
           {12:25,14:25}, ['0x00000064 = 0x00000019'],
           'run\nword 0x64\nregs\nexit\n')
    assert '[ecall] 25' in execute('09_ecall_opcional.bin', 'run\nexit\n')
    print('[OK] ECALL opcional')
    result = execute('01_suma_1_a_5.bin', 'step 3\nreset\npc\nstep\nregs x5 x6 x7\nexit\n')
    assert 'pc = 0x00000000' in result
    assert 'x5 = 0x00000000' in result
    print('[OK] Ejecucion paso a paso y reset')
    print('\n10 grupos de pruebas correctos.')
except Exception as error:
    print('[ERROR]', error)
    sys.exit(1)
