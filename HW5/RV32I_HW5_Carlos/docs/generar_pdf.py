"""Generar el informe PDF desde datos_entrega.json.
Para actualizar enlaces: editar docs/datos_entrega.json y ejecutar:
    python docs/generar_pdf.py
Depende de: pip install reportlab
"""
from pathlib import Path
from html import escape
import json

from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import cm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, LongTable, TableStyle, KeepTogether, PageBreak

ROOT = Path(__file__).resolve().parents[1]
PDF = ROOT / 'Informe_HW5_RV32I.pdf'
DATA = json.loads((Path(__file__).resolve().parent / 'datos_entrega.json').read_text(encoding='utf-8'))

# Estas fuentes solo se usan internamente para generar el PDF, no se distribuyen.
fontfile = Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
boldfile = Path('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf')
if fontfile.exists() and boldfile.exists():
    pdfmetrics.registerFont(TTFont('DejaVu', str(fontfile)))
    pdfmetrics.registerFont(TTFont('DejaVu-Bold', str(boldfile)))
    NORMAL, BOLD = 'DejaVu', 'DejaVu-Bold'
else:
    NORMAL, BOLD = 'Helvetica', 'Helvetica-Bold'

INK = colors.HexColor('#1F2937')
ACCENT = colors.HexColor('#1E3A5F')
LIGHT = colors.HexColor('#E6EAF0')

styles = getSampleStyleSheet()
styles.add(ParagraphStyle(name='CoverCustom', fontName=BOLD, fontSize=17, leading=22, textColor=ACCENT, spaceAfter=6))
styles.add(ParagraphStyle(name='SubCoverCustom', fontName=NORMAL, fontSize=9, leading=13, textColor=INK, spaceAfter=14))
styles.add(ParagraphStyle(name='HeadingCustom', fontName=BOLD, fontSize=10.7, leading=16, textColor=ACCENT, spaceBefore=12, spaceAfter=5))
styles.add(ParagraphStyle(name='BodyCustom', fontName=NORMAL, fontSize=8.7, leading=13, textColor=INK, spaceAfter=6, alignment=TA_LEFT))
styles.add(ParagraphStyle(name='SmallCustom', fontName=NORMAL, fontSize=7.4, leading=10.5, textColor=INK, spaceAfter=1))
styles.add(ParagraphStyle(name='StrongCustom', fontName=BOLD, fontSize=8.8, leading=12, textColor=INK))

P = lambda text, sty='BodyCustom': Paragraph(text, styles[sty])

def link_text(key):
    value = DATA.get(key, '[PENDIENTE]')
    if isinstance(value, str) and value.startswith(('https://', 'http://')):
        safe = escape(value)
        return f'<link href="{safe}" color="#1E3A5F"><u>{safe}</u></link>'
    return escape(str(value))

story = []
story += [P('HW#5 - Simulador del ISA RISC-V (RV32I)', 'CoverCustom'),
          P('CS3051 Arquitectura de Computadores · 2026-II · Informe breve de implementación', 'SubCoverCustom'),
          P('<b>Integrantes:</b> ' + escape(DATA.get('integrantes','[PENDIENTE]'))),
          P('<b>Repositorio Git:</b> ' + link_text('repositorio_git')),
          P('<b>Google Drive (video de 4 demostraciones y código fuente):</b> ' + link_text('drive_video_codigo'))]

story += [P('1. Objetivo', 'HeadingCustom'),
          P('Implementar un simulador funcional del ISA base RISC-V de 32 bits que cargue código de máquina desde archivos binarios crudos (RAW) en la dirección 0x00000000, permita avanzar instrucción por instrucción y facilite la inspección del contador de programa (PC), los 32 registros y la memoria.')]

story += [P('2. Arquitectura del simulador', 'HeadingCustom'),
          P('Se implementó en <b>C++17</b>. La clase <b>Simulator</b> mantiene el PC, los 32 registros de 32 bits, un contador de instrucciones y una memoria plana representada como mapa disperso de bytes (<b>unordered_map&lt;uint32_t, uint8_t&gt;</b>). Las lecturas y escrituras multibyte respetan el orden <b>little-endian</b>. Las escrituras sobre x0 se descartan.'),
          P('Cada paso realiza <b>fetch → decode → execute → actualización de registros/memoria → actualización del PC</b>. La decodificación utiliza opcode, funct3, funct7 e inmediatos de los formatos R, I, S, B, U y J. No se intenta simular el pipeline ni el tiempo de propagación de hardware.')]

story += [P('3. Funcionalidades', 'HeadingCustom'),
          P('<b>RV32I:</b> se implementaron las 37 instrucciones del anexo A: 5 cargas, 9 operaciones con inmediato, auipc, 3 stores, 10 operaciones de registro, lui, 6 branches y 2 saltos (jal y jalr).'),
          P('<b>Consola:</b> comandos <b>step</b>, <b>run</b>, <b>pc</b>, <b>regs</b>, <b>mem</b>, <b>word</b>, <b>disasm</b>, <b>reset</b>, <b>set</b>, <b>setpc</b> y <b>load</b>. Se reportan errores por instrucciones inválidas y accesos desalineados. Se añadieron ebreak para detener pruebas y un conjunto limitado de ecall opcionales.')]

