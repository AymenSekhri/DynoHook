#pragma once

#include "instruction.h"
#include "mem_accessor.h"

extern "C" {
    typedef struct ZydisDecoder_  ZydisDecoder;
    typedef struct ZydisFormatter_  ZydisFormatter;
    typedef struct ZydisDecodedOperand_  ZydisDecodedOperand;
    typedef struct ZydisDecodedInstruction_ ZydisDecodedInstruction;
}

namespace dyno {
	typedef std::unordered_map<uint64_t, insts_t> branch_map_t;

	class ZydisDisassembler {
	public:
		explicit ZydisDisassembler(Mode mode);
		virtual ~ZydisDisassembler();

		std::vector<Instruction> disassemble(uint64_t firstInstruction, uint64_t start, uint64_t end, const MemAccessor& accessor);

		// TODO: Move to accessor
		static void writeEncoding(const insts_t& instructions, const MemAccessor& accessor) {
			for (const auto& inst : instructions)
				writeEncoding(inst, accessor);
		}

		/**Write the raw bytes of the given instruction into the memory specified by the
		* instruction's address. If the address value of the instruction has been changed
		* since the time it was decoded this will copy the instruction to a new memory address.
		* This will not automatically do any code relocation, all relocation logic should
		* first modify the byte array, and then call write encoding, proper order to relocate
		* an instruction should be disasm instructions -> set relative/absolute displacement() ->
		**/
		static void writeEncoding(const Instruction& instruction, const MemAccessor& accessor) {
			assert(instruction.size() <= instruction.getBytes().size());
			accessor.mem_copy(instruction.getAddress(), (uint64_t)&instruction.getBytes()[0], instruction.size());
		}

		static bool isConditionalJump(const Instruction& instruction) {
			// http://unixwiz.net/techtips/x86-jumps.html
			if (instruction.size() < 1)
				return false;

			std::vector<uint8_t> bytes = instruction.getBytes();
			if (bytes[0] == 0x0F && instruction.size() > 1) {
				if (bytes[1] >= 0x80 && bytes[1] <= 0x8F)
					return true;
			}

			if (bytes[0] >= 0x70 && bytes[0] <= 0x7F)
				return true;

			if (bytes[0] == 0xE3)
				return true;

			return false;
		}

		static bool isFuncEnd(const Instruction& instruction, const bool firstFunc = false) {
			// TODO: more?
			/*
			* 0xABABABAB : Used by Microsoft's HeapAlloc() to mark "no man's land" guard bytes after allocated heap memory
			* 0xABADCAFE : A startup to this value to initialize all free memory to catch errant pointers
			* 0xBAADF00D : Used by Microsoft's LocalAlloc(LMEM_FIXED) to mark uninitialised allocated heap memory
			* 0xBADCAB1E : Error Code returned to the Microsoft eVC debugger when connection is severed to the debugger
			* 0xBEEFCACE : Used by Microsoft .NET as a magic number in resource files
			* 0xCCCCCCCC : Used by Microsoft's C++ debugging runtime library to mark uninitialised stack memory
			* 0xCDCDCDCD : Used by Microsoft's C++ debugging runtime library to mark uninitialised heap memory
			* 0xDDDDDDDD : Used by Microsoft's C++ debugging heap to mark freed heap memory
			* 0xDEADDEAD : A Microsoft Windows STOP Error code used when the user manually initiates the crash.
			* 0xFDFDFDFD : Used by Microsoft's C++ debugging heap to mark "no man's land" guard bytes before and after allocated heap memory
			* 0xFEEEFEEE : Used by Microsoft's HeapFree() to mark freed heap memory
			*/
			auto& mnemonic = instruction.getMnemonic();
			auto bytes = instruction.getBytes();
			return (instruction.size() == 1 && bytes[0] == 0xCC) ||
				(instruction.size() >= 2 && bytes[0] == 0xf3 && bytes[1] == 0xc3) ||
				(mnemonic == "jmp" && !firstFunc) || // Jump to tranlslation
				mnemonic == "ret" || mnemonic.find("iret") == 0;
		}

		static bool isPadBytes(const Instruction& instruction) {
			// supports multi-byte nops
			return instruction.getMnemonic() == "nop";
		}

		const branch_map_t& getBranchMap() const {
			return m_branchMap;
		}

		void addToBranchMap(insts_t& insVec, const Instruction& inst) {
			if (inst.isBranching()) {
				// search back, check if new instruction points to older ones (one to one)
				auto destInst = std::find_if(insVec.begin(), insVec.end(), [&](const Instruction& oldIns) {
					return oldIns.getAddress() == inst.getDestination();
				});

				if (destInst != insVec.end()) {
					updateBranchMap(destInst->getAddress(), inst);
				}
			}

			// search forward, check if old instructions now point to new one (many to one possible)
			for (const Instruction& oldInst : insVec) {
				if (oldInst.isBranching() && oldInst.hasDisplacement() && oldInst.getDestination() == inst.getAddress()) {
					updateBranchMap(inst.getAddress(), oldInst);
				}
			}
		}

		inline Mode getMode() const {
			return m_mode;
		}
		
	protected:

		bool getOpStr(ZydisDecodedInstruction* pInstruction, const ZydisDecodedOperand* decoded_operands, uint64_t addr, std::string* pOpStrOut);

		void setDisplacementFields(Instruction& inst, const ZydisDecodedInstruction* zydisInst, const ZydisDecodedOperand* operands) const;

		typename branch_map_t::mapped_type& updateBranchMap(uint64_t key, const Instruction& new_val) {
			auto it = m_branchMap.find(key);
			if (it != m_branchMap.end()) {
				it->second.push_back(new_val);
			} else {
				branch_map_t::mapped_type s;
				s.push_back(new_val);
				m_branchMap.emplace(key, s);
				return m_branchMap.at(key);
			}
			return it->second;
		}

		// we use a void pointer here since we don't want forward declare the ZydisDecoder
		ZydisDecoder* m_decoder;
		ZydisFormatter* m_formatter;

		Mode m_mode;

		/* key = address of instruction pointed at (dest of jump). Value = set of unique instruction branching to dest
		   Must only hold entries from the last segment disassembled. I.E clear every new call to disassemble
		*/
		branch_map_t m_branchMap;
	};
}