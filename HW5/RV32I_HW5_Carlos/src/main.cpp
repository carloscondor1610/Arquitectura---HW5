// HW#5 - Simulador RV32I para CS3051 Arquitectura de Computadores
// C++17. La entrada del simulador es codigo de maquina RAW, NO assembly.
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

static string hex32(uint32_t v) {
    ostringstream out;
    out << "0x" << uppercase << hex << setfill('0') << setw(8) << v;
    return out.str();
}
static string hex8(uint8_t v) {
    ostringstream out;
    out << uppercase << hex << setfill('0') << setw(2) << unsigned(v);
    return out.str();
}
static int32_t signExtend(uint32_t value, unsigned bits) {
    const uint32_t sign = uint32_t(1) << (bits - 1);
    const uint32_t mask = (uint32_t(1) << bits) - 1;
    value &= mask;
    if (value & sign) value |= ~mask;
    return static_cast<int32_t>(value);
}
static uint32_t parseNumber(const string &token) {
    size_t n = 0;
    unsigned long long v = stoull(token, &n, 0);
    if (n != token.size() || v > UINT32_MAX) throw runtime_error("Numero fuera de rango: " + token);
    return static_cast<uint32_t>(v);
}
static string regName(unsigned r) { return "x" + to_string(r); }
static unsigned parseReg(string name) {
    if (!name.empty() && name[0] == 'x') {
        auto v = parseNumber(name.substr(1));
        if (v <= 31) return v;
    }
    static const unordered_map<string, unsigned> abi = {
        {"zero", 0}, {"ra", 1}, {"sp", 2}, {"gp", 3}, {"tp", 4},
        {"t0", 5}, {"t1", 6}, {"t2", 7}, {"s0", 8}, {"fp", 8}, {"s1", 9},
        {"a0", 10}, {"a1", 11}, {"a2", 12}, {"a3", 13}, {"a4", 14}, {"a5", 15},
        {"a6", 16}, {"a7", 17}, {"s2", 18}, {"s3", 19}, {"s4", 20}, {"s5", 21},
        {"s6", 22}, {"s7", 23}, {"s8", 24}, {"s9", 25}, {"s10", 26}, {"s11", 27},
        {"t3", 28}, {"t4", 29}, {"t5", 30}, {"t6", 31}
    };
    auto it = abi.find(name);
    if (it != abi.end()) return it->second;
    throw runtime_error("Registro no valido: " + name);
}

enum class Op {
    Invalid, LB, LH, LW, LBU, LHU, ADDI, SLLI, SLTI, SLTIU, XORI, SRLI, SRAI, ORI,
    ANDI, AUIPC, SB, SH, SW, ADD, SUB, SLL, SLT, SLTU, XOR, SRL, SRA, OR, AND, LUI,
    BEQ, BNE, BLT, BGE, BLTU, BGEU, JALR, JAL, ECALL, EBREAK
};
struct Decoded {
    Op op = Op::Invalid;
    unsigned rd = 0, rs1 = 0, rs2 = 0;
    int32_t imm = 0;
    uint32_t raw = 0;
    string name;
    string error;
};

