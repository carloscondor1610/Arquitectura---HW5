# HW#5 - Simulador RISC-V RV32I (C++17)

Proyecto de **CS3051 Arquitectura de Computadores, 2026-II**. Simula un procesador RISC-V de 32 bits en la terminal. El programa **lee instrucciones de maquina de un archivo RAW `.bin`**; no recibe instrucciones assembly como entrada ni implementa un ensamblador.

## Requisitos cubiertos

- Las **37 instrucciones RV32I** exigidas en el anexo A del enunciado, incluidas cargas, stores, ALU, branches, `lui`, `auipc`, `jal` y `jalr`.
- Programa cargado en `0x00000000` a partir de un archivo binario crudo.
- Registros `x0..x31` de 32 bits; `x0` es inmutable y el PC avanza en palabras de 4 bytes (sin extensión comprimida).
- Memoria **byte-addressable, little-endian**, con espacio de direcciones de 32 bits y representación dispersa (`unordered_map`). Las celdas no escritas se leen como cero.
- **Ejecución paso a paso**, consulta de PC, registros y memoria, y desensamblador básico.
- Errores explícitos por instrucciones inválidas, saltos desalineados y accesos `lh/lw/sh/sw` desalineados.
- Extensión didáctica opcional: `ebreak` para terminar pruebas y un subconjunto de `ecall` (`a7=1,4,10,11,93`).

**Limitaciones conocidas:** no carga archivos ELF, no ofrece interfaz GDB ni soporta extensiones C/M/A/F/D. El manejo de `ebreak` y `ecall` es simplificado para ejecución en la consola. No se modela hardware, pipeline, excepciones privilegiadas ni ciclos de reloj: es un simulador funcional del ISA base. La memoria tiene direccionamiento lógico de 4 GiB, **no** reserva 4 GiB físicos. El archivo RAW se limita a 16 MiB para evitar cargas accidentales enormes. `sp=x2` inicia en `0x00100000` por conveniencia para funciones recursivas; puede configurarse con `setsp`.

## 1. Compilar y ejecutar

### Windows (PowerShell con `g++` instalado, por ejemplo MSYS2/MinGW)

Abrir la terminal en la carpeta donde está este `README.md`.

```powershell
g++ -std=c++17 -O2 -Wall -Wextra src/main.cpp -o riscv_sim.exe
.\riscv_sim.exe .\tests\01_suma_1_a_5.bin
```

### Linux / WSL

```bash
g++ -std=c++17 -O2 -Wall -Wextra src/main.cpp -o build/riscv_sim
./build/riscv_sim tests/01_suma_1_a_5.bin
```

Los archivos `.bin` de `tests/` ya están generados y listos para usarse. **No necesitas ensamblar nada** para las demostraciones incluidas.

## 2. Comandos del simulador

| Comando | Acción |
| --- | --- |
| `help` | Lista los comandos disponibles |
| `pc` | Muestra PC en hexadecimal |
| `regs` | Muestra los 32 registros |
| `regs x5 a0 sp` | Muestra registros individuales (soporta alias ABI) |
| `step` | Ejecuta una sola instrucción |
| `step 3` | Ejecuta tres instrucciones controladamente |
| `run` | Ejecuta hasta `ebreak`, `ecall` de fin, error o límite |
| `run 2000` | Máximo 2000 instrucciones en esa ejecución |
| `mem 0x1000 0x1007` | Muestra 8 bytes de memoria en orden de direcciones |
| `word 0x1000 7` | Muestra 7 palabras de 32 bits |
| `disasm 8` | Muestra las siguientes ocho instrucciones decodificadas |
| `set x5 42` | Cambia el registro `x5` |
| `setpc 0x0` | Modifica el PC |
| `setsp 0x100000` | Cambia el valor inicial y actual del puntero de pila |
| `load tests/03_memoria_little_endian.bin` | Carga otro programa RAW |
| `reset` | Restablece binario, registros y PC |
| `status` | Muestra total de instrucciones y estado |
| `exit` | Sale de la aplicación |

Los números en hexadecimal empiezan con `0x`; números sin prefijo son decimales.

## 3. Cuatro ejemplos para el video obligatorio

