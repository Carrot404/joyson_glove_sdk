#!/usr/bin/env python3
"""
Joyson Glove SDK - Network Monitor Tool
Monitor UDP traffic between SDK and glove hardware
"""

import socket
import struct
import sys
import time
from datetime import datetime

# Protocol constants
PACKET_HEADER = 0xEE
PACKET_TAIL = 0x7E
MODULE_MOTOR = 0x01
MODULE_ENCODER = 0x02
MODULE_IMU = 0x03

# Command names
MOTOR_COMMANDS = {
    0x00: "READ_STATUS",
    0x20: "READ_STATUS_ALT",
    0x30: "SET_MODE",
    0x31: "SET_POSITION",
    0x32: "SET_SPEED",
    0x33: "SET_FORCE"
}

MODULE_NAMES = {
    0x01: "MOTOR",
    0x02: "ENCODER",
    0x03: "IMU"
}

class PacketMonitor:
    def __init__(self, interface='any', port=8080):
        self.interface = interface
        self.port = port
        self.stats = {
            'total': 0,
            'motor': 0,
            'encoder': 0,
            'imu': 0,
            'invalid': 0
        }

    def parse_packet(self, data):
        """Parse UDP packet"""
        if len(data) < 9:
            return None

        try:
            header = data[0]
            if header != PACKET_HEADER:
                return None

            length = struct.unpack('<H', data[1:3])[0]
            if length != len(data):
                return None

            module_id = data[3]
            target = data[4]
            command = data[5]
            body = data[6:-3]
            checksum = struct.unpack('<H', data[-3:-1])[0]
            tail = data[-1]

            if tail != PACKET_TAIL:
                return None

            return {
                'header': header,
                'length': length,
                'module_id': module_id,
                'target': target,
                'command': command,
                'body': body,
                'checksum': checksum,
                'tail': tail
            }
        except Exception as e:
            return None

    def format_packet(self, packet):
        """Format packet for display"""
        if not packet:
            return "Invalid packet"

        module_name = MODULE_NAMES.get(packet['module_id'], f"UNKNOWN(0x{packet['module_id']:02X})")

        if packet['module_id'] == MODULE_MOTOR:
            cmd_name = MOTOR_COMMANDS.get(packet['command'], f"UNKNOWN(0x{packet['command']:02X})")
            return f"{module_name} | Motor {packet['target']} | {cmd_name} | Body: {len(packet['body'])} bytes"
        elif packet['module_id'] == MODULE_ENCODER:
            return f"{module_name} | Target {packet['target']} | Cmd 0x{packet['command']:02X} | Body: {len(packet['body'])} bytes"
        elif packet['module_id'] == MODULE_IMU:
            return f"{module_name} | Target {packet['target']} | Cmd 0x{packet['command']:02X} | Body: {len(packet['body'])} bytes"
        else:
            return f"Module 0x{packet['module_id']:02X} | Target {packet['target']} | Cmd 0x{packet['command']:02X}"

    def print_stats(self):
        """Print statistics"""
        print("\n" + "="*60)
        print("Statistics:")
        print(f"  Total packets: {self.stats['total']}")
        print(f"  Motor packets: {self.stats['motor']}")
        print(f"  Encoder packets: {self.stats['encoder']}")
        print(f"  IMU packets: {self.stats['imu']}")
        print(f"  Invalid packets: {self.stats['invalid']}")
        print("="*60)

    def monitor(self):
        """Monitor UDP traffic"""
        print(f"Monitoring UDP traffic on port {self.port}...")
        print("Press Ctrl+C to stop\n")

        # Create raw socket (requires root)
        try:
            sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW, socket.ntohs(0x0800))
        except PermissionError:
            print("Error: This tool requires root privileges")
            print("Please run with: sudo python3 monitor_network.py")
            sys.exit(1)

        try:
            while True:
                data, addr = sock.recvfrom(65535)

                # Parse Ethernet frame
                eth_header = data[:14]
                eth_protocol = struct.unpack('!H', eth_header[12:14])[0]

                # Check if IP packet
                if eth_protocol != 0x0800:
                    continue

                # Parse IP header
                ip_header = data[14:34]
                ip_protocol = ip_header[9]

                # Check if UDP
                if ip_protocol != 17:
                    continue

                # Parse UDP header
                udp_header = data[34:42]
                src_port, dst_port = struct.unpack('!HH', udp_header[0:4])

                # Check if our port
                if src_port != self.port and dst_port != self.port:
                    continue

                # Extract UDP payload
                udp_payload = data[42:]

                # Parse packet
                packet = self.parse_packet(udp_payload)

                # Update statistics
                self.stats['total'] += 1
                if packet:
                    if packet['module_id'] == MODULE_MOTOR:
                        self.stats['motor'] += 1
                    elif packet['module_id'] == MODULE_ENCODER:
                        self.stats['encoder'] += 1
                    elif packet['module_id'] == MODULE_IMU:
                        self.stats['imu'] += 1
                else:
                    self.stats['invalid'] += 1

                # Display packet
                timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                direction = "→" if dst_port == self.port else "←"
                packet_info = self.format_packet(packet) if packet else "Invalid packet"

                print(f"[{timestamp}] {direction} {packet_info}")

        except KeyboardInterrupt:
            print("\n\nMonitoring stopped")
            self.print_stats()

def main():
    if len(sys.argv) > 1:
        port = int(sys.argv[1])
    else:
        port = 8080

    monitor = PacketMonitor(port=port)
    monitor.monitor()

if __name__ == "__main__":
    main()
