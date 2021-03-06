/**
 * libnes
 * Copyright (C) 2015 David Jolly
 * ----------------------
 *
 * libnes is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libnes is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/nes.h"
#include "../include/nes_memory_type.h"

namespace NES {

	namespace COMP {

		_nes_memory *_nes_memory::m_instance = NULL;

		_nes_memory::_nes_memory(void) :
			m_initialized(false)
		{
			std::atexit(nes_memory::_delete);
		}

		_nes_memory::~_nes_memory(void)
		{

			if(m_initialized) {
				uninitialize();
			}
		}

		void 
		_nes_memory::_delete(void)
		{

			if(nes_memory::m_instance) {
				delete nes_memory::m_instance;
				nes_memory::m_instance = NULL;
			}
		}

		_nes_memory *
		_nes_memory::acquire(void)
		{

			if(!nes_memory::m_instance) {

				nes_memory::m_instance = new nes_memory;
				if(!nes_memory::m_instance) {
					THROW_NES_MEMORY_EXCEPTION(NES_MEMORY_EXCEPTION_ALLOCATED);
				}
			}

			return nes_memory::m_instance;
		}

		std::string 
		_nes_memory::address_as_string(
			__in nes_memory_t type,
			__in uint16_t address,
			__in_opt bool verbose
			)
		{
			nes_memory_block *blk = NULL;

			ATOMIC_CALL_RECUR(m_lock);

			switch(type) {
				case NES_MEM_MMU:
					blk = &m_mmu;
					break;
				case NES_MEM_PPU:
					blk = &m_ppu;
					break;
				case NES_MEM_PPU_OAM:
					blk = &m_ppu_oam;
					break;
				default:
					THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_TYPE,
						"type. %lu", type);
			}

			return nes_memory::address_as_string(*blk, address, verbose);
		}

		std::string 
		_nes_memory::address_as_string(
			__in const nes_memory_block &block,
			__in uint16_t address,
			__in_opt bool verbose
			)
		{
			std::stringstream result;
			uint8_t iter = BITS_PER_BYTE, value;

			if(address >= block.size()) {
				THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_ADDRESS,
					"addr. 0x%x (max. 0x%x)", address, block.size() - 1);
			}

			value = block.at(address);

			if(verbose) {
				result << "0x" << VALUE_AS_HEX(uint16_t, address) << ": ";
			}

			result << "0x" << VALUE_AS_HEX(uint8_t, block.at(address)) << " [0b";

			while(iter--) {
				result << ((value & 0x80) ? "1" : "0");
				value <<= 1;
			}

			result << "]";

			return result.str();
		}

		uint8_t &
		_nes_memory::at(
			__in nes_memory_t type,
			__in uint16_t address
			)
		{
			nes_memory_block *blk = NULL;

			ATOMIC_CALL_RECUR(m_lock);

			if(!m_initialized) {
				THROW_NES_MEMORY_EXCEPTION(NES_MEMORY_EXCEPTION_UNINITIALIZED);
			}

			switch(type) {
				case NES_MEM_MMU:
					blk = &m_mmu;
					break;
				case NES_MEM_PPU:
					blk = &m_ppu;
					break;
				case NES_MEM_PPU_OAM:
					blk = &m_ppu_oam;
					break;
				default:
					THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_TYPE,
						"type. %lu", type);
			}

			if(address >= blk->size()) {
				THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_ADDRESS,
					"addr. 0x%x (max. 0x%x)", address, NES_MMU_MAX);
			}

			return blk->at(address);
		}

		std::string 
		_nes_memory::block_as_string(
			__in const nes_memory_block &block,
			__in uint16_t address,
			__in uint16_t offset,
			__in_opt bool verbose
			)
		{
			uint16_t fill;
			uint32_t iter, iter_end;
			std::stringstream result;
			
			if(address >= block.size()) {
				THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_ADDRESS,
					"addr. {0x%x - 0x%x} (max. 0x%x)", address, address + offset, 
					block.size() - 1);
			}

			if((address + offset) >= block.size()) {
				offset = (block.size() - address);
			}

			result << "[Sz. " << (offset / BYTES_PER_KBYTE) << " KB (" 
				<< offset << " bytes)]";

			if(address % BLOCK_WIDTH) {

				fill = (address - (address % BLOCK_WIDTH));
				if(fill) {
					result << std::endl << "0x" << VALUE_AS_HEX(uint16_t, fill) 
						<< " |";

					for(iter = fill; iter < address; ++iter) {
						result << " --";
					}
				}
			}

			iter_end = (address + offset);

			for(iter = address; iter < iter_end; ++iter) {

				if(!(iter % BLOCK_WIDTH)) {
					result << std::endl << "0x" << VALUE_AS_HEX(uint16_t, iter) 
						<< " |";
				}

				result << " " << VALUE_AS_HEX(uint8_t, block.at(iter));
			}

			if(iter % BLOCK_WIDTH) {

				fill = (iter + (BLOCK_WIDTH - (iter % BLOCK_WIDTH)));
				if(fill) {

					for(; iter < fill; ++iter) {
						result << " --";
					}
				}
			}

			return result.str();
		}

		void 
		_nes_memory::clear(void)
		{
			ATOMIC_CALL_RECUR(m_lock);

			if(!m_initialized) {
				THROW_NES_MEMORY_EXCEPTION(NES_MEMORY_EXCEPTION_UNINITIALIZED);
			}

			clear(NES_MEM_MMU);
			clear(NES_MEM_PPU);
			clear(NES_MEM_PPU_OAM);
		}

		void 
		_nes_memory::clear(
			__in nes_memory_t type
			)
		{
			ATOMIC_CALL_RECUR(m_lock);

			if(!m_initialized) {
				THROW_NES_MEMORY_EXCEPTION(NES_MEMORY_EXCEPTION_UNINITIALIZED);
			}

			switch(type) {
				case NES_MEM_MMU:
					m_mmu.clear();
					m_mmu.resize(NES_MMU_MAX + 1, 0);
					break;
				case NES_MEM_PPU:
					m_ppu.clear();
					m_ppu.resize(NES_PPU_MAX + 1, 0);
					break;
				case NES_MEM_PPU_OAM:
					m_ppu_oam.clear();
					m_ppu_oam.resize(NES_PPU_OAM_MAX + 1, 0);
					break;
				default:
					THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_TYPE,
						"type. %lu", type);
			}
		}

		std::string 
		_nes_memory::flag_as_string(
			__in uint8_t flag,
			__in_opt bool verbose
			)
		{
			std::stringstream result;
			uint8_t iter = BITS_PER_BYTE, value = flag;

			result << "[";

			while(iter--) {
				result << ((value & 0x80) ? "1" : "0");
				value <<= 1;
			}

			result << "]";

			if(verbose) {
				result << " (" << VALUE_AS_HEX(uint8_t, flag) << ")";
			}

			return result.str();		
		}

		bool 
		_nes_memory::flag_check(
			__in nes_memory_t type,
			__in uint16_t address,
			__in uint8_t flag
			)
		{
			ATOMIC_CALL_RECUR(m_lock);
			return (at(type, address) & flag);
		}

		void 
		_nes_memory::flag_clear(
			__in nes_memory_t type,
			__in uint16_t address,
			__in uint8_t flag
			)
		{
			ATOMIC_CALL_RECUR(m_lock);
			at(type, address) &= ~flag;
		}

		void 
		_nes_memory::flag_set(
			__in nes_memory_t type,
			__in uint16_t address,
			__in uint8_t flag
			)
		{
			ATOMIC_CALL_RECUR(m_lock);
			at(type, address) |= flag;
		}

		void 
		_nes_memory::initialize(void)
		{
			ATOMIC_CALL_RECUR(m_lock);

			if(m_initialized) {
				THROW_NES_MEMORY_EXCEPTION(NES_MEMORY_EXCEPTION_INITIALIZED);
			}

			m_initialized = true;
			clear();
		}

		bool 
		_nes_memory::is_allocated(void)
		{
			return (nes_memory::m_instance != NULL);
		}

		bool 
		_nes_memory::is_initialized(void)
		{
			ATOMIC_CALL_RECUR(m_lock);
			return m_initialized;
		}

		uint16_t 
		_nes_memory::read(
			__in nes_memory_t type,
			__in uint16_t address,
			__in uint16_t offset,
			__out nes_memory_block &block
			)
		{			
			uint16_t result = offset;
			nes_memory_block *blk = NULL;
			nes_memory_block::iterator end;

			ATOMIC_CALL_RECUR(m_lock);

			if(!m_initialized) {
				THROW_NES_MEMORY_EXCEPTION(NES_MEMORY_EXCEPTION_UNINITIALIZED);
			}

			switch(type) {
				case NES_MEM_MMU:
					blk = &m_mmu;
					break;
				case NES_MEM_PPU:
					blk = &m_ppu;
					break;
				case NES_MEM_PPU_OAM:
					blk = &m_ppu_oam;
					break;
				default:
					THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_TYPE,
						"type. %lu", type);
			}

			if(address >= blk->size()) {
				THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_ADDRESS,
					"addr. 0x%x", address);
			}

			if((address + offset) >= blk->size()) {
				result = (blk->size() - address);
				end = blk->end();
			} else {
				end = (blk->begin() + (address + offset));
			}

			block.insert(block.begin(), blk->begin() + address, end);

			return result;
		}

		std::string 
		_nes_memory::to_string(
			__in nes_memory_t type,
			__in uint16_t address,
			__in uint16_t offset,
			__in_opt bool verbose
			)
		{
			std::stringstream result;
			nes_memory_block *blk = NULL;

			ATOMIC_CALL_RECUR(m_lock);

			switch(type) {
				case NES_MEM_MMU:
					blk = &m_mmu;
					break;
				case NES_MEM_PPU:
					blk = &m_ppu;
					break;
				case NES_MEM_PPU_OAM:
					blk = &m_ppu_oam;
					break;
				default:
					THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_TYPE,
						"type. %lu", type);
			}

			result << "<" << NES_MEMORY_HEADER << "> (" 
				<< (m_initialized ? INITIALIZED : UNINITIALIZED); 

			if(verbose) {
				result << ", ptr. 0x" << VALUE_AS_HEX(nes_memory_ptr, this);
			}

			result << ")";

			if(m_initialized) {
				result << std::endl 
					<< nes_memory::block_as_string(*blk, address, offset, 
						verbose);
			}

			return result.str();
		}

		void 
		_nes_memory::uninitialize(void)
		{
			ATOMIC_CALL_RECUR(m_lock);

			if(!m_initialized) {
				THROW_NES_MEMORY_EXCEPTION(NES_MEMORY_EXCEPTION_UNINITIALIZED);
			}

			m_mmu.clear();
			m_ppu.clear();
			m_ppu_oam.clear();
			m_initialized = false;
		}

		uint16_t 
		_nes_memory::write(
			__in nes_memory_t type,
			__in uint16_t address,
			__in const nes_memory_block &block
			)
		{
			nes_memory_block *blk = NULL;
			uint16_t iter = 0, result = block.size();

			ATOMIC_CALL_RECUR(m_lock);

			if(!m_initialized) {
				THROW_NES_MEMORY_EXCEPTION(NES_MEMORY_EXCEPTION_UNINITIALIZED);
			}

			switch(type) {
				case NES_MEM_MMU:
					blk = &m_mmu;
					break;
				case NES_MEM_PPU:
					blk = &m_ppu;
					break;
				case NES_MEM_PPU_OAM:
					blk = &m_ppu_oam;
					break;
				default:
					THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_TYPE,
						"type. %lu", type);
			}

			if(address >= blk->size()) {
				THROW_NES_MEMORY_EXCEPTION_MESSAGE(NES_MEMORY_EXCEPTION_INVALID_ADDRESS,
					"addr. 0x%x", address);
			}

			if((address + block.size()) >= blk->size()) {
				result = (blk->size() - address);
			}

			for(; iter < result; ++iter) {
				blk->at(address + iter) = block.at(iter);
			}

			return result;
		}
	}
}
