from enum import Enum
import struct


# -------------------- enums ---------------------------
class ErrorPacket(Enum):
    errNoFile = struct.pack(b'!2H15sB', 5, 1, b'File not found.', 0)
    errFileOpen = struct.pack(b'!2H18sB', 5, 2, b'Can not open file.', 0)
    errFileWrite = struct.pack(b'!2H19sB', 5, 2, b'Can not write file.', 0)
    errBlockNo = struct.pack(b'!2H20sB', 5, 5, b'Unknown transfer ID.', 0)
    errFileExists = struct.pack(b'!2H20sB', 5, 6, b'File already exists.', 0)
    errUnknown = struct.pack(b'!2H23sB', 5, 4, b'Illegal TFTP operation.', 0)


class Opcodes(Enum):
    RRQ = 1
    WRQ = 2
    DATA = 3
    ACK = 4
    ERROR = 5
    TIMEOUT = 6


# ------------------ packing ---------------------
def pack_rrq(fileName):
    format = '!H' + str(len(fileName)) + 'sB5sB'
    return struct.pack(format.encode(), 1, fileName.encode(), 0, b'octet', 0)


def pack_wrq(fileName):
    format = '!H' + str(len(fileName)) + 'sB5sB'
    return struct.pack(format.encode(), 2, fileName.encode(), 0, b'octet', 0)


def pack_data_chunk(dataChunk, blockNo):
    return struct.pack(b'!2H', 3, blockNo) + dataChunk


def pack_ack(blockNo):
    return struct.pack(b'!2H', 4, blockNo)


# ------------------ unpacking ---------------------
def unpack_data_packet(data):
    blockNo = struct.unpack('!H', data[2:4])[0]
    dataPayload = data[4:]
    return blockNo, dataPayload


def unpack_opcode(data):
    return struct.unpack('!H', data[0:2])[0]


def unpack_blockno(data):
    return struct.unpack('!H', data[2:4])[0]


def unpack_error(data):
    return struct.unpack('!H', data[2:4])[0], data[4:-1]