story += [P('4. Ejemplos y resultados verificados', 'HeadingCustom')]
rows = [
    ('Programa RAW', 'Verificación realizada', 'Resultado'),
    ('01_suma_1_a_5.bin', 'Bucle y sumas', 'mem[0x1000] = 15'),
    ('02_alu_y_comparaciones.bin', 'ALU, shifts y comparaciones', 'x10 = 0xFFFFFFFF; x22 = 0x12345000'),
    ('03_memoria_little_endian.bin', 'Load/store y extensión de signo', '78 56 34 12 en 0x1000..1003; a0=-128'),
    ('04_funcion_jal_jalr.bin', 'Llamada y retorno a función', 'ra = 8; a0 = 15; mem[0x1000] = 15'),
    ('05_branches_signed_~unsigned.bin', 'Condiciones de salto', 'x20 = 37'),
    ('06 y 07 (errores)', 'Encoding y alineación inválidos', 'TRAP explícito'),
    ('08_riscvtest_propio.bin', 'Carga, ALU y saltos', 'mem[100] = 25 (0x19)'),
    ('09_ecall_opcional.bin', 'ECALL didáctica', 'Imprime 25')
]
colw = [4.7*cm, 6.0*cm, 6.7*cm]
table = LongTable([[P(escape(cell).replace('~','<br/>'), 'StrongCustom' if i==0 else 'SmallCustom') for cell in row]
                   for i,row in enumerate(rows)], colWidths=colw, repeatRows=1,
                  hAlign='LEFT')
table.setStyle(TableStyle([
    ('BACKGROUND', (0,0), (-1,0), LIGHT),
    ('LINEBELOW', (0,0), (-1,0), .7, ACCENT),
    ('LINEBELOW', (0,1), (-1,-1), .25, colors.HexColor('#CDD4DD')),
    ('VALIGN', (0,0), (-1,-1), 'TOP'),
    ('TOPPADDING', (0,0), (-1,-1), 5),
    ('BOTTOMPADDING', (0,0), (-1,-1), 5),
    ('LEFTPADDING', (0,0), (-1,-1), 5),
    ('RIGHTPADDING', (0,0), (-1,-1), 5),
]))
story.append(table)
story.append(Spacer(1, 8))
story.append(P('<b>Verificación:</b> se ejecutaron 10 grupos de pruebas automáticas sin errores con g++ 14.2 en Linux. Los programas de demostración fueron elaborados específicamente para este proyecto; no son los ejemplos externos originales enlazados en el enunciado.'))

story.append(PageBreak())
story += [P('5. Instrucciones de uso', 'HeadingCustom'),
          P('<b>Compilar:</b> g++ -std=c++17 -O2 src/main.cpp -o riscv_sim'),
          P('<b>Ejecutar:</b> ./riscv_sim tests/01_suma_1_a_5.bin'),
          P('<b>Comandos de ejemplo:</b> step · regs x5 x6 · run · word 0x1000 · exit. El README y guion adjuntos detallan los cuatro ejemplos para el video.'),
          P('Demostraciones propuestas para el video', 'HeadingCustom')]

demo_rows = [
    ('Demostración', 'Comandos durante la grabación'),
    ('1 · Bucle de suma', 'pc · step 3 · regs x5 x6 · run · word 0x1000'),
    ('2 · ALU signed/unsigned', 'step 4 · regs x1 x2 x3 x4 · run · regs x10 x15'),
    ('3 · Little-endian', 'step 4 · mem 0x1000 0x1003 · run · regs a0 a1'),
    ('4 · JAL / JALR', 'step 2 · regs ra a0 · run · word 0x1000')
]
demo_table = LongTable([[P(escape(cell), 'StrongCustom' if i==0 else 'SmallCustom') for cell in row]
                        for i,row in enumerate(demo_rows)], colWidths=[5.0*cm,12.4*cm])
demo_table.setStyle(TableStyle([
    ('BACKGROUND',(0,0),(-1,0),LIGHT),
    ('LINEBELOW',(0,0),(-1,0),.7,ACCENT),
    ('LINEBELOW',(0,1),(-1,-1),.25,colors.HexColor('#CDD4DD')),
    ('VALIGN',(0,0),(-1,-1),'TOP'),
    ('TOPPADDING',(0,0),(-1,-1),6),('BOTTOMPADDING',(0,0),(-1,-1),6)
]))
story.append(demo_table)
story.append(Spacer(1, 10))

story += [P('6. Limitaciones y conclusión', 'HeadingCustom'),
          P('No se admite ELF, GDB ni extensiones distintas de RV32I. La representación de memoria es dispersa y las operaciones ecall/ebreak son simplificaciones de consola. El simulador muestra de manera controlada cómo las instrucciones afectan al PC, a los registros y a la memoria; las pruebas verifican los principales comportamientos exigidos en el encargo.')]

if not all(str(DATA.get(k,'')).startswith('https://') for k in ('repositorio_git','drive_video_codigo')):
    story.append(P('<b>Importante:</b> completar los dos enlaces y los datos del equipo antes de enviar el documento por Canvas. El video debe grabarse y subirse a Google Drive.', 'SmallCustom'))


def footer(canvas, document):
    canvas.setStrokeColor(colors.HexColor('#CCD4DE'))
    w,h = A4
    canvas.line(1.8*cm, 1.42*cm, w-1.8*cm, 1.42*cm)
    canvas.setFont(NORMAL, 7.4)
    canvas.setFillColor(colors.HexColor('#566273'))
    canvas.drawString(1.8*cm, 1.09*cm, 'CS3051 · HW#5 · Simulador RV32I')
    canvas.drawRightString(w-1.8*cm, 1.09*cm, f'Página {document.page}')

pdf = SimpleDocTemplate(str(PDF), pagesize=A4, leftMargin=1.8*cm, rightMargin=1.8*cm,
                        topMargin=1.9*cm, bottomMargin=1.9*cm, title='HW#5 - Simulador RV32I',
                        author='Equipo de Arquitectura de Computadores')
pdf.build(story, onFirstPage=footer, onLaterPages=footer)
print(PDF)
