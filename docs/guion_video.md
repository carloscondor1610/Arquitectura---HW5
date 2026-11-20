# Guion sugerido del video demostrativo (HW#5)

**Antes de empezar:** abre PowerShell en la carpeta del proyecto, compila con `g++ -std=c++17 -O2 src/main.cpp -o riscv_sim.exe` y agranda la fuente de la terminal para que se lea. Muestra brevemente `src/main.cpp` al principio o al final.

## Introducción

«Este proyecto implementa un simulador funcional del ISA RISC-V RV32I, escrito en C++17. Lee un archivo binario RAW que se carga desde la dirección cero, mantiene el PC, 32 registros y memoria little-endian, e interpreta las instrucciones una por una. Con `step` puedo ejecutar de manera controlada y con `regs` y `mem` inspeccionar el estado.»

## Ejemplo 1: suma de naturales

```powershell
.\riscv_sim.exe .\tests\01_suma_1_a_5.bin
```

```text
pc
disasm 8
step 3
regs x5 x6 x7
run
word 0x1000
exit
```

**Qué explicar:** PC avanza de 0 a 4, 8, 12; `blt` repite el bucle. La suma almacenada es 15 (`0xF`).

## Ejemplo 2: operaciones ALU

```powershell
.\riscv_sim.exe .\tests\02_alu_y_comparaciones.bin
```

```text
step 4
regs x1 x2 x3 x4
run
regs x0 x9 x10 x15 x16 x17 x21 x22
exit
```

**Qué explicar:** `x1=-8`, `x2=3`; suma da `-5` y resta 11. `srl` completa con ceros, `sra` conserva el signo. `x0` sigue siendo cero.

## Ejemplo 3: accesos a memoria

```powershell
.\riscv_sim.exe .\tests\03_memoria_little_endian.bin
```

```text
step 4
mem 0x1000 0x1003
run
regs a0 a1 a2 a3
mem 0x1000 0x1007
exit
```

**Qué explicar:** Se escribe `0x12345678`; la memoria muestra bytes `78 56 34 12`, característicos de little-endian. Una carga `lb` extiende signo (-128), una `lbu` no lo hace (128).

## Ejemplo 4: llamada a una función

```powershell
.\riscv_sim.exe .\tests\04_funcion_jal_jalr.bin
```

```text
pc
step 2
regs ra a0
run
regs ra a0
word 0x1000
exit
```

**Qué explicar:** `jal` guarda en `ra=x1` la dirección de retorno `0x00000008`; la función suma 5+4+3+2+1 y `jalr` regresa al llamador. `a0=15` y `mem[0x1000]=15`.

## Cierre

«Además, agregué pruebas de los seis tipos de branch, manejo de instrucciones inválidas, accesos desalineados y una suite automática. El PDF incluye enlaces al repositorio y al video.»

**Importante:** tienes que grabar de verdad las cuatro ejecuciones y subir el video a Drive. El ZIP entregado no incluye un video.
