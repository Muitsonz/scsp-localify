#ifndef __TOOL_BUILD_CALLING_RELATIONS__
void tools::BuildCallingRelations() {}
#else

#define ZYDIS_STATIC_BUILD
#define ZYCORE_STATIC_BUILD

#include "stdinclude.hpp"
#include <Zydis/Zydis.h>
#include <cstdint>

struct MethodCallSummary {
	const MethodInfo* entryPoint;
	std::string entryName; // cached for stable sorting/output
	std::vector<const MethodInfo*> callees;
};

static const ZydisDecoder* GetDecoder() {
	static ZydisDecoder s_decoder{};
	static const bool inited = []() -> bool {
		return ZYAN_SUCCESS(ZydisDecoderInit(
			&s_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64));
		}();
	return inited ? &s_decoder : nullptr;
}

static uintptr_t GetGameAssemblyBase() {
	static auto baseAddr = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
	return baseAddr;
}

// @brief resolve a direct call operand to an absolute target address
// @return resolved absolute address (0 for indirect calls (register/memory))
static uintptr_t GetCallTarget(
	const ZydisDecodedInstruction& ix, const ZydisDecodedOperand* operands, uintptr_t runtimeAddress
)
{
	if (ix.meta.category != ZYDIS_CATEGORY_CALL)
		return false;
	if (ix.operand_count < 1)
		return false;

	const ZydisDecodedOperand& op = operands[0];

	if (op.type == ZYDIS_OPERAND_TYPE_IMMEDIATE) {
		if (op.imm.is_relative) {
			// RIP-relative: next-instruction address + signed displacement.
			// Unsigned wrap-around is defined in C++ and correct for x64.
			return runtimeAddress + ix.length
				+ static_cast<uintptr_t>(static_cast<intptr_t>(op.imm.value.s));
		}
		else {
			// Absolute immediate (e.g. far CALL).
			return static_cast<uintptr_t>(op.imm.value.u);
		}
	}

	// Indirect call (via register or memory) — cannot resolve statically.
	return false;
}

// the last entry in the table has no known end address and is skipped
void tools::BuildCallingRelations() {
	// output to files
	const char* outputFilePath = "calling-relations.txt";
	const auto outPath = g_localify_base / outputFilePath;
	std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
	if (!ofs.is_open()) {
		std::cout << "Failed to open file '" << outputFilePath << "' to write." << std::endl;
		return;
	}

	const std::vector<const MethodInfo*>& table = debug::GetManagedMethodTable();
	const ZydisDecoder* decoder = GetDecoder();
	if (!decoder)
		return;

	std::vector<MethodCallSummary> summaries;
	summaries.reserve(table.size());

	for (size_t i = 0; i + 1 < table.size(); ++i) {
		const MethodInfo* caller = table[i];
		const uintptr_t start = caller->methodPointer;
		const uintptr_t end = table[i + 1]->methodPointer;

		if (start >= end)
			continue;

		MethodCallSummary summary{ caller, debug::FormatMethodInfo(caller), {} };

		const auto* ip = reinterpret_cast<const uint8_t*>(start);
		const auto* limit = reinterpret_cast<const uint8_t*>(end);

		// Per-entry deduplication to keep the call set unique (by target RVA/address).
		std::unordered_set<uintptr_t> uniqueCallees;

		while (ip < limit) {
			ZydisDecodedInstruction ix{};
			ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT]{};

			const ZyanStatus status = ZydisDecoderDecodeFull(
				decoder, ip, static_cast<ZyanUSize>(limit - ip),
				&ix, operands);

			if (!ZYAN_SUCCESS(status))
				break;

			const uintptr_t runtimeAddr = reinterpret_cast<uintptr_t>(ip);
			auto targetAddr = GetCallTarget(ix, operands, runtimeAddr);
			if (targetAddr) {
				const MethodInfo* callee = debug::ResolveAddress(targetAddr);
				if (callee) {
					uintptr_t addrKey = callee->methodPointer;
					if (uniqueCallees.insert(addrKey).second) {
						summary.callees.emplace_back(callee);
					}
				}
			}

			ip += ix.length;
		}

		if (!summary.callees.empty())
			summaries.emplace_back(std::move(summary));
	}

	// Pre-compute per-type base RVA used to derive RRVA (relative RVA).
	const uintptr_t base = GetGameAssemblyBase();
	std::unordered_map<uintptr_t, uintptr_t> typeBaseRva;
	typeBaseRva.reserve(table.size());
	for (const MethodInfo* m : table) {
		uintptr_t klass = m->klass;
		uintptr_t rva = base ? (m->methodPointer - base) : m->methodPointer;
		auto it = typeBaseRva.find(klass);
		if (it == typeBaseRva.end() || rva < it->second) {
			typeBaseRva[klass] = rva; // as input list is already sorted
		}
	}

	// Sort summaries by full entry name for stable, readable diffs
	std::sort(summaries.begin(), summaries.end(),
		[](const MethodCallSummary& a, const MethodCallSummary& b) {
			return a.entryName < b.entryName;
		});

	for (const auto& summary : summaries) {
		const MethodInfo* entry = summary.entryPoint;
		const uintptr_t entryPtr = entry->methodPointer;
		const uintptr_t entryRva = base ? (entryPtr - base) : entryPtr;

		const auto baseIt = typeBaseRva.find(entry->klass);
		const uintptr_t typeBase = (baseIt != typeBaseRva.end()) ? baseIt->second : entryRva;

		const int64_t rrva = static_cast<int64_t>(entryRva) - static_cast<int64_t>(typeBase);

		// add assembly name
		const auto* entryKlass = reinterpret_cast<const Il2CppClass*>(entry->klass);
		const void* image = entryKlass ? entryKlass->image : nullptr;
		const char* assemblyName = (image && il2cpp_image_get_name)
			? il2cpp_image_get_name(image)
			: nullptr;

		ofs << "=== " << summary.entryName << " ===";
		if (assemblyName && *assemblyName) {
			ofs << " (" << assemblyName << ")";
		}
		ofs << "\n";
		ofs << "# RRVA = " << rrva << "\n";

		std::vector<std::string> calleeLines;
		calleeLines.reserve(summary.callees.size());
		for (const MethodInfo* callee : summary.callees) {
			calleeLines.emplace_back(debug::FormatMethodInfo(callee));
		}
		std::sort(calleeLines.begin(), calleeLines.end());

		for (const auto& line : calleeLines) {
			ofs << "- " << line << "\n";
		}

		ofs << "\n";
	}
}

#endif