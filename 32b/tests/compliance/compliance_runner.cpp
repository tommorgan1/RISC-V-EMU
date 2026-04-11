// tests/compliance/compliance_runner.cpp
//
// Loads a riscv-tests ELF, runs it on the CPU, and checks the tohost symbol.
// Pass:  mem[tohost] == 1
// Fail:  mem[tohost] == (TESTNUM << 1) | 1  (any odd value != 1)
//
// Usage: compliance_runner <test1.elf> [test2.elf ...]
// Returns 0 if all tests pass, 1 if any fail.

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include "cpu.hpp"

// ---------------------------------------------------------------------------
// Minimal ELF32 structs — avoids depending on <elf.h> (Linux-only header)
// ---------------------------------------------------------------------------

static constexpr uint8_t  ELFMAG0      = 0x7F;
static constexpr uint8_t  ELFMAG1      = 'E';
static constexpr uint8_t  ELFMAG2      = 'L';
static constexpr uint8_t  ELFMAG3      = 'F';
static constexpr uint8_t  ELFCLASS32   = 1;
static constexpr uint8_t  ELFDATA2LSB  = 1;
static constexpr uint16_t EM_RISCV     = 243;
static constexpr uint32_t PT_LOAD      = 1;
static constexpr uint32_t SHT_SYMTAB   = 2;

struct Elf32_Ehdr {
    uint8_t  e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version, e_entry, e_phoff, e_shoff, e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum;
    uint16_t e_shentsize, e_shnum, e_shstrndx;
};

struct Elf32_Phdr {
    uint32_t p_type, p_offset, p_vaddr, p_paddr;
    uint32_t p_filesz, p_memsz, p_flags, p_align;
};

struct Elf32_Shdr {
    uint32_t sh_name, sh_type, sh_flags, sh_addr;
    uint32_t sh_offset, sh_size, sh_link, sh_info;
    uint32_t sh_addralign, sh_entsize;
};

struct Elf32_Sym {
    uint32_t st_name, st_value, st_size;
    uint8_t  st_info, st_other;
    uint16_t st_shndx;
};

// ---------------------------------------------------------------------------
// ELF loader
// ---------------------------------------------------------------------------

struct TestImage {
    std::vector<uint8_t> data;
    uint32_t mem_base    = 0;
    uint32_t mem_size    = 0;
    uint32_t tohost_addr = 0;
};

static bool parse_elf(const std::string& path, TestImage& img)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    std::vector<uint8_t> raw(std::istreambuf_iterator<char>(f), {});

    if (raw.size() < sizeof(Elf32_Ehdr)) return false;
    const auto* eh = reinterpret_cast<const Elf32_Ehdr*>(raw.data());

    if (eh->e_ident[0] != ELFMAG0 || eh->e_ident[1] != ELFMAG1 ||
        eh->e_ident[2] != ELFMAG2 || eh->e_ident[3] != ELFMAG3) return false;
    if (eh->e_ident[4] != ELFCLASS32)  return false;
    if (eh->e_ident[5] != ELFDATA2LSB) return false;
    if (eh->e_machine  != EM_RISCV)    return false;

    // Determine memory footprint from PT_LOAD segments
    uint32_t lo = UINT32_MAX, hi = 0;
    const auto* ph = reinterpret_cast<const Elf32_Phdr*>(raw.data() + eh->e_phoff);
    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        lo = std::min(lo, ph[i].p_vaddr);
        hi = std::max(hi, ph[i].p_vaddr + ph[i].p_memsz);
    }
    if (lo == UINT32_MAX) return false;

    img.mem_base = lo;
    img.mem_size = (hi - lo + 0xFFFu) & ~0xFFFu;
    img.data.assign(img.mem_size, 0);

    for (int i = 0; i < eh->e_phnum; ++i) {
        if (ph[i].p_type != PT_LOAD) continue;
        uint32_t dst = ph[i].p_vaddr - lo;
        if (dst + ph[i].p_filesz > img.data.size()) continue;
        memcpy(img.data.data() + dst, raw.data() + ph[i].p_offset, ph[i].p_filesz);
    }

    // Locate tohost symbol in .symtab
    const auto* sh = reinterpret_cast<const Elf32_Shdr*>(raw.data() + eh->e_shoff);
    for (int i = 0; i < eh->e_shnum; ++i) {
        if (sh[i].sh_type != SHT_SYMTAB) continue;

        const auto* syms   = reinterpret_cast<const Elf32_Sym*>(raw.data() + sh[i].sh_offset);
        const char* strtab = reinterpret_cast<const char*>(raw.data() + sh[sh[i].sh_link].sh_offset);
        int nsyms = static_cast<int>(sh[i].sh_size / sizeof(Elf32_Sym));

        for (int j = 0; j < nsyms; ++j) {
            if (std::strcmp(strtab + syms[j].st_name, "tohost") == 0) {
                img.tohost_addr = syms[j].st_value;
                break;
            }
        }
        break;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Test runner
// ---------------------------------------------------------------------------

static constexpr uint32_t MAX_STEPS     = 10'000'000;
static constexpr uint32_t POLL_INTERVAL = 128;

static bool run_test(const std::string& path)
{
    TestImage img;
    std::string name = std::filesystem::path(path).stem().string();

    if (!parse_elf(path, img)) {
        std::cout << "[ERROR ] " << name << "  (ELF parse failed)\n";
        return false;
    }
    if (img.tohost_addr == 0) {
        std::cout << "[ERROR ] " << name << "  (tohost symbol not found)\n";
        return false;
    }

    CPU cpu(img.mem_base, img.mem_size);
    cpu.load(img.data, img.mem_base);

    std::string crash_reason;
    for (uint32_t n = 0; n < MAX_STEPS; ++n) {
        if ((n & (POLL_INTERVAL - 1)) == 0) {
            auto val = cpu.peekWord(img.tohost_addr);
            if (val && *val != 0) break;
        }
        try {
            cpu.step();
        } catch (const std::exception& e) {
            crash_reason = e.what();
            break;
        }
    }

    auto tohost = cpu.peekWord(img.tohost_addr);
    bool passed = (tohost && *tohost == 1);

    if (passed) {
        std::cout << "[PASS  ] " << name << "\n";
    } else {
      if (!crash_reason.empty())
      {
        std::cout << "         crash: " << crash_reason << "\n";
      }
      uint32_t code    = tohost.value_or(0);
      uint32_t testnum = code >> 1;
      std::cout << "[FAIL  ] " << name
                << "  (tohost=0x" << std::hex << code << std::dec
                << ", failing case " << testnum << ")\n";
    }
    return passed;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cerr << "Usage: compliance_runner <test.elf> [test2.elf ...]\n";
        return 1;
    }

    int passed = 0, failed = 0;
    for (int i = 1; i < argc; ++i) {
        if (run_test(argv[i])) ++passed; else ++failed;
    }

    std::cout << "\n" << (passed + failed) << " tests: "
              << passed << " passed, " << failed << " failed\n";
    return (failed == 0) ? 0 : 1;
}