static Decoded decode(uint32_t raw) {
    Decoded d;
    d.raw = raw;
    d.rd = (raw >> 7) & 31;
    const unsigned funct3 = (raw >> 12) & 7;
    d.rs1 = (raw >> 15) & 31;
    d.rs2 = (raw >> 20) & 31;
    const unsigned funct7 = (raw >> 25) & 127;
    auto set = [&](Op op, const char *name) { d.op = op; d.name = name; };
    const unsigned opcode = raw & 127;
    switch (opcode) {
        case 0x03: // Loads
            d.imm = signExtend(raw >> 20, 12);
            switch (funct3) {
                case 0: set(Op::LB, "lb"); break;
                case 1: set(Op::LH, "lh"); break;
                case 2: set(Op::LW, "lw"); break;
                case 4: set(Op::LBU, "lbu"); break;
                case 5: set(Op::LHU, "lhu"); break;
            }
            break;
        case 0x13: // OP-IMM
            d.imm = signExtend(raw >> 20, 12);
            switch (funct3) {
                case 0: set(Op::ADDI, "addi"); break;
                case 1:
                    if (funct7 == 0) { set(Op::SLLI, "slli"); d.imm = d.rs2; }
                    break;
                case 2: set(Op::SLTI, "slti"); break;
                case 3: set(Op::SLTIU, "sltiu"); break;
                case 4: set(Op::XORI, "xori"); break;
                case 5:
                    if (funct7 == 0) { set(Op::SRLI, "srli"); d.imm = d.rs2; }
                    if (funct7 == 0x20) { set(Op::SRAI, "srai"); d.imm = d.rs2; }
                    break;
                case 6: set(Op::ORI, "ori"); break;
                case 7: set(Op::ANDI, "andi"); break;
            }
            break;
        case 0x17: set(Op::AUIPC, "auipc"); d.imm = static_cast<int32_t>(raw & 0xFFFFF000u); break;
        case 0x23: // Stores
            d.imm = signExtend(((raw >> 25) << 5) | ((raw >> 7) & 31), 12);
            switch (funct3) {
                case 0: set(Op::SB, "sb"); break;
                case 1: set(Op::SH, "sh"); break;
                case 2: set(Op::SW, "sw"); break;
            }
            break;
        case 0x33: // OP R-type
            switch (funct3) {
                case 0:
                    if (funct7 == 0) set(Op::ADD, "add");
                    if (funct7 == 0x20) set(Op::SUB, "sub");
                    break;
                case 1: if (funct7 == 0) set(Op::SLL, "sll"); break;
                case 2: if (funct7 == 0) set(Op::SLT, "slt"); break;
                case 3: if (funct7 == 0) set(Op::SLTU, "sltu"); break;
                case 4: if (funct7 == 0) set(Op::XOR, "xor"); break;
                case 5:
                    if (funct7 == 0) set(Op::SRL, "srl");
                    if (funct7 == 0x20) set(Op::SRA, "sra");
                    break;
                case 6: if (funct7 == 0) set(Op::OR, "or"); break;
                case 7: if (funct7 == 0) set(Op::AND, "and"); break;
            }
            break;
        case 0x37: set(Op::LUI, "lui"); d.imm = static_cast<int32_t>(raw & 0xFFFFF000u); break;
        case 0x63: // Branch offsets with bit 0 = 0
            d.imm = signExtend(
                (((raw >> 31) & 1) << 12) | (((raw >> 7) & 1) << 11) |
                (((raw >> 25) & 63) << 5) | (((raw >> 8) & 15) << 1), 13);
            switch (funct3) {
                case 0: set(Op::BEQ, "beq"); break;
                case 1: set(Op::BNE, "bne"); break;
                case 4: set(Op::BLT, "blt"); break;
                case 5: set(Op::BGE, "bge"); break;
                case 6: set(Op::BLTU, "bltu"); break;
                case 7: set(Op::BGEU, "bgeu"); break;
            }
            break;
        case 0x67:
            if (funct3 == 0) { set(Op::JALR, "jalr"); d.imm = signExtend(raw >> 20, 12); }
            break;
        case 0x6F: // J offsets with bit 0 = 0
            set(Op::JAL, "jal");
            d.imm = signExtend(
                (((raw >> 31) & 1) << 20) | (((raw >> 12) & 255) << 12) |
                (((raw >> 20) & 1) << 11) | (((raw >> 21) & 1023) << 1), 21);
            break;
        case 0x73:
            if (raw == 0x00000073) set(Op::ECALL, "ecall");
            if (raw == 0x00100073) set(Op::EBREAK, "ebreak");
            break;
    }
    if (d.op == Op::Invalid) d.error = "Instruccion no soportada o encoding invalido: " + hex32(raw);
    return d;
}
static string disassemble(const Decoded &d, uint32_t at) {
    if (d.op == Op::Invalid) return "INVALIDA " + hex32(d.raw);
    const string rd = regName(d.rd), a = regName(d.rs1), b = regName(d.rs2);
    switch (d.op) {
        case Op::LB: case Op::LH: case Op::LW: case Op::LBU: case Op::LHU:
            return d.name + " " + rd + ", " + to_string(d.imm) + "(" + a + ")";
        case Op::SB: case Op::SH: case Op::SW:
            return d.name + " " + b + ", " + to_string(d.imm) + "(" + a + ")";
        case Op::ADDI: case Op::SLLI: case Op::SLTI: case Op::SLTIU: case Op::XORI:
        case Op::SRLI: case Op::SRAI: case Op::ORI: case Op::ANDI:
            return d.name + " " + rd + ", " + a + ", " + to_string(d.imm);
        case Op::ADD: case Op::SUB: case Op::SLL: case Op::SLT: case Op::SLTU:
        case Op::XOR: case Op::SRL: case Op::SRA: case Op::OR: case Op::AND:
            return d.name + " " + rd + ", " + a + ", " + b;
        case Op::AUIPC: case Op::LUI:
            return d.name + " " + rd + ", " + hex32(static_cast<uint32_t>(d.imm) >> 12);
        case Op::BEQ: case Op::BNE: case Op::BLT: case Op::BGE: case Op::BLTU: case Op::BGEU:
            return d.name + " " + a + ", " + b + ", " + hex32(at + static_cast<uint32_t>(d.imm));
        case Op::JAL:
            return "jal " + rd + ", " + hex32(at + static_cast<uint32_t>(d.imm));
        case Op::JALR:
            return "jalr " + rd + ", " + to_string(d.imm) + "(" + a + ")";
        case Op::ECALL: case Op::EBREAK:
            return d.name;
        default: return "INVALIDA";
    }
}

