/* Copyright (C) 2016 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "sequence_generator.h"

#include "strings.h"
#include <gflags/gflags.h>

DEFINE_string(handshake, "lvp",
              "Handshake mode. One of lvp, nmclr-first, pgm-first. For "
              "low-voltage/single-supply-voltage programming, simply use lvp. If either nMCLR or "
              "PGM drives a high-voltage, nmclr-first or pgm-first can be used to determine which "
              "pin should raise first.");

Datastring PicSequenceGenerator::GenerateBitSequenceLsbUpDown(uint32_t data, int bits,
                                                              uint8_t base) const {
  Datastring result;
  for (int i = 0; i < bits; ++i) {
    bool bit_set = (data >> i) & 1;
    result.push_back(base | PGC | (bit_set ? PGD : 0));
    result.push_back(base | (bit_set ? PGD : 0));
  }
  return result;
}

Datastring PicSequenceGenerator::GenerateBitSequenceMsbUpDown(uint32_t data, int bits,
                                                              uint8_t base) const {
  Datastring result;
  for (int i = bits - 1; i >= 0; --i) {
    bool bit_set = (data >> i) & 1;
    result.push_back(base | PGC | (bit_set ? PGD : 0));
    result.push_back(base | (bit_set ? PGD : 0));
  }
  return result;
}

Datastring PicSequenceGenerator::GenerateBitSequenceLsbDownUp(uint32_t data, int bits,
                                                              uint8_t base) const {
  Datastring result;
  for (int i = 0; i < bits; ++i) {
    bool bit_set = (data >> i) & 1;
    result.push_back(base | (bit_set ? PGD : 0));
    result.push_back(base | PGC | (bit_set ? PGD : 0));
  }
  return result;
}

Datastring PicSequenceGenerator::GenerateBitSequenceMsbDownUp(uint32_t data, int bits,
                                                              uint8_t base) const {
  Datastring result;
  for (int i = bits - 1; i >= 0; --i) {
    bool bit_set = (data >> i) & 1;
    result.push_back(base | (bit_set ? PGD : 0));
    result.push_back(base | PGC | (bit_set ? PGD : 0));
  }
  return result;
}

std::vector<TimedStep> PicSequenceGenerator::GenerateInitSequence(bool down_up) const {
  std::vector<TimedStep> result;
  if (FLAGS_handshake == "nmclr-first") {
    result.push_back(TimedStep{{0}, MicroSeconds(100)});
    result.push_back(TimedStep{{nMCLR}, MicroSeconds(100)});
  } else if (FLAGS_handshake == "pgm-first") {
    result.push_back(TimedStep{{0}, MicroSeconds(100)});
    result.push_back(TimedStep{{PGM}, MicroSeconds(100)});
  } else {
    if (FLAGS_handshake != "lvp") {
      fprintf(stderr, "WARNING: handshake mode %s is unknown. Falling back to lvp mode.",
              FLAGS_handshake.c_str());
    }
    // This sequence combines the requirements for both the two and the three pin programming.
    // It inserts a little extra wait, but that is a small price to pay for the extra convenience
    // of having only a single sequence.
    result.push_back(TimedStep{{0, nMCLR, 0}, MilliSeconds(10)});
    {
      Datastring magic;
      if (down_up) {
        magic = GenerateBitSequenceMsbDownUp(0x4D434850, 32, 0);  // MCHP
      } else {
        magic = GenerateBitSequenceMsbUpDown(0x4D434850, 32, 0);  // MCHP
      }
      // Needs to be held for 40ns for the three-pin sequence, but for several microseconds for
      // the two-pin version.
      magic.push_back(PGM);
      result.push_back(TimedStep{magic, MicroSeconds(20)});
    }
    if (!down_up) {
      // Don't raise nMCLR for the non-down_up sequence.
      return result;
    }
  }
  result.push_back(TimedStep{{PGM | nMCLR}, MicroSeconds(400)});
  return result;
}

Datastring Pic18SequenceGenerator::GetCommandSequence(Pic18Command command,
                                                      uint16_t payload) const {
  Datastring result;
  result += GenerateBitSequenceLsbUpDown(static_cast<uint32_t>(command), 4);
  result += GenerateBitSequenceLsbUpDown(payload, 16);
  return result;
}

std::vector<TimedStep> Pic18SequenceGenerator::GetTimedSequence(
    TimedSequenceType type, const DeviceInfo *device_info) const {
  std::vector<TimedStep> result;
  constexpr int base = nMCLR | PGM;

  switch (type) {
    case INIT_SEQUENCE:
      result = GenerateInitSequence();
      break;
    case BULK_ERASE_SEQUENCE:
      result.push_back(
          TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC, base},
                    device_info ? device_info->bulk_erase_timing : MilliSeconds(500)});
      result.push_back(TimedStep{GenerateBitSequenceLsbUpDown(0, 16), ZeroDuration});
      break;
    case WRITE_SEQUENCE:
      result.push_back(TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC},
                                 device_info ? device_info->block_write_timing : MilliSeconds(1)});
      result.push_back(TimedStep{{base}, MicroSeconds(200)});
      result.push_back(TimedStep{GenerateBitSequenceLsbUpDown(0, 16), ZeroDuration});
      break;
    case WRITE_CONFIG_SEQUENCE:
      result.push_back(TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC},
                                 device_info ? device_info->config_write_timing : MilliSeconds(1)});
      result.push_back(TimedStep{{base}, MicroSeconds(200)});
      result.push_back(TimedStep{GenerateBitSequenceLsbUpDown(0, 16), ZeroDuration});
      break;
    default:
      FATAL("Requested unimplemented sequence %d\n", type);
  }
  return result;
}

//==================================================================================================

Datastring Pic16SequenceGenerator::GetCommandSequence(Pic16Command command,
                                                      uint16_t payload) const {
  Datastring result;
  result += GenerateBitSequenceLsbUpDown(static_cast<uint32_t>(command), 6);
  result += GenerateBitSequenceLsbUpDown(0, 1);
  result += GenerateBitSequenceLsbUpDown(payload, 14);
  result += GenerateBitSequenceLsbUpDown(0, 1);
  return result;
}

Datastring Pic16SequenceGenerator::GetCommandSequence(uint8_t command) const {
  Datastring result;
  result += GenerateBitSequenceLsbUpDown(command, 6);
  return result;
}

std::vector<TimedStep> Pic16SequenceGenerator::GetTimedSequence(
    TimedSequenceType type, const DeviceInfo *device_info) const {
  switch (type) {
    case INIT_SEQUENCE:
      return GenerateInitSequence();
    case CHIP_ERASE_SEQUENCE:
      return TimedSequenceFromDatastring16(device_info->chip_erase_sequence,
                                           device_info->bulk_erase_timing);
    case ERASE_DATA_SEQUENCE:
      return TimedSequenceFromDatastring16(device_info->eeprom_erase_sequence,
                                           device_info->bulk_erase_timing);
    case WRITE_DATA_SEQUENCE:
      return TimedSequenceFromDatastring16(device_info->block_write_sequence,
                                           device_info->block_write_timing);
    default:
      FATAL("Requested unimplemented sequence %d\n", type);
  }
}

Status Pic16SequenceGenerator::ValidateSequence(const Datastring16 &sequence) {
  for (auto iter = sequence.begin(); iter != sequence.end(); ++iter) {
    Pic16Command step = static_cast<Pic16Command>(*iter);
    if (step == Pic16Command::LOAD_PROG_MEMORY || step == Pic16Command::LOAD_DATA_MEMORY) {
      ++iter;
      if (iter == sequence.end()) {
        return Status(
            PARSE_ERROR,
            strings::Cat("Load command ", HexByte(static_cast<uint8_t>(step)), " missing data"));
      }
    } else if (*iter == 0xff) {
      // Sleep. Do nothing.
    } else if (*iter == 0xfe) {
      ++iter;
      if (iter == sequence.end()) {
        return Status(PARSE_ERROR, strings::Cat("Set address command missing address"));
      }
    } else if (*iter > 0x3f) {
      return Status(PARSE_ERROR, strings::Cat("Invalid command value ", HexUint16(*iter)));
    }
  }
  return Status::OK;
}

std::vector<TimedStep> Pic16SequenceGenerator::TimedSequenceFromDatastring16(
    const Datastring16 &sequence, Duration timing) const {
  std::vector<TimedStep> result;
  Datastring step_string;
  for (auto iter = sequence.begin(); iter != sequence.end(); ++iter) {
    Pic16Command step = static_cast<Pic16Command>(*iter);
    if (step == Pic16Command::LOAD_CONFIGURATION || step == Pic16Command::LOAD_PROG_MEMORY) {
      ++iter;
      step_string += GetCommandSequence(step, *iter);
    } else if (*iter == 0xff) {
      result.push_back(TimedStep{step_string, timing});
      step_string.clear();
    } else if (*iter == 0xfe) {
      ++iter;
      for (uint16_t i = 0; i <= *iter; ++i) {
        step_string += GetCommandSequence(static_cast<uint8_t>(Pic16Command::INCREMENT_ADDRESS));
      }
    } else {
      step_string += GetCommandSequence(*iter);
    }
  }
  if (!step_string.empty()) {
    result.push_back(TimedStep{step_string, MicroSeconds(0)});
  }
  return result;
}

//==================================================================================================

Datastring PicNew8BitSequenceGenerator::GetCommandSequence(PicNew8BitCommand command,
                                                           uint32_t payload) const {
  Datastring result;
  result += GenerateBitSequenceMsbUpDown(static_cast<uint32_t>(command), 8, PGM);
  result += GenerateBitSequenceMsbUpDown(0, 1, PGM);
  result += GenerateBitSequenceMsbUpDown(payload, 22, PGM);
  result += GenerateBitSequenceMsbUpDown(0, 1, PGM);
  return result;
}

Datastring PicNew8BitSequenceGenerator::GetCommandSequence(PicNew8BitCommand command) const {
  Datastring result;
  result += GenerateBitSequenceMsbUpDown(static_cast<uint32_t>(command), 8, PGM);
  return result;
}

std::vector<TimedStep> PicNew8BitSequenceGenerator::GetTimedSequence(
    TimedSequenceType type, const DeviceInfo *device_info) const {
  switch (type) {
    case INIT_SEQUENCE:
      return GenerateInitSequence(false);
    case CHIP_ERASE_SEQUENCE: {
      std::vector<TimedStep> result;
      result.push_back(
          TimedStep{GetCommandSequence(PicNew8BitCommand::LOAD_PC, device_info->config_address) +
                        GetCommandSequence(PicNew8BitCommand::BULK_ERASE),
                    device_info->bulk_erase_timing});
      if (device_info->eeprom_address != 0) {
        result.push_back(
            TimedStep{GetCommandSequence(PicNew8BitCommand::LOAD_PC, device_info->eeprom_address) +
                          GetCommandSequence(PicNew8BitCommand::BULK_ERASE),
                      device_info->bulk_erase_timing});
      }
      return result;
    }
    case WRITE_SEQUENCE:
      return {TimedStep{GetCommandSequence(PicNew8BitCommand::BEGIN_PROGRAMMING_INT_TIMED),
                        device_info->block_write_timing}};
    case CONFIG_WRITE_SEQUENCE:
      return {TimedStep{GetCommandSequence(PicNew8BitCommand::BEGIN_PROGRAMMING_INT_TIMED),
                        device_info->config_write_timing}};
    default:
      FATAL("Requested unimplemented sequence %d\n", type);
  }
}

//==================================================================================================

Datastring Pic24SequenceGenerator::GetWriteCommandSequence(uint32_t payload) const {
  Datastring result;
  result += GenerateBitSequenceLsbDownUp(0, 4);
  result += GenerateBitSequenceLsbDownUp(payload, 24);
  return result;
}

Datastring Pic24SequenceGenerator::GetReadCommandSequence() const {
  Datastring result;
  result += GenerateBitSequenceLsbDownUp(1, 4);
  result += GenerateBitSequenceLsbDownUp(0, 24);
  return result;
}

std::vector<TimedStep> Pic24SequenceGenerator::GetTimedSequence(TimedSequenceType type,
                                                                const DeviceInfo *) const {
  std::vector<TimedStep> result;
  switch (type) {
    case INIT_SEQUENCE:
      result.push_back(TimedStep{{0, nMCLR | PGM, PGM}, MilliSeconds(2)});
      {
        Datastring magic = GenerateBitSequenceMsbDownUp(0x4D434851, 32, PGM);  // MCHQ
        result.push_back(TimedStep{magic, MilliSeconds(2)});
      }
      result.push_back(TimedStep{{nMCLR | PGM}, MilliSeconds(27)});
      // First command should be a NOP (e.g. all zeros), but also requires 9 clocks instead of the
      // normal 4 clocks to clock in the command.
      result.push_back(
          {GenerateBitSequenceLsbDownUp(0, 9) + GenerateBitSequenceLsbDownUp(0, 24), ZeroDuration});
      break;
    default:
      FATAL("Requested unimplemented sequence %d\n", type);
  }
  return result;
}
