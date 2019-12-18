import lib.PacketManager as pm

import os
import socket
import sys


class Client:
    def __init__(self, serverIP, serverPort):
        self.serverIP = serverIP
        self.serverPort = serverPort

        self.clientSocket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.clientSocket.settimeout(5)
        self._chunkSize = 512

    # ----------------------------------- GET -----------------------------------------------
    def get(self, fileName, localFileName=None):
        if localFileName == None:
            self.localFilePath = os.path.join('.', fileName)
        else:
            self.localFilePath = os.path.join('.', localFileName)

        self.fileName = fileName

        if os.path.isfile(self.localFilePath):
            print(self.localFilePath.split("/")[-1] + ' already exists. Replacing.')
            os.remove(self.localFilePath)

        # --------------------- Send read request to server -------------------------
        #          2 bytes    string   1 byte     string   1 byte
        #          -----------------------------------------------
        #          |  01   |  Filename  |   0  |    Mode    |   0  |
        #          -----------------------------------------------

        self.sendPacket = pm.pack_rrq(self.fileName)
        self.clientSocket.sendto(self.sendPacket, (self.serverIP, self.serverPort))

        getFile = None
        try:
            getFile = open(self.localFilePath, 'wb')
        except:
            print(self.fileName + ' can not open.')
            pass

        currDataSize = 0
        countBlock = 1
        errCount = 0

        while True:
            opcode = pm.Opcodes.TIMEOUT.value
            data = None
            remoteSocket = None

            # try to get current packet, no more than 4
            while errCount < 4:
                try:
                    data, remoteSocket = self.clientSocket.recvfrom(4096)
                    opcode = pm.unpack_opcode(data)
                    errCount = 0
                    break
                except:
                    self.clientSocket.sendto(self.sendPacket, (self.serverIP, self.serverPort))
                    opcode = pm.Opcodes.TIMEOUT.value
                    errCount += 1

            # --------------------- Get new block of file from server -------------------------
            #          2 bytes    2 bytes       n bytes
            #          ---------------------------------
            #         | 03    |   Block #  |    Data    |
            #          ---------------------------------

            if opcode == pm.Opcodes.DATA.value:
                blockNo, dataPayload = pm.unpack_data_packet(data)

                if blockNo != countBlock:
                    self.clientSocket.sendto(pm.ErrorPacket.errBlockNo.value, remoteSocket)
                    print('Received wrong block. Continue')
                    continue

                countBlock += 1
                if countBlock == 65536:
                    countBlock = 1

                try:
                    getFile.write(dataPayload)
                except:
                    self.clientSocket.sendto(pm.ErrorPacket.errFileWrite.value, remoteSocket)
                    print('Can not write data. Session closed.')
                    getFile.close()
                    break

                currDataSize += len(dataPayload)
                sys.stdout.write('\rget %s :%s bytes.' \
                                 % (self.fileName, currDataSize))

                self.sendPacket = pm.pack_ack(blockNo)
                self.clientSocket.sendto(self.sendPacket, remoteSocket)

                if len(dataPayload) < self._chunkSize:
                    sys.stdout.write('\rget %s :%s bytes. finish.\n' \
                                     % (self.fileName, currDataSize))
                    getFile.close()
                    break

            # --------------------- Error processing -------------------------
            elif opcode == pm.Opcodes.ERROR.value:
                errCode, errString = pm.unpack_error(data)

                print('Received error code %s : %s' \
                      % (str(errCode), bytes.decode(errString)))
                getFile.close()
                os.remove(getFile.name)
                break

            elif opcode == pm.Opcodes.TIMEOUT.value:
                print('Timeout. Session closed.')
                try:
                    getFile.close()
                except:
                    break
            else:
                print('Unknown error. Session closed.')
                try:
                    getFile.close()
                except:
                    break

    # ----------------------------------- PUT -----------------------------------------------
    def put(self, fileName, targetFileName=None):
        self.localFilePath = os.path.join('.', fileName)

        if targetFileName == None:
            self.fileName = fileName
        else:
            self.fileName = targetFileName

        if not os.path.isfile(self.localFilePath):
            print(self.fileName + ' does not exist. Can not start.')
            pass

        # --------------------- Send write request to server -------------------------
        #          2 bytes    string   1 byte     string   1 byte
        #          -----------------------------------------------
        #          |  02   |  Filename  |   0  |    Mode    |   0  |
        #          -----------------------------------------------

        self.clientSocket.sendto(pm.pack_wrq(fileName), (self.serverIP, self.serverPort))

        putFile = None

        try:
            putFile = open(self.localFilePath, 'rb')
        except:
            print(self.localFilePath + ' can not open.')
            pass

        endFlag = False
        currDataSize = 0
        countBlock = 0

        while True:

            data, remoteSocket = self.clientSocket.recvfrom(4096)
            Opcode = pm.unpack_opcode(data)

            # --------------------- Send new block of file to server -------------------------
            #          2 bytes    2 bytes
            #          --------------------
            #          | 04    |   Block #  |
            #          --------------------

            if Opcode == pm.Opcodes.ACK:

                if endFlag == True:
                    putFile.close()
                    sys.stdout.write('\rput %s :%s bytes. finish.\n' \
                                     % (self.fileName, currDataSize))
                    break

                blockNo = pm.unpack_blockno(data)

                if blockNo != countBlock:
                    self.clientSocket.sendto(pm.ErrorPacket.errBlockNo.value, remoteSocket)
                    print('Receive wrong block. Session closed.')
                    putFile.close()
                    break

                blockNo += 1
                if blockNo == 65536:
                    blockNo = 0

                dataChunk = putFile.read(self._chunkSize)

                self.clientSocket.sendto(pm.pack_data_chunk(dataChunk, blockNo), remoteSocket)

                currDataSize += len(dataChunk)
                sys.stdout.write('\rput %s :%s bytes.' % (self.fileName, currDataSize))

                countBlock += 1
                if countBlock == 65536:
                    countBlock = 0

                if len(dataChunk) < self._chunkSize:
                    endFlag = True

            # --------------------- Error processing -------------------------
            elif Opcode == pm.Opcodes.ERROR:
                errCode, errString = pm.unpack_error(data)
                print('Receive error code %s : %s' \
                      % (str(errCode), bytes.decode(errString)))
                putFile.close()
                break
            else:
                self.clear('Unknown error. Session closed.')
                try:
                    putFile.close()
                except:
                    pass
                break