class Simulator {
    // Memoria dispersa: direccionamiento de 32 bits sin reservar 4 GiB fisicos.
    unordered_map<uint32_t, uint8_t> memory;
    vector<uint8_t> image;
    array<uint32_t, 32> x{};
    uint32_t pc_ = 0;
    uint64_t executed_ = 0;
    bool halted_ = false;
    string reason_;
    uint32_t stackPointer_ = 0x00100000;

    uint8_t getByte(uint32_t addr) const {
        auto it = memory.find(addr);
        return it == memory.end() ? 0 : it->second;
    }
    void putByte(uint32_t addr, uint8_t value) {
        if (value == 0) memory.erase(addr);
        else memory[addr] = value;
    }
    uint32_t readLittle(uint32_t addr, unsigned n) const {
        uint32_t v = 0;
        for (unsigned i = 0; i < n; ++i) v |= uint32_t(getByte(addr + i)) << (8 * i);
        return v;
    }
    void writeLittle(uint32_t addr, unsigned n, uint32_t value) {
        for (unsigned i = 0; i < n; ++i) putByte(addr + i, uint8_t(value >> (8 * i)));
    }
    void checkAlignment(uint32_t addr, unsigned n, const string &op) const {
        if (addr % n) throw runtime_error(op + ": acceso desalineado en " + hex32(addr));
    }
public:
    bool loadRaw(const string &filename) {
        ifstream file(filename, ios::binary);
        if (!file) throw runtime_error("No se puede abrir: " + filename);
        vector<uint8_t> bytes{istreambuf_iterator<char>(file), istreambuf_iterator<char>()};
        if (bytes.empty()) throw runtime_error("El binario esta vacio");
        if (bytes.size() > 16'777'216) throw runtime_error("Archivo demasiado grande (>16 MiB)");
        if (bytes.size() >= 4 && bytes[0] == 0x7F && bytes[1] == 'E' && bytes[2] == 'L' && bytes[3] == 'F')
            throw runtime_error("Se recibio un ELF. Exporta formato RAW desde CPUlator (anexo B)");
        image = move(bytes);
        reset();
        cout << "Cargado " << filename << " (" << image.size() << " bytes) desde " << hex32(0) << "\n";
        return true;
    }
    void reset() {
        memory.clear();
        for (size_t i = 0; i < image.size(); ++i) putByte(static_cast<uint32_t>(i), image[i]);
        x.fill(0);
        x[2] = stackPointer_;
        pc_ = 0;
        executed_ = 0;
        halted_ = false;
        reason_.clear();
    }
    uint32_t pc() const { return pc_; }
    bool halted() const { return halted_; }
    const string &reason() const { return reason_; }
    uint64_t executed() const { return executed_; }
    uint32_t reg(unsigned r) const { return x.at(r); }
    uint32_t word(uint32_t addr) const { checkAlignment(addr, 4, "word"); return readLittle(addr, 4); }
    uint8_t byte(uint32_t addr) const { return getByte(addr); }
    void setReg(unsigned r, uint32_t value) {
        if (r == 0) throw runtime_error("x0 es constante y siempre vale cero");
        x.at(r) = value;
    }
    void setPC(uint32_t value) {
        if (value % 4) throw runtime_error("El PC debe ser multiplo de 4 en RV32I");
        pc_ = value;
        halted_ = false;
        reason_.clear();
    }
    void setSP(uint32_t value) { stackPointer_ = value; x[2] = value; }
    void memoryDump(uint32_t start, uint32_t end) const {
        if (end < start || uint64_t(end) - start + 1 > 256)
            throw runtime_error("Rango invalido. Maximo 256 bytes por consulta");
        for (uint64_t base = start; base <= end; base += 16) {
            cout << hex32(static_cast<uint32_t>(base)) << ": ";
            for (uint64_t addr = base; addr < base + 16 && addr <= end; ++addr)
                cout << hex8(getByte(static_cast<uint32_t>(addr))) << ' ';
            cout << '\n';
        }
    }
    void printRegs() const {
        for (unsigned i = 0; i < 32; ++i) {
            cout << left << setw(4) << regName(i) << "=" << hex32(x[i])
                 << (i % 4 == 3 ? "\n" : "    ");
        }
    }
    void showDisasm(unsigned amount) const {
        if (amount > 80) throw runtime_error("Maximo 80 instrucciones por consulta");
        for (unsigned i = 0; i < amount; ++i) {
            uint32_t at = pc_ + i * 4u;
            uint32_t raw = readLittle(at, 4);
            cout << hex32(at) << "  " << hex32(raw) << "  " << disassemble(decode(raw), at) << '\n';
        }
    }
    bool step(bool verbose = true) {
        if (halted_) {
            if (verbose) cout << "Ejecucion detenida: " << reason_ << '\n';
            return false;
        }
        try {
            checkAlignment(pc_, 4, "fetch");
            const uint32_t originalPC = pc_;
            const uint32_t instruction = readLittle(pc_, 4);
            const Decoded d = decode(instruction);
            if (d.op == Op::Invalid) throw runtime_error(d.error);
            const uint32_t a = x[d.rs1], b = x[d.rs2];
            const uint32_t imm = static_cast<uint32_t>(d.imm);
            uint32_t nextPC = pc_ + 4;
            uint32_t result = 0;
            bool writeRegister = false;
            bool writeMemory = false;
            uint32_t memoryAddr = 0, memoryValue = 0;
            unsigned memoryWidth = 0;
            bool jumped = false;
            switch (d.op) {
                case Op::LB: case Op::LH: case Op::LW: case Op::LBU: case Op::LHU: {
                    const unsigned n = (d.op == Op::LB || d.op == Op::LBU) ? 1 :
                        ((d.op == Op::LH || d.op == Op::LHU) ? 2 : 4);
                    const uint32_t addr = a + imm;
                    checkAlignment(addr, n, d.name);
                    const uint32_t v = readLittle(addr, n);
                    result = (d.op == Op::LB) ? uint32_t(signExtend(v, 8)) :
                             (d.op == Op::LH) ? uint32_t(signExtend(v, 16)) : v;
                    writeRegister = true;
                    break;
                }
                case Op::SB: case Op::SH: case Op::SW:
                    memoryWidth = d.op == Op::SB ? 1 : (d.op == Op::SH ? 2 : 4);
                    memoryAddr = a + imm;
                    checkAlignment(memoryAddr, memoryWidth, d.name);
                    memoryValue = b;
                    writeMemory = true;
                    break;
                case Op::ADDI: result = a + imm; writeRegister = true; break;
                case Op::SLLI: result = a << (unsigned(d.imm) & 31u); writeRegister = true; break;
                case Op::SLTI: result = int32_t(a) < d.imm; writeRegister = true; break;
                case Op::SLTIU: result = a < imm; writeRegister = true; break;
                case Op::XORI: result = a ^ imm; writeRegister = true; break;
                case Op::SRLI: result = a >> (unsigned(d.imm) & 31u); writeRegister = true; break;
                case Op::SRAI: result = uint32_t(int32_t(a) >> (unsigned(d.imm) & 31u)); writeRegister = true; break;
                case Op::ORI: result = a | imm; writeRegister = true; break;
                case Op::ANDI: result = a & imm; writeRegister = true; break;
                case Op::AUIPC: result = originalPC + imm; writeRegister = true; break;
                case Op::ADD: result = a + b; writeRegister = true; break;
                case Op::SUB: result = a - b; writeRegister = true; break;
                case Op::SLL: result = a << (b & 31u); writeRegister = true; break;
                case Op::SLT: result = int32_t(a) < int32_t(b); writeRegister = true; break;
                case Op::SLTU: result = a < b; writeRegister = true; break;
                case Op::XOR: result = a ^ b; writeRegister = true; break;
                case Op::SRL: result = a >> (b & 31u); writeRegister = true; break;
                case Op::SRA: result = uint32_t(int32_t(a) >> (b & 31u)); writeRegister = true; break;
                case Op::OR: result = a | b; writeRegister = true; break;
                case Op::AND: result = a & b; writeRegister = true; break;
                case Op::LUI: result = imm; writeRegister = true; break;
                case Op::BEQ: jumped = (a == b); break;
                case Op::BNE: jumped = (a != b); break;
                case Op::BLT: jumped = int32_t(a) < int32_t(b); break;
                case Op::BGE: jumped = int32_t(a) >= int32_t(b); break;
                case Op::BLTU: jumped = a < b; break;
                case Op::BGEU: jumped = a >= b; break;
                case Op::JAL: result = originalPC + 4; writeRegister = true;
                    nextPC = originalPC + imm; break;
                case Op::JALR: result = originalPC + 4; writeRegister = true;
                    nextPC = (a + imm) & ~uint32_t(1); break;
                case Op::ECALL: {
                    // Llamadas de entorno didacticas SPIM (opcionales).
                    const uint32_t service = x[17];
                    if (service == 10 || service == 93) {
                        halted_ = true; reason_ = "ECALL de terminacion";
                    } else if (service == 1) {
                        cout << "[ecall] " << int32_t(x[10]) << '\n';
                    } else if (service == 11) {
                        cout << char(x[10] & 0xFFu) << flush;
                    } else if (service == 4) {
                        uint32_t addr = x[10];
                        cout << "[ecall] ";
                        bool ended = false;
                        for (unsigned i = 0; i < 4096; ++i) {
                            uint8_t ch = getByte(addr + i);
                            if (ch == 0) { ended = true; break; }
                            cout << char(ch);
                        }
                        cout << '\n';
                        if (!ended) throw runtime_error("Texto ecall sin terminador (limite: 4096 bytes)");
                    } else throw runtime_error("ECALL no implementada (a7=" + to_string(service) + ")");
                    break;
                }
                case Op::EBREAK: halted_ = true; reason_ = "EBREAK: fin de la demostracion"; break;
                default: throw runtime_error("Operacion sin implementacion");
            }
            if (jumped) nextPC = originalPC + imm;
            // Deteccion de traps ANTES de modificar registros/memoria.
            if (!halted_ && nextPC % 4) throw runtime_error("Destino de salto desalineado: " + hex32(nextPC));
            if (writeMemory) writeLittle(memoryAddr, memoryWidth, memoryValue);
            if (writeRegister && d.rd != 0) x[d.rd] = result;
            x[0] = 0;
            pc_ = nextPC;
            ++executed_;
            if (verbose) {
                cout << hex32(originalPC) << "  " << hex32(instruction) << "  "
                     << disassemble(d, originalPC);
                if (writeRegister && d.rd != 0) cout << "  | " << regName(d.rd) << "=" << hex32(x[d.rd]);
                if (writeMemory) cout << "  | mem[" << hex32(memoryAddr) << "] <- "
                                      << hex32(memoryValue) << " (" << memoryWidth << " B)";
                if (jumped) cout << "  | branch tomado";
                if (halted_) cout << "  | HALT";
                cout << "  | PC=" << hex32(pc_) << '\n';
            }
            return !halted_;
        } catch (const exception &e) {
            halted_ = true;
            reason_ = "TRAP en PC " + hex32(pc_) + ": " + e.what();
            if (verbose) cerr << reason_ << '\n';
            return false;
        }
    }
    void run(uint64_t maxSteps) {
        uint64_t count = 0;
        while (count < maxSteps && !halted_) {
            step(false);
            ++count;
        }
        cout << "Instrucciones ejecutadas en este comando: " << count << '\n';
        cout << "PC=" << hex32(pc_) << "  total=" << executed_ << '\n';
        if (halted_) cout << reason_ << '\n';
        else cout << "Limite alcanzado: usa 'step' o 'run' para continuar.\n";
    }
};

