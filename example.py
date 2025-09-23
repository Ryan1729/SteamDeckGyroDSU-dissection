import multiprocessing as mp
import time

# TODO? Make configurable?
CLIENT_IP = "127.0.0.1"
CLIENT_PORT = 26760

INCOMING_MAGIC = b"DSUC"
OUTGOING_MAGIC = b"DSUS"
VERSION = 1001

class DSUServer:
    def __init__(self):
        def calc_crc(packet):
            crc=0xFFFF_FFFF;

            for i in range(len(packet)):
                crc ^= packet[i];
                i += 1
                for _ in range(8):
                    crc = (crc >> 1) ^ 0xedb88320 if crc & 1 else crc >> 1;

            return (~crc) & 0xFFFF_FFFF;

        def serve(child_conn):
            import socket
            import struct
            from enum import Enum

            sock = socket.socket(socket.AF_INET,  # Internet
                                 socket.SOCK_DGRAM) # UDP
            sock.bind(((CLIENT_IP, CLIENT_PORT)))
            for i in range(5):
                [acc, gyro] = child_conn.recv()
                ts = time.time_ns()
                print(acc[0], acc[1], acc[2])
                print(gyro[0], gyro[1], gyro[2])
                print("ts", ts)

                packed_data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
                print(f"Received message: {len(packed_data)} {packed_data} from {addr}")
                (magic, version, length) = struct.unpack('<4sHH', packed_data[0:8])
                print(f"{magic, version, length}")
                # ~ char magic[4]; // DSUS - server, DSUC - client
                # ~ uint16_t version; // 1001
                # ~ uint16_t length; // without header
                if magic != b"DSUC":
                    print(f"Unexpected magic value from {addr}: {magic}. Full data: {packed_data}")
                    child_conn.close()
                    return
                print(f"Received message: {(magic, version, length)} from {addr}")
                (_checksum, id, event_type) = struct.unpack('<III', packed_data[8:20])
                # ~ uint32_t crc32; // whole packet with this field = 0
                # ~ uint32_t id; // of packet source, constant among one run
                # ~ uint32_t eventType; // no part of the header where length is involved

                print(f"Received event: {hex(event_type)} from {addr}")

                class EventType(Enum):
                    VERSION = 0x100000
                    INFO = 0x100001
                    DATA = 0x100002


                match EventType(event_type):
                    case EventType.VERSION:
                        # Apparently doesn't happen
                        pass
                    case EventType.INFO:
                        (port_count,) = struct.unpack('<I', packed_data[20:24])

                        at = 24
                        for i in range(port_count):
                            (port_number,) = struct.unpack('<B', packed_data[at:at+1])

                            packet = (
                                b"DSUS",
                                VERSION,
                                16, # length from event_type on
                                0, # crc initial value
                                id,
                                event_type,
                                #
                                port_number,
                                2 if i == 0 else 0, # 0 - not connected, 1 - reserved, 2 - connected
                                2 if i == 0 else 0, # 0 - not applicable, 1 - no or partial gyro, 2 - full gyro, 3 - do not use
                                1, # 0 - not applicable, 1 - USB, 2 - bluetooth
                                0, # unused - 0
                                0, # unused - 0
                                0, # unused - 0
                                0, # 1 - connected, for info - 0
                            )

                            packet = packet[:3] + (
                                calc_crc(
                                    struct.pack(
                                        "<4sHHIIIBBBBIHBB",
                                        *packet
                                    )
                                )
                            ,) + packet[4:]

                            print("CRC: ", packet[3])

                            sock.sendto(
                                struct.pack(
                                    "<4sHHIIIBBBBIHBB",
                                    *packet
                                ),
                                addr
                            )

                            at += 1
                    case _:
                        print(f"Received unhandled event type {event_type}")


            child_conn.close()

        parent_conn, child_conn = mp.Pipe()
        self.process = mp.Process(target=serve, args=(child_conn,))
        self.process.start()

        self.parent_conn = parent_conn

    def update(self, acc, gyro):
        self.parent_conn.send([
            [acc[0], acc[1], acc[2]],
            [gyro[0], gyro[1], gyro[2]]
        ])

    def close(self):
        self.process.join()
        self.process.close()


def main():
    from math import sin, cos

    server = DSUServer()

    t = 0.0
    scale = 45.0
    g = 9.81

    try:
        while 1:
            server.update(
                [sin(t) * scale, cos(t) * scale, sin(t + 0.5) * scale],
                [sin(t) * g, cos(t) * g, 0.0]
            )

            t += 1.0 / 1024.0;
    except ConnectionResetError:
        print("Internal pipe was closed")
        pass
    except BrokenPipeError:
        # shutdown
        pass
    finally:
        server.close()

if __name__ == "__main__":
    main()
