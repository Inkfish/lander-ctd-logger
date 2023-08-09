#!/usr/bin/env python3
import argparse
import sys
import time

try:
    import serial
except ImportError:
    print('Missing pyserial:\n\n    pip install pyserial', file=sys.stderr)
    sys.exit(1)


parser = argparse.ArgumentParser()
parser.add_argument('--baud', '-b', type=int, default=9600)
parser.add_argument('--samplerate', '-r', type=int, default=16)
parser.add_argument('--sal', action='store_true')
parser.add_argument('--sv', action='store_true')
parser.add_argument('device')
args = parser.parse_args()


device = serial.Serial(args.device, args.baud, timeout=1)


def format_sample(temperature, conductivity, pressure,
                  salinity=None, sound_velocity=None):
    sample = f'{temperature:8.4f}, {conductivity:8.5f}, {pressure:8.3f}'
    if salinity is not None:
        sample += f', {salinity:8.4f}'
    if sound_velocity is not None:
        sample += f', {sound_velocity:8.3f}'
    sample += '\n'
    return sample.encode()


def parse_sample(sample):
    if not sample.endswith(b'\n') or sample.count(b'\n') != 1:
        return []

    output = []
    for val in sample.split(b','):
        try:
            output.append(float(val.strip()))
        except ValueError:
            output.append(float('nan'))
    return output


errors = 0
successes = 0

while True:
    for i in range(1, args.samplerate + 1):
        # Generate a sample from our fake CTD that simply uses the iteration
        # count for all of the values.
        device.write(format_sample(
            1*i, 2*i, 3*i,
            4*i if args.sal else None,
            5*i if args.sv else None
        ))

        time.sleep(1 / args.samplerate)

    # Receive, parse, and compare to expected value
    line = device.readline()
    parsed = parse_sample(line)
    expected = (args.samplerate + 1) / 2
    expected = [
        expected, 2*expected, 3*expected,
        (4*expected) if args.sal else -9999,
        (5*expected) if args.sv else -9999
    ]

    if len(expected) == 5 and \
        all(abs(p-e) < 0.01 for p,e in zip(parsed, expected)):
        successes += 1
    else:
        errors += 1
    
    if (successes + errors) % 10 == 0:
        print(f'{errors} errors in {successes + errors} trials',
              f'({100*successes/(successes+errors):.2f}% accurate)')
