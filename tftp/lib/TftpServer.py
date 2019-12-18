import lib.PacketManager as pm
import os
import socket
import threading
import time


class Watchdog(threading.Thread):
    def __init__(self, owner, server):
        threading.Thread.__init__(self)
        self.setDaemon(True)
        self.resetEvent = threading.Event()
        self.stopEvent = threading.Event()
        self.owner = owner
        self.server = server

    def run(self):
        timeCount = 0

        while True:

            if self.stopEvent.isSet():
                break

            if timeCount % 5 == 0 and 0 < timeCount < 25:
                self.server.remoteDict[self.owner].reSend()
                print('WATCH_DOG:Resend data.(%s:%s)' % (self.owner[0], self.owner[1]))

            elif timeCount >= 25:
                self.server.remoteDict[self.owner].clear('Session timeout. (%s:%s)' \
                                                         % (self.owner[0], self.owner[1]))
                break

            if self.resetEvent.isSet():
                timeCount = 0
                self.resetEvent.clear()

            time.sleep(1)
            timeCount += 1

    def countReset(self):
        self.resetEvent.set()

    def stop(self):
        self.stopEvent.set()


class PacketProcess:
    def __init__(self, remoteSocket, server):
        self.remoteSocket = remoteSocket
        self.endFrag = False
        self.server = server
        self.watchdog = Watchdog(self.remoteSocket, self.server)
        self._chunkSize = 512
        self.countBlock = 1

    def runProc(self, data):
        self.watchdog.countReset()
        Opcode = pm.unpack_opcode(data)

        # --------------------- RRQ -------------------------
        if Opcode == Opcode == pm.Opcodes.RRQ.value:

            filename = bytes.decode(data[2:].split(b'\x00')[0])
            filePath = os.path.join(self.server.serverDir, filename)
            print('Read request from:%s:%s, filename:%s' \
                  % (self.remoteSocket[0], self.remoteSocket[1], filename))

            if os.path.isfile(filePath):
                try:
                    self.sendFile = open(filePath, 'rb')
                except:
                    self.server.serverLocalSocket.sendto(pm.ErrorPacket.errFileOpen.value, self.remoteSocket)
                    self.clear('Can not read file. Session closed. (%s:%s)' \
                               % (self.remoteSocket[0], self.remoteSocket[1]))
                    return None

                dataChunk = self.sendFile.read(self._chunkSize)
                self.totalDatalen = len(dataChunk)

                self.sendPacket = pm.pack_data_chunk(dataChunk, self.countBlock)
                self.server.verbose_print("first packet = '%s'" % self.sendPacket)
                self.server.serverLocalSocket.sendto(self.sendPacket, self.remoteSocket)

                if len(dataChunk) < self._chunkSize:
                    self.endFrag = True

                self.watchdog.start()

            else:
                self.server.serverLocalSocket.sendto(pm.ErrorPacket.errNoFile.value, self.remoteSocket)
                self.clear('Requested file not found. Session closed. (%s:%s)' \
                           % (self.remoteSocket[0], self.remoteSocket[1]))

        # --------------------- WRQ ------------------------------
        elif Opcode == pm.Opcodes.WRQ.value:

            filename = bytes.decode(data[2:].split(b'\x00')[0])
            filePath = os.path.join(self.server.serverDir, filename)
            print('Write request from:%s:%s, filename:%s' \
                  % (self.remoteSocket[0], self.remoteSocket[1], filename))

            if os.path.isfile(filePath):
                self.server.serverLocalSocket.sendto(pm.ErrorPacket.errFileExists.value, self.remoteSocket)
                self.clear('File already exist. Session closed. (%s:%s)' \
                           % (self.remoteSocket[0], self.remoteSocket[1]))

            else:
                try:
                    self.rcvFile = open(filePath, 'wb')
                except:
                    self.server.serverLocalSocket.sendto(pm.ErrorPacket.errFileOpen.value, self.remoteSocket)
                    self.clear('Can not open file. Session closed. (%s:%s)' \
                               % (self.remoteSocket[0], self.remoteSocket[1]))
                    return None

                self.totalDatalen = 0

                self.sendPacket = pm.pack_ack(0)
                self.server.serverLocalSocket.sendto(self.sendPacket, self.remoteSocket)

                self.watchdog.start()

        # --------------------- DATA ---------------------------------
        elif Opcode == Opcode == pm.Opcodes.DATA.value:
            blockNo, dataPayload = pm.unpack_data_packet(data)
            self.totalDatalen += len(dataPayload)

            if blockNo == self.countBlock:
                try:
                    self.rcvFile.write(dataPayload)
                except:
                    self.server.serverLocalSocket.sendto(pm.ErrorPacket.errFileWrite.value, self.remoteSocket)
                    self.clear('Can not write data. Session closed. (%s:%s)' \
                               % (self.remoteSocket[0], self.remoteSocket[1]))
                    return None

                self.countBlock += 1
                if self.countBlock == 65536:
                    self.countBlock = 0

                self.sendPacket = pm.pack_ack(blockNo)
                self.server.serverLocalSocket.sendto(self.sendPacket, self.remoteSocket)

                self.watchdog.countReset()

                if len(dataPayload) < self._chunkSize:
                    self.clear('Data receive finish. %s bytes (%s:%s)' \
                               % (self.totalDatalen, self.remoteSocket[0],
                                  self.remoteSocket[1]))

            else:
                print('Receive wrong block. Resend data. (%d:%s:%s)'
                      % (blockNo, self.remoteSocket[0], self.remoteSocket[1]))

        # ----------------------- ACK -------------------------------
        elif Opcode == Opcode == pm.Opcodes.ACK.value:
            if self.endFrag:
                self.clear('Data send finish. %s bytes (%s:%s)' \
                           % (self.totalDatalen, self.remoteSocket[0],
                              self.remoteSocket[1]))

            else:
                blockNo = pm.unpack_blockno(data)
                if not hasattr(self, "countBlock"):
                    print("NO COUNTBLOCK, EXCUSE ME WHAT THE FUCK")
                    self.countBlock = 1
                if blockNo == self.countBlock:
                    try:
                        dataChunk = self.sendFile.read(self._chunkSize)
                    except:
                        dataChunk = ''

                    dataLen = len(dataChunk)
                    self.totalDatalen += dataLen
                    self.countBlock += 1
                    if self.countBlock == 65536:
                        self.countBlock = 0

                    self.sendPacket = pm.pack_data_chunk(dataChunk, self.countBlock)
                    self.server.serverLocalSocket.sendto(self.sendPacket, self.remoteSocket)

                    self.watchdog.countReset()

                    if dataLen < self._chunkSize:
                        self.endFrag = True

                else:
                    print('Receive wrong block. Resend data. (%d:%s:%s)'
                          % (blockNo, self.remoteSocket[0], self.remoteSocket[1]))

        # ----------------------- ERROR -------------------------------
        elif Opcode == Opcode == pm.Opcodes.ERROR.value:
            self.server.verbose_print("ERROR OPCODE")
            errCode, errString = pm.unpack_error(data)
            self.clear('Received error code %s:%s Session closed.(%s:%s)' \
                       % (str(errCode), errString, self.remoteSocket[0],
                          self.remoteSocket[1]))


        # ----------------------- Undefined -------------------------------
        else:
            self.server.serverLocalSocket.sendto(pm.ErrorPacket.errUnknown.value, self.remoteSocket)
            self.clear('Unknown error. Session closed.(%s:%s)' \
                       % (self.remoteSocket[0], self.remoteSocket[1]))

    def reSend(self):
        self.server.serverLocalSocket.sendto(self.sendPacket, self.remoteSocket)

    def clear(self, message):
        try:
            self.sendFile.close()
        except:
            pass
        try:
            self.rcvFile.close()
        except:
            pass

        del self.server.remoteDict[self.remoteSocket]
        self.watchdog.stop()
        print(message.strip())


class Server:
    def __init__(self, dPath, portno, verbose):
        self.serverDir = dPath
        self.serverLocalSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.portno = portno
        self.serverLocalSocket.bind(('', self.portno))
        self.remoteDict = {}
        self.verbose = verbose

    def verbose_print(self, text):
        if self.verbose:
            print("DEBUG:%s" % text)

    def run(self):
        print("server running on port = %s" % self.portno)
        self.verbose_print("TURNED ON")
        while True:

            data, remoteSocket = self.serverLocalSocket.recvfrom(4096)

            if remoteSocket in self.remoteDict:
                self.remoteDict[remoteSocket].runProc(data)
            else:
                self.remoteDict[remoteSocket] = PacketProcess(remoteSocket, self)
                self.remoteDict[remoteSocket].runProc(data)
