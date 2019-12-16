from lib.TftpClient import Client
import sys


def get_command_from_cmd():
    while True:
        command = input("\nINPUT FORMAT 'GET/PUT FILE_NAME' or 'EXIT'\n")
        command_list = command.split(" ")
        if command_list[0] == "EXIT" or command_list[0] == "exit":
            exit(0)
        elif len(command_list) < 2 or len(command_list) > 3:
            print("Unexpected arguments number")
            continue
        elif command_list[0] == "GET" or command_list[0] == "get":
            if len(command_list) == 2:
                return 0, command_list[1], None
            else:
                return 0, command_list[1], command_list[2]

        elif command_list[0] == "PUT" or command_list[0] == "put":
            if len(command_list) == 2:
                return 1, command_list[1], None
            else:
                return 1, command_list[1], command_list[2]

        else:
            print("Unexpected command '%s'" % command_list[0])


def main():
    if len(sys.argv) != 3:
        raise Exception("Illegal amount of arguments: argc != 2")

    serverIp = sys.argv[1]
    serverPort = int(sys.argv[2])

    tftpClient = Client(serverIp, serverPort)
    while True:
        request, fileName, targetFileName = get_command_from_cmd()

        if request == 0:
            tftpClient.get(fileName, targetFileName)
        elif request == 1:
            tftpClient.get(fileName, targetFileName)


if __name__ == '__main__':
    main()