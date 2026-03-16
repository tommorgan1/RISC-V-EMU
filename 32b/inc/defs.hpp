#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// Constants

constexpr uint32_t MASK32 = 0xFFFFFFFF;

// Structure for decoded instructions 

struct instructionField
{
  string type;
  uint32_t    opcode;
  uint32_t    rd;
  uint32_t    rs1;
  uint32_t    rs2;
  uint32_t    funct3;
  uint32_t    funct7;
  uint32_t    imm;
};

// Opcode map

unordered_map<string, vector<uint32_t>> opcode_map
{
  {"R",    {0x33}},         // add sub and or xor sll srl sra slt sltu
  {"I",    {0x13}},         // addi andi ori xori slti sltiu slli srli srai
  {"IL",   {0x03}},         // lb lh lw lbu lhu  (I-type loads)
  {"JALR", {0x67}},         // jalr              (I-type jump)
  {"S",    {0x23}},         // sb sh sw
  {"B",    {0x63}},         // beq bne blt bge bltu bgeu
  {"U",    {0x37, 0x17}},   // lui auipc
  {"J",    {0x6F}},         // jal
  {"SYS",  {0x73}}          // ecall ebreak mret csrrw csrrs csrrc csrrwi csrrsi csrrci
};

// CSR registers 

// Machine mode 
constexpr uint32_t MHARTID    = 0xF14;  // hardware thread ID
constexpr uint32_t MSTATUS    = 0x300;  // global interrupt enable + privilege state
constexpr uint32_t MIE        = 0x304;  // interrupt enable bits
constexpr uint32_t MTVEC      = 0x305;  // trap handler base address

constexpr uint32_t MSCRATCH   = 0x340;  // scratch register for trap handler
constexpr uint32_t MEPC       = 0x341;  // PC saved on trap entry
constexpr uint32_t MCAUSE     = 0x342;  // trap cause (interrupt bit + exception code)
constexpr uint32_t MTVAL      = 0x343;  // bad address or instruction on trap
constexpr uint32_t MIP        = 0x344;  // interrupt pending bits

constexpr uint32_t MISA       = 0x301;  // ISA and extensions supported
constexpr uint32_t MEDELEG    = 0x302;  // delegate exceptions to S-mode
constexpr uint32_t MIDELEG    = 0x303;  // delegate interrupts to S-mode
constexpr uint32_t MCOUNTEREN = 0x306;  // allow S/U-mode to read counters

constexpr uint32_t MVENDORID  = 0xF11;  // vendor ID (can return 0)
constexpr uint32_t MARCHID    = 0xF12;  // architecture ID (can return 0)
constexpr uint32_t MIMPID     = 0xF13;  // implementation ID (can return 0)

// Supervisor mode 
constexpr uint32_t SSTATUS    = 0x100;  // S-mode view of MSTATUS
constexpr uint32_t SIE        = 0x104;  // S-mode interrupt enable
constexpr uint32_t STVEC      = 0x105;  // S-mode trap vector
constexpr uint32_t SCOUNTEREN = 0x106;  // allow U-mode to read counters
constexpr uint32_t SSCRATCH   = 0x140;  // S-mode scratch
constexpr uint32_t SEPC       = 0x141;  // S-mode exception PC
constexpr uint32_t SCAUSE     = 0x142;  // S-mode trap cause
constexpr uint32_t STVAL      = 0x143;  // S-mode bad address/instruction
constexpr uint32_t SIP        = 0x144;  // S-mode interrupt pending
constexpr uint32_t SATP       = 0x180;  // page table base + paging mode

// Counters
constexpr uint32_t CYCLE      = 0xC00;
constexpr uint32_t TIME       = 0xC01;
constexpr uint32_t INSTRET    = 0xC02;
