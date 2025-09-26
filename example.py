from dataclasses import dataclass
import multiprocessing as mp
import time
from typing import Union

# TODO? Make configurable?
CLIENT_IP = "127.0.0.1"
CLIENT_PORT = 26760

INCOMING_MAGIC = b"DSUC"
OUTGOING_MAGIC = b"DSUS"
VERSION = 1001

@dataclass
class Client:
    addr: tuple[str, int]
    id: int

@dataclass
class AccAndGyro:
    acc: [float, float, float]
    gyro: [float, float, float]

class Stop:
    pass

ControllerDataMsg = Union[Client, AccAndGyro, Stop]


class DSUServer:
    def __init__(self):
        import socket
        import struct

        def calc_crc(packet):
            crc=0xFFFF_FFFF;

            for i in range(len(packet)):
                crc ^= packet[i];
                i += 1
                for _ in range(8):
                    crc = (crc >> 1) ^ 0xedb88320 if crc & 1 else crc >> 1;

            return (~crc) & 0xFFFF_FFFF;

        def serve(child_conn, client_conn):
            from enum import Enum

            sock = socket.socket(socket.AF_INET,  # Internet
                                 socket.SOCK_DGRAM) # UDP
            sock.bind(((CLIENT_IP, CLIENT_PORT)))
            while True:
                if child_conn.poll(0):
                    should_stop = child_conn.recv()
                    if should_stop:
                        break

                packed_data, addr = sock.recvfrom(1024) # buffer size is 1024 bytes
                print(f"Received message: {len(packed_data)} {packed_data} from {addr}")
                (magic, version, length) = struct.unpack('<4sHH', packed_data[0:8])

                # ~ char magic[4]; // DSUS - server, DSUC - client
                # ~ uint16_t version; // 1001
                # ~ uint16_t length; // without header
                if magic != b"DSUC":
                    print(f"Unexpected magic value from {addr}: {magic}. Full data: {packed_data}")
                    child_conn.close()
                    return

                (_checksum, id, event_type) = struct.unpack('<III', packed_data[8:20])
                # ~ uint32_t crc32; // whole packet with this field = 0
                # ~ uint32_t id; // of packet source, constant among one run
                # ~ uint32_t eventType; // no part of the header where length is involved

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
                            (slot_number,) = struct.unpack('<B', packed_data[at:at+1])

                            packet = (
                                b"DSUS",
                                VERSION,
                                16, # length from event_type on
                                0, # crc initial value
                                id,
                                event_type,
                                #
                                slot_number,
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

                            sock.sendto(
                                struct.pack(
                                    "<4sHHIIIBBBBIHBB",
                                    *packet
                                ),
                                addr
                            )

                            at += 1
                    case EventType.DATA:
                        print(f"EventType.DATA {self.clients} {(addr, id)}")
                        client = Client(addr, id)

                        client_conn.send(
                            client
                        )
                    case _:
                        print(f"Received unhandled event type {event_type}")


            child_conn.close()

        def controller_data(child_conn):
            # TODO send the controller data to all connected clients
            clients = []
            acc = [0.0, 0.0, 0.0]
            gyro = [0.0, 0.0, 0.0]

            sock = socket.socket(socket.AF_INET,  # Internet
                                 socket.SOCK_DGRAM) # UDP

            packet_number = 0
            timestamp_low = 0

            while True:
                if child_conn.poll(0):
                    msg = child_conn.recv()
                    if isinstance(msg, Stop):
                        break
                    elif isinstance(msg, Client):
                        if msg not in clients:
                            clients.append(msg)
                    else:
                        # We know isinstance(msg, AccAndGyro)
                        acc = msg.acc
                        gyro = msg.gyro

                for client in clients:
                    # TODO proper packet, not just the info packet
                    packet = (
                        b"DSUS",
                        VERSION,
                        84, # length from event_type on TODO
                        0, # crc initial value
                        client.id,
                        0x100002,
                        #
                        0,
                        2, # 0 - not connected, 1 - reserved, 2 - connected
                        2, # 0 - not applicable, 1 - no or partial gyro, 2 - full gyro, 3 - do not use
                        1, # 0 - not applicable, 1 - USB, 2 - bluetooth
                        0, # unused - 0
                        0, # unused - 0
                        0, # unused - 0
                        0, # 1 - connected, for info - 0
                        packet_number,
                        0,
                        0,
                        0,
                        0,

                        0,
                        0,
                        0,
                        0,

                        0,
                        0,
                        0,
                        0,

                        0,
                        0,
                        0,
                        0,

                        0,
                        0,
                        0,
                        0,

                        0,
                        0,
                        0,

                        timestamp_low,
                        0,

                        acc[0],
                        acc[1],
                        acc[2],

                        gyro[0],
                        gyro[1],
                        gyro[2],
                    )


                    packet = packet[:3] + (
                        calc_crc(
                            struct.pack(
                                "<4sHHIIIBBBBIHBBIBBBBBBBBBBBBBBBBBBBBIIIIIffffff",
                                *packet
                            )
                        )
                    ,) + packet[4:]

                    sock.sendto(
                        struct.pack(
                            "<4sHHIIIBBBBIHBBIBBBBBBBBBBBBBBBBBBBBIIIIIffffff",
                            *packet
                        ),
                        client.addr
                    )

                    packet_number += 1
                    packet_number &= 0xFFFF_FFFF

                    timestamp_low += 1

                #time.sleep(0.0001)

            child_conn.close()

        self.clients = []

        to_serve_conn, into_serve_conn = mp.Pipe()

        to_controller_data_conn, into_controller_data_conn = mp.Pipe()

        self.serve_process = mp.Process(target=serve, args=(into_serve_conn, to_controller_data_conn))
        self.serve_process.start()

        self.controller_data_process = mp.Process(target=controller_data, args=(into_controller_data_conn,))
        self.controller_data_process.start()

        self.to_serve_conn = to_serve_conn
        self.to_controller_data_conn = to_controller_data_conn


    def update(self, acc, gyro):
        self.to_controller_data_conn.send(AccAndGyro(
            [acc[0], acc[1], acc[2]],
            [gyro[0], gyro[1], gyro[2]],
        ))

    def close(self):
        # Tell thread to shut down
        self.to_serve_conn.send(True)
        self.serve_process.join()
        self.serve_process.close()

        # Tell thread to shut down
        self.to_controller_data_conn.send(Stop())
        self.controller_data_process.join()
        self.controller_data_process.close()


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