### Ejemplo 1 - Suma 1+2+3+4+5

```text
pc
disasm 8
step 3
regs x5 x6 x7
run
word 0x1000
exit
```

**Resultado esperado:** `word 0x1000 = 0x0000000F` (15). Muestra `addi`, `add`, `blt`, `lui` y `sw`.

### Ejemplo 2 - ALU y comparaciones signed/unsigned

Ejecutar con `tests/02_alu_y_comparaciones.bin`:

```text
step 4
regs x1 x2 x3 x4
run
regs x0 x9 x10 x15 x16 x17 x21 x22
exit
```

**Resultado esperado:** `x0=0`, `x9=0x1FFFFFFF`, `x10=0xFFFFFFFF`, `x15=0xFFFFFFFE`, `x21=0x00001050`, `x22=0x12345000`.

### Ejemplo 3 - Memoria little-endian

Ejecutar con `tests/03_memoria_little_endian.bin`:

```text
step 4
mem 0x1000 0x1003
run
regs x7 x31 a0 a1 a2 a3
mem 0x1000 0x1007
exit
```

**Resultado esperado:** `78 56 34 12 80 00 80 FF`; `a0=0xFFFFFF80` y `a1=0x00000080` muestran la diferencia entre `lb` y `lbu`.

### Ejemplo 4 - Funciones con JAL/JALR

Ejecutar con `tests/04_funcion_jal_jalr.bin`:

```text
pc
step 2
regs ra a0
run
regs ra a0
word 0x1000
exit
```

**Resultado esperado:** `ra=0x00000008`, `a0=0x0000000F` y `word 0x1000=0x0000000F`. La función suma los naturales de 1 a 5 y regresa con `jalr`.

Guion más detallado en `docs/guion_video.md`.

## 4. Pruebas automáticas

Después de compilar (Linux / WSL o Windows con `python` instalado):

```bash
python3 tests/verify.py
```

En Windows, usa `python tests\verify.py`. La suite busca automáticamente `build/riscv_sim`, `build/riscv_sim.exe` o `riscv_sim.exe`.

La prueba revisa resultados de registros, memoria, signos, branches, `jal`/`jalr`, errores, `ecall`, paso a paso y reset. **10 grupos de verificaciones**. Si se desea regenerar binarios de los casos de prueba: `python3 tests/make_tests.py`. Este script Python es una herramienta auxiliar que genera bytes RAW de prueba: **no forma parte del simulador ni es un ensamblador requerido para usarlo**.

## 5. Archivos

```text
src/main.cpp                  Simulador completo
build/                       Carpeta de compilación (crear si hace falta)
tests/*.bin                   Nueve programas RAW incluidos
tests/make_tests.py           Generador auxiliar de programas de prueba
tests/verify.py               Suite de pruebas
docs/guion_video.md           Guion para grabar cuatro casos
docs/informe_entrega.md       Texto editable del informe
docs/datos_entrega.json       Completar los enlaces propios
Informe_HW5_RV32I.pdf         Informe en PDF, con enlaces pendientes
```

## 6. Cómo hacer la entrega final

El enunciado pide **un PDF en Canvas** con un enlace al repositorio Git, un enlace a una carpeta de Google Drive que contenga **un video con cuatro ejecuciones + el código fuente**, y un breve informe.

1. Subir el contenido de este proyecto a un repositorio propio de GitHub.
2. Grabar las cuatro demostraciones en tu computadora con los programas de la sección 3 (no se incluye ningún video pregrabado).
3. Subir el video y el archivo ZIP de código a Google Drive y comprobar los permisos de lectura.
4. Reemplazar los campos **PENDIENTE** del informe PDF con los dos enlaces reales. Se adjunta el contenido editable en Markdown.
5. Entregar **solo el PDF actualizado** por Canvas, de acuerdo con el documento.

**Verificación realizada:** los 10 grupos automáticos fueron ejecutados con el compilador `g++ 14.2` en Linux. No se ha verificado el funcionamiento en una PC Windows concreta, ni se han ejecutado todavía los tres programas externos del apartado «Consideraciones» del enunciado. Los 4 programas de demostración son pruebas originales del proyecto, no copias del Pastebin externo.
