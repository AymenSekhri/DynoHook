#pragma once

#include <dynohook/nat_hook.h>

namespace dyno {
	class VHook final : public NatHook {
	public:
		VHook(uintptr_t fnAddress, const ConvFunc& convention);
		~VHook() override;

		bool hook() override;
		bool unhook() override;

		HookMode getMode() const override {
			return HookMode::VTableSwap;
		}

		const uintptr_t& getAddress() const override {
			return m_fnAddress;
		}

	private:
		// address of the original function
		uintptr_t m_fnAddress;
	};
}