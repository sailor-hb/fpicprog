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
#ifndef SEQUENCE_GENERATOR_H_
#define SEQUENCE_GENERATOR_H_

#include <string>
#include <vector>

#include "device_db.h"
#include "util.h"

struct TimedStep {
  Datastring data;
  Duration sleep;
};

typedef std::vector<TimedStep> TimedSequence;

class PicSequenceGenerator {
 public:
  virtual ~PicSequenceGenerator() = default;

 protected:
  std::vector<TimedStep> GenerateInitSequence(bool down_up = true) const;
  Datastring GenerateBitSequenceLsbUpDown(uint32_t data, int bits,
                                          uint8_t base = nMCLR | PGM) const;
  Datastring GenerateBitSequenceMsbUpDown(uint32_t data, int bits,
                                          uint8_t base = nMCLR | PGM) const;
  Datastring GenerateBitSequenceLsbDownUp(uint32_t data, int bits,
                                          uint8_t base = nMCLR | PGM) const;
  Datastring GenerateBitSequenceMsbDownUp(uint32_t data, int bits,
                                          uint8_t base = nMCLR | PGM) const;
};

class Pic18SequenceGenerator : public PicSequenceGenerator {
 public:
  enum TimedSequenceType {
    INIT_SEQUENCE,
    BULK_ERASE_SEQUENCE,
    WRITE_SEQUENCE,
    WRITE_CONFIG_SEQUENCE,
  };

  Datastring GetCommandSequence(Pic18Command command, uint16_t payload) const;
  std::vector<TimedStep> GetTimedSequence(TimedSequenceType type,
                                          const DeviceInfo *device_info) const;
};

class Pic16SequenceGenerator : public PicSequenceGenerator {
 public:
  enum TimedSequenceType {
    INIT_SEQUENCE,
    CHIP_ERASE_SEQUENCE,
    ERASE_DATA_SEQUENCE,
    WRITE_DATA_SEQUENCE,
  };

  Datastring GetCommandSequence(Pic16Command command, uint16_t payload) const;
  Datastring GetCommandSequence(uint8_t command) const;
  std::vector<TimedStep> GetTimedSequence(TimedSequenceType type,
                                          const DeviceInfo *device_info) const;

  static Status ValidateSequence(const Datastring16 &sequence);

 private:
  std::vector<TimedStep> TimedSequenceFromDatastring16(const Datastring16 &sequence,
                                                       Duration timing) const;
};

class PicNew8BitSequenceGenerator : public PicSequenceGenerator {
 public:
  enum TimedSequenceType {
    INIT_SEQUENCE,
    CHIP_ERASE_SEQUENCE,
    WRITE_SEQUENCE,
    CONFIG_WRITE_SEQUENCE,
  };

  Datastring GetCommandSequence(PicNew8BitCommand command, uint32_t payload) const;
  Datastring GetCommandSequence(PicNew8BitCommand command) const;
  std::vector<TimedStep> GetTimedSequence(TimedSequenceType type,
                                          const DeviceInfo *device_info) const;
};

class Pic24SequenceGenerator : public PicSequenceGenerator {
 public:
  enum TimedSequenceType {
    INIT_SEQUENCE,
  };

  // PIC24 has only two commands: SIX and REGOUT. SIX executes a command while REGOUT reads data.
  Datastring GetWriteCommandSequence(uint32_t payload) const;
  Datastring GetReadCommandSequence() const;
  std::vector<TimedStep> GetTimedSequence(TimedSequenceType type,
                                          const DeviceInfo *device_info) const;
};

#endif
