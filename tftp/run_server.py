from lib.TftpServer import Server
import sys
import os


def main():
    if len(sys.argv) != 3:
        raise Exception("Illegal amount of arguments: argc != 2")

    localDir = os.path.abspath(sys.argv[1])
    localPort = int(sys.argv[2])

    server = Server(localDir, localPort, False)
    server.run()


if __name__ == '__main__':
    main()
