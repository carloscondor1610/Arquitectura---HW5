# HW#5 - Informe breve: Simulador del ISA RISC-V (RV32I)

**Curso:** CS3051 Arquitectura de Computadores, 2026-II

**Integrantes:** [NOMBRE(S) Y CÓDIGO(S)]

**Repositorio Git:** [PENDIENTE: URL REAL]

**Google Drive (video demostrativo + código fuente):** [PENDIENTE: URL REAL]

## 1. Objetivo

Desarrollar un simulador funcional de instrucciones RV32I que permita cargar código de máquina desde un archivo RAW, ejecutarlo paso a paso y examinar el PC, los 32 registros arquitectónicos y la memoria.

## 2. Implementación

El simulador se desarrolló en C++17. Se emplea una clase `Simulator`, que contiene un arreglo de 32 registros de 32 bits, el PC, contador de instrucciones ejecutadas y una memoria dispersa representada mediante `unordered_map<uint32_t, uint8_t>`. Los programas se cargan desde archivos `.bin` a partir de `0x00000000`. Las instrucciones se recuperan como palabras de 32 bits little-endian y se decodifican con campos opcode, funct3, funct7 y operandos inmediatos según los formatos R/I/S/B/U/J. Las escrituras al registro `x0` se ignoran.

El flujo de una instrucción es: **fetch → decode → execute → actualizar registros/memoria → actualizar PC**. No se simulan etapas de pipeline ni ciclos de reloj.

## 3. Funciones implementadas

- Las 37 instrucciones RV32I solicitadas en el anexo A: cargas, stores, ALU con inmediatos y registros, `auipc`, `lui`, los seis branches, `jal` y `jalr`.
- Carga de binarios RAW; ejecución con `step`, `step n` y `run`.
- Visualización de `pc`, `regs`, `mem`, `word`; desensamblado básico con `disasm`.
- Reset, cambio de PC y registros, y reporte de instrucciones inválidas/accesos desalineados.
- `ebreak` como parada didáctica y algunas llamadas `ecall` opcionales para imprimir o terminar.

## 4. Pruebas y resultados

| Caso | Aspecto comprobado | Resultado esperado y observado |
| --- | --- | --- |
| 01_suma_1_a_5.bin | `addi`, `add`, `blt`, `lui`, `sw` | `mem[0x1000] = 15` |
| 02_alu_y_comparaciones.bin | Operaciones ALU y diferencias signed/unsigned | `x0=0`, `x10=0xFFFFFFFF`, `x22=0x12345000` |
| 03_memoria_little_endian.bin | `lb/lh/lw/lbu/lhu/sb/sh/sw` y signo | `78 56 34 12` en `0x1000..0x1003`; `a0=-128`, `a1=128` |
| 04_funcion_jal_jalr.bin | `jal`, `jalr` y retorno | `a0 = 15`, `ra = 8` |
| 05_branches_signed_unsigned.bin | Seis condiciones de branch | `x20 = 37` |
| 06_instruccion_invalida.bin | Manejo de instrucciones inválidas | Reporte `TRAP` |
| 07_lw_desalineado.bin | Manejo de acceso `lw` desalineado | Reporte `TRAP` |
| 08_riscvtest_propio.bin | Registros, aritmética, load/store, beq/jal | `mem[100] = 25 (0x19)` |
| 09_ecall_opcional.bin | Extensión de llamadas de entorno | Imprime `25` |

Se ejecutaron **10 grupos de verificaciones automáticas**, todos aprobados en Linux con `g++ 14.2`. No se han ejecutado los programas externos originales de QuickSort, árbol simétrico o riscvtest.s indicados en el documento; los casos anteriores son programas propios.

## 5. Limitaciones y conclusión

El simulador no carga ELF y no tiene integración con GDB. La memoria es plana, dispersa y accesible byte a byte, sin permisos. Se asume RV32I sin instrucciones comprimidas; los accesos naturales a palabras/medias palabras deben estar alineados. La implementación permite entender la relación entre las instrucciones, la actualización del estado arquitectónico y las operaciones de memoria. Las pruebas realizadas cubren las instrucciones pedidas y el flujo de ejecución básico.

**Pendiente antes de Canvas:** completar integrantes y dos enlaces reales, grabar al menos cuatro demostraciones y subir el video y código ZIP a Drive.