static void showHelp() {
    cout << "Comandos disponibles:\n"
         << "  help                  Mostrar esta ayuda\n"
         << "  pc                    Ver el Program Counter\n"
         << "  regs                  Ver los 32 registros\n"
         << "  regs x5 a0 sp         Ver registros especificos (tambien alias ABI)\n"
         << "  mem 0x1000 0x101F     Ver bytes de memoria (maximo 256)\n"
         << "  word 0x1000 7         Ver 7 palabras de 32 bits desde esa direccion\n"
         << "  step [n]              Ejecutar n instrucciones, 1 por defecto\n"
         << "  run [n]               Ejecutar hasta HALT o maximo n instrucciones (100000 por defecto)\n"
         << "  disasm [n]            Desensamblar n instrucciones desde PC\n"
         << "  set x5 42             Modificar un registro (no x0)\n"
         << "  setpc 0x00000000      Modificar PC\n"
         << "  setsp 0x00100000     Configurar SP actual y para los proximos resets\n"
         << "  load programa.bin     Cargar otro binario RAW desde direccion 0\n"
         << "  reset                 Restaurar binario original, registros y PC\n"
         << "  status                Ver estado de ejecucion\n"
         << "  exit                  Salir\n";
}

int main(int argc, char **argv) {
    Simulator cpu;
    cout << "=================================================\n"
         << " HW#5 - SIMULADOR RISC-V RV32I (32 bits)\n"
         << "=================================================\n";
    try {
        if (argc >= 2) cpu.loadRaw(argv[1]);
        else cout << "Uso: ./riscv_sim archivo.bin  (o usa 'load archivo.bin')\n";
    } catch (const exception &e) { cerr << "Error: " << e.what() << '\n'; }
    showHelp();
    string line;
    while (true) {
        cout << "rv32i> " << flush;
        if (!getline(cin, line)) break;
        istringstream iss(line);
        vector<string> a;
        string token;
        while (iss >> token) a.push_back(token);
        if (a.empty()) continue;
        try {
            const string &cmd = a[0];
            if (cmd == "exit" || cmd == "quit" || cmd == "q") break;
            if (cmd == "help" || cmd == "h") showHelp();
            else if (cmd == "pc") cout << "pc = " << hex32(cpu.pc()) << '\n';
            else if (cmd == "regs") {
                if (a.size() == 1) cpu.printRegs();
                else for (size_t i = 1; i < a.size(); ++i) {
                    unsigned r = parseReg(a[i]);
                    cout << regName(r) << " = " << hex32(cpu.reg(r)) << '\n';
                }
            } else if (cmd == "mem") {
                if (a.size() != 3) throw runtime_error("Uso: mem inicio fin");
                cpu.memoryDump(parseNumber(a[1]), parseNumber(a[2]));
            } else if (cmd == "word") {
                if (a.size() < 2 || a.size() > 3) throw runtime_error("Uso: word direccion [cantidad]");
                uint32_t start = parseNumber(a[1]);
                unsigned count = a.size() == 3 ? parseNumber(a[2]) : 1;
                if (count > 64) throw runtime_error("Maximo 64 palabras");
                for (unsigned i = 0; i < count; ++i) {
                    uint32_t addr = start + 4u * i;
                    cout << hex32(addr) << " = " << hex32(cpu.word(addr))
                         << " (signed " << int32_t(cpu.word(addr)) << ")\n";
                }
            } else if (cmd == "step" || cmd == "s") {
                uint32_t count = a.size() >= 2 ? parseNumber(a[1]) : 1;
                if (count > 1000) throw runtime_error("step admite maximo 1000 instrucciones");
                for (unsigned i = 0; i < count; ++i) {
                    if (!cpu.step()) break;
                }
            } else if (cmd == "run") {
                uint32_t count = a.size() >= 2 ? parseNumber(a[1]) : 100000;
                if (count > 1'000'000) throw runtime_error("run admite maximo 1000000 instrucciones");
                cpu.run(count);
            } else if (cmd == "disasm" || cmd == "d") {
                unsigned count = a.size() >= 2 ? parseNumber(a[1]) : 8;
                cpu.showDisasm(count);
            } else if (cmd == "set") {
                if (a.size() != 3) throw runtime_error("Uso: set x5 42");
                unsigned r = parseReg(a[1]);
                cpu.setReg(r, parseNumber(a[2]));
                cout << regName(r) << " = " << hex32(cpu.reg(r)) << '\n';
            } else if (cmd == "setpc") {
                if (a.size() != 2) throw runtime_error("Uso: setpc 0x0");
                cpu.setPC(parseNumber(a[1]));
                cout << "PC = " << hex32(cpu.pc()) << '\n';
            } else if (cmd == "setsp") {
                if (a.size() != 2) throw runtime_error("Uso: setsp 0x100000");
                cpu.setSP(parseNumber(a[1]));
                cout << "SP = " << hex32(cpu.reg(2)) << '\n';
            } else if (cmd == "load") {
                if (a.size() != 2) throw runtime_error("Uso: load programa.bin");
                cpu.loadRaw(a[1]);
            } else if (cmd == "reset") {
                cpu.reset(); cout << "Simulador reiniciado. PC = " << hex32(cpu.pc()) << '\n';
            } else if (cmd == "status") {
                cout << "PC=" << hex32(cpu.pc()) << "  ejecutadas=" << cpu.executed()
                     << "  halted=" << (cpu.halted() ? "si" : "no") << '\n';
                if (cpu.halted()) cout << cpu.reason() << '\n';
            } else cout << "Comando desconocido. Usa 'help'.\n";
        } catch (const exception &e) {
            cout << "Error: " << e.what() << '\n';
        }
    }
    cout << "Simulador cerrado.\n";
    return 0;
}